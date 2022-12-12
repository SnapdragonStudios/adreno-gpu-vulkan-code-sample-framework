//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "animationGltfLoader.hpp"
#include "animationData.hpp"
#include "mesh/meshLoader.hpp"
#include "system/os_common.h"
#include <algorithm>

#ifdef GLM_FORCE_QUAT_DATA_WXYZ
#error "code currently expects glm::quat to be xyzw"
#endif

AnimationGltfProcessor::AnimationGltfProcessor() {}
AnimationGltfProcessor::~AnimationGltfProcessor() {}


bool AnimationGltfProcessor::operator()(const tinygltf::Model& ModelData)
{
    //const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];
    m_animations.reserve(ModelData.animations.size());

    for (const auto& animation : ModelData.animations)
    {
        // find all the channels with the same target node
        if (animation.channels.empty())
            continue;
        std::vector<int> targetNodeIdxs;
        targetNodeIdxs.push_back(animation.channels[0].target_node);

        for (size_t channelIdx = 1; channelIdx < animation.channels.size(); ++channelIdx)
        {
            int targetNodeIdx = animation.channels[channelIdx].target_node;
            if (std::find(targetNodeIdxs.begin(), targetNodeIdxs.end(), targetNodeIdx) == targetNodeIdxs.end())
            {
                targetNodeIdxs.push_back(targetNodeIdx);
            }
        }

        std::vector<AnimationNodeData> animationNodes;
        animationNodes.reserve(targetNodeIdxs.size());

        // Each target gets a set of frame data
        // Grab each channel for each unique target node (seperate channels for translation, rotation, scale)

        for (int targetNodeIdx : targetNodeIdxs)
        {
            const uint8_t* pTranslationChannelData = nullptr;
            const uint8_t* pRotationChannelData = nullptr; // rotations as quaternion but in gltf order (wxyz)
            const uint8_t* pScaleChannelData = nullptr;
            size_t translationDataItemCount = 0;
            size_t rotationDataItemCount = 0;
            size_t scaleDataItemCount = 0;
            size_t translationDataSrcItemStride = 0;
            size_t rotationDataSrcItemStride = 0;
            size_t scaleDataSrcItemStride = 0;
            const float* pTranslationTimeData = nullptr;
            const float* pRotationTimeData = nullptr;
            const float* pScaleTimeData = nullptr;

            const auto& targetNode = ModelData.nodes[targetNodeIdx];

            for (const auto& channel : animation.channels)
            {
                if (targetNodeIdx == channel.target_node)
                {
                    static const std::string sTranslationTargetPathId("translation");
                    static const std::string sRotationTargetPathId("rotation");
                    static const std::string sScaleTargetPathId("scale");

                    // Grab a pointer to the time data; currently we are strict with how this data is expected to be formatted in the gltf binary.
                    const tinygltf::Accessor& timeAccessorData = ModelData.accessors[animation.samplers[channel.sampler].input];
                    const size_t timeDataItemCount = timeAccessorData.count;
                    const auto& timeBuffer = ModelData.bufferViews[timeAccessorData.bufferView];

                    if ((timeBuffer.byteStride != 4 && timeBuffer.byteStride != 0/*packed/default*/) || (timeBuffer.byteOffset&3)!=0)
                    {
                        LOGE("Error reading time data for gltf animation \"%s\" (expecting contiguous array of aligned float data)", animation.name.c_str());
                        return false;
                    }
                    const float* timeDataSrcPtr = (const float*) &ModelData.buffers[timeBuffer.buffer].data[timeBuffer.byteOffset + timeAccessorData.byteOffset];

                    // Grab the relevant animation channel data (rotation, translation, or scale) pointers and strides etc.
                    const tinygltf::Accessor& dataAccessorData = ModelData.accessors[animation.samplers[channel.sampler].output];
                    const auto& dataBuffer = ModelData.bufferViews[dataAccessorData.bufferView];
                    size_t dataDstItemSize = 0;
                    const size_t dataItemCount = dataAccessorData.count;
                    const uint8_t* dataSrcPtr = &ModelData.buffers[dataBuffer.buffer].data[dataBuffer.byteOffset + dataAccessorData.byteOffset];

                    if (channel.target_path == sTranslationTargetPathId) {
                        pTranslationTimeData = timeDataSrcPtr;
                        pTranslationChannelData = dataSrcPtr;
                        dataDstItemSize = sizeof(glm::vec3);
                        translationDataSrcItemStride = dataBuffer.byteStride == 0 ? dataDstItemSize : dataBuffer.byteStride;
                        translationDataItemCount = dataItemCount;
                        assert(translationDataItemCount <= dataBuffer.byteLength / translationDataSrcItemStride);
                    }
                    else if (channel.target_path == sRotationTargetPathId) {
                        pRotationTimeData = timeDataSrcPtr;
                        pRotationChannelData = dataSrcPtr;
                        dataDstItemSize = sizeof(glm::vec4);
                        rotationDataSrcItemStride = dataBuffer.byteStride == 0 ? dataDstItemSize : dataBuffer.byteStride;
                        rotationDataItemCount = dataItemCount;
                        assert(rotationDataItemCount <= dataBuffer.byteLength / rotationDataSrcItemStride);
                    }
                    else if (channel.target_path == sScaleTargetPathId) {
                        pScaleTimeData = timeDataSrcPtr;
                        pScaleChannelData = dataSrcPtr;
                        dataDstItemSize = sizeof(glm::vec3);
                        scaleDataSrcItemStride = dataBuffer.byteStride == 0 ? dataDstItemSize : dataBuffer.byteStride;
                        scaleDataItemCount = dataItemCount;
                        assert(scaleDataItemCount <= dataBuffer.byteLength / scaleDataSrcItemStride);
                    }

                    // Last sanity check
                    if (timeDataItemCount != dataItemCount)
                    {
                        LOGE("Error reading channel data for gltf animation \"%s\".  Different number of items in time/input (%zu) and value/output buffers (%zu)", animation.name.c_str(), timeDataItemCount, dataItemCount);
                        return false;
                    }
                }
            }

            // Combine all the channel data into animation 'frames'

            std::vector< AnimationFrameData> frames;
            {
                // Grab all the time values and sort them.
                struct TimeAndIndex { float time; uint32_t channelMask; uint32_t index; glm::vec4 data; };
                std::vector<TimeAndIndex> timesSorted;
                timesSorted.reserve(translationDataItemCount + rotationDataItemCount + scaleDataItemCount);
                for (uint32_t i = 0; i < (uint32_t)translationDataItemCount; ++i)
                {
                    timesSorted.push_back({ pTranslationTimeData[i], 1, i, glm::vec4( *(const glm::vec3*)(pTranslationChannelData + translationDataSrcItemStride * i), 0.0f) });
                }
                for (uint32_t i = 0; i < (uint32_t)rotationDataItemCount; ++i)
                {
                    timesSorted.push_back({ pRotationTimeData[i], 2, i, *(const glm::vec4*)(pRotationChannelData + rotationDataSrcItemStride * i) });
                }
                for (uint32_t i = 0; i < (uint32_t)scaleDataItemCount; ++i)
                {
                    timesSorted.push_back({ pScaleTimeData[i], 4, i, glm::vec4( *(const glm::vec3*)(pScaleChannelData + scaleDataSrcItemStride*i), 0.0f) });
                }
                std::sort(timesSorted.begin(), timesSorted.end(), [](auto& a, auto& b) { return (a.time < b.time); });

                // Generate frame data for the known frames and merge identical timestamps
                std::vector< uint8_t > frameChannels;   // For each output frame records the mask of valid channels (so we can later fill in the missing channels)
                frames.reserve(timesSorted.size());
                frameChannels.reserve(timesSorted.size());

                AnimationFrameData frame{ };

                // Setup the frame with local node transform incase parts of it (eg translation) are not overwritten by the animation
                if (targetNode.translation.size() == 3)
                {
                    frame.Translation = glm::vec3(targetNode.translation[0], targetNode.translation[1], targetNode.translation[2]);
                }
                if (targetNode.rotation.size() == 4)
                {
                    frame.Rotation = glm::quat(/*xyzw in gltf*/ (float)targetNode.rotation[3], (float)targetNode.rotation[0], (float)targetNode.rotation[1], (float)targetNode.rotation[2]);
                }
                if (targetNode.scale.size() == 3)
                {
                    frame.Scale = glm::vec3(targetNode.scale[0], targetNode.scale[1], targetNode.scale[2]);
                }

                for (size_t i = 0, nexti = 0; nexti < timesSorted.size(); )
                {
                    if (timesSorted[i].time == timesSorted[nexti].time)
                    {
                        timesSorted[i].channelMask |= timesSorted[nexti].channelMask;
                        switch (timesSorted[nexti].channelMask) {
                        case 1:
                            frame.Translation = timesSorted[nexti].data;
                            break;
                        case 2:
                            frame.Rotation = glm::quat( timesSorted[nexti].data.w, timesSorted[nexti].data.x, timesSorted[nexti].data.y, timesSorted[nexti].data.z );
                            break;
                        case 4:
                            frame.Scale = timesSorted[nexti].data;
                            break;
                        default:
                            assert(0);
                            break;
                        }
                        ++nexti;
                    }
                    else
                    {
                        // We have a new timestamp, save out the just merged frame and move to the new one.
                        frame.Timestamp = timesSorted[i].time;
                        frames.push_back(frame);
                        frameChannels.push_back(timesSorted[i].channelMask);
                        i = nexti;
                    }
                }

                //
                // Fill in any 'missing' values
                glm::vec3 lastTranslation = frame.Translation;
                glm::vec3 nextTranslation = lastTranslation;
                float lastTranslationTime = 0.0f;
                float translationTimeSpanInv = 0.0f;

                glm::quat lastRotation = frame.Rotation;
                glm::quat nextRotation = lastRotation;
                float lastRotationTime = 0.0f;
                float rotationTimeSpanInv = 0.0f;

                glm::vec3 lastScale = frame.Scale;
                glm::vec3 nextScale = lastScale;
                float lastScaleTime = 0.0f;
                float scaleTimeSpanInv = 0.0f;

                const float animationTotalTime = timesSorted.back().time;

                for (size_t i = 0; i < frames.size(); ++i)
                {
                    // Translation
                    if ((frameChannels[i] & 1) == 0)
                    {
                        // Generate this timestep's translation by linear interpolation
                        frames[i].Translation = glm::mix(lastTranslation, nextTranslation, (frames[i].Timestamp - lastTranslationTime) * translationTimeSpanInv);
                    }
                    else
                    {
                        // Frame has a translation value, save it for interpolating missing values.
                        lastTranslation = frames[i].Translation;
                        lastTranslationTime = frames[i].Timestamp;
                        nextTranslation = lastTranslation;
                        translationTimeSpanInv = 0.0f;
                        // See if there is a future keyframe that has a translation, search wraps around
                        size_t j = 1, idx = i + 1;
                        for (; j < frames.size(); ++j, ++idx)
                        {
                            idx = (idx < frames.size()) ? idx : 0;
                            if (frameChannels[idx] & 1)
                            {
                                nextTranslation = frames[idx].Translation;
                                float translationTimeSpan = frames[idx].Timestamp - frames[i].Timestamp;
                                if (translationTimeSpan < 0.0f)
                                    translationTimeSpan += animationTotalTime;
                                translationTimeSpanInv = 1.0f / translationTimeSpan;
                                break;
                            }
                        }
                    }

                    // rotation
                    if ((frameChannels[i] & 2) == 0)
                    {
                        // Generate this timestep's rotation by linear interpolation
                        frames[i].Rotation = glm::slerp(lastRotation, nextRotation, (frames[i].Timestamp - lastRotationTime) * rotationTimeSpanInv);
                    }
                    else
                    {
                        // Frame has a rotation value, save it for interpolating missing values.
                        lastRotation = frames[i].Rotation;
                        lastRotationTime = frames[i].Timestamp;
                        nextRotation = lastRotation;
                        rotationTimeSpanInv = 0.0f;
                        // See if there is a future keyframe that has a rotation, search wraps around
                        size_t j = 1, idx = i + 1;
                        for (; j < frames.size(); ++j, ++idx)
                        {
                            if (idx >= frames.size())
                            {
                                idx = 0;
                            }
                            if (frameChannels[idx] & 2)
                            {
                                nextRotation = frames[idx].Rotation;
                                float rotationTimeSpan = frames[idx].Timestamp - frames[i].Timestamp;
                                if (rotationTimeSpan < 0.0f)
                                    rotationTimeSpan += animationTotalTime;
                                rotationTimeSpanInv = 1.0f / rotationTimeSpan;
                                break;
                            }
                        }
                    }
                    // Scale
                    if ((frameChannels[i] & 4) == 0)
                    {
                        // Generate this timestep's scale by linear interpolation
                        frames[i].Scale = glm::mix(lastScale, nextScale, (frames[i].Timestamp - lastScaleTime) * scaleTimeSpanInv);
                    }
                    else
                    {
                        // Frame has a scale value, save it for interpolating missing values.
                        lastScale = frames[i].Scale;
                        lastScaleTime = frames[i].Timestamp;
                        nextScale = lastScale;
                        scaleTimeSpanInv = 0.0f;
                        // See if there is a future keyframe that has a scale, search wraps around
                        size_t j = 1, idx = i + 1;
                        for (; j < frames.size(); ++j, ++idx)
                        {
                            idx = (idx < frames.size()) ? idx : 0;
                            if (frameChannels[idx] & 2)
                            {
                                nextScale = frames[idx].Scale;
                                float scaleTimeSpan = frames[idx].Timestamp - frames[i].Timestamp;
                                if (scaleTimeSpan < 0.0f)
                                    scaleTimeSpan += animationTotalTime;
                                scaleTimeSpanInv = 1.0f / scaleTimeSpan;
                                break;
                            }
                        }
                    }
                }
            }

            animationNodes.emplace_back( AnimationNodeData { std::move( frames ), (uint32_t) targetNodeIdx } );
        }

        m_animations.emplace_back(AnimationData{ animation.name, std::move(animationNodes) });
    }

    return true;
}
