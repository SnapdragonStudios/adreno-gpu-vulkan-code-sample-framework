//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "animation.hpp"
#include "skeleton.hpp"
#include "skeletonData.hpp"
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include "glm\gtx\quaternion.hpp"

static const glm::quat cRotateToYUp{ 0.7071067094802856f, -0.7071068286895752f, 0.f, 0.f };
static const glm::quat cIdentityRotate = glm::identity<glm::quat>();
//static const glm::quat cIdentityRotate = glm::quat(0.0f, 0.0f,0.0f,1.0f);

Animation::Animation(AnimationData&& d)
    : m_AnimationData(std::move(d))
{
    // Find the 'biggest' timestamp value in the animation to determine the enimation length
    for (const auto& animNode : m_AnimationData.GetNodes())
        if (!animNode.Frames.empty() && animNode.Frames.back().Timestamp > m_EndTime)
            m_EndTime = animNode.Frames.back().Timestamp;
}

Animation::~Animation()
{
}

static float CalcFrameMix(const std::vector<AnimationFrameData>& frames, float time, float loopTime, uint32_t& startFrameIdx, uint32_t& nextFrameIdx)
{
    const AnimationFrameData* pStartFrame = &frames[(startFrameIdx < frames.size()) ? startFrameIdx:0];
    if (time > pStartFrame->Timestamp)
    {
        while (pStartFrame <= &frames.back() && (time > pStartFrame->Timestamp))
            ++pStartFrame;
        pStartFrame--;
    }
    else
    {
        while (pStartFrame != &frames.front() && (time < pStartFrame->Timestamp))
            --pStartFrame;
    }
    startFrameIdx = (uint32_t)(pStartFrame - &frames.front());
    nextFrameIdx = startFrameIdx + 1;

    float frameDt = 0.0f;
    // Fix cases where we are straddling the 'loop'
    if (nextFrameIdx >= frames.size())
    {
        nextFrameIdx = 0;
        frameDt = loopTime;
    }

    const auto& nextFrame = frames[nextFrameIdx];
    frameDt += nextFrame.Timestamp - pStartFrame->Timestamp;
    if (frameDt <= 0.0001f)
        return 0.0f;
    return (time - pStartFrame->Timestamp) / frameDt;
}

glm::vec3 Animation::CalcLocalTranslation(uint32_t animationNodeIdx, float time, uint32_t& startFrameIdx) const
{
    const auto& frames = m_AnimationData.GetNodes()[animationNodeIdx].Frames;
    if (frames.empty())
        return { 0.0f,0.0f,0.0f };

    uint32_t nextFrameIdx;
    float mix = CalcFrameMix(frames, time, m_EndTime, startFrameIdx, nextFrameIdx);

    return glm::mix(frames[startFrameIdx].Translation, frames[nextFrameIdx].Translation, mix );
}

glm::quat Animation::CalcLocalRotation(uint32_t animationNodeIdx, float time, uint32_t& startFrameIdx) const
{
    const AnimationNodeData& nodeData = m_AnimationData.GetNodes()[animationNodeIdx];

    const auto& frames = nodeData.Frames;
    if (frames.empty())
        return cIdentityRotate;

    uint32_t nextFrameIdx;
    float mix = CalcFrameMix(frames, time, m_EndTime, startFrameIdx, nextFrameIdx);
    return glm::slerp(frames[startFrameIdx].Rotation, frames[nextFrameIdx].Rotation, mix);
}

glm::vec3 Animation::CalcLocalScale(uint32_t animationNodeIdx, float time, uint32_t& startFrameIdx) const
{
    const auto& frames = m_AnimationData.GetNodes()[animationNodeIdx].Frames;
    if (frames.empty())
        return { 1.0f,1.0f,1.0f };

    uint32_t nextFrameIdx;
    float mix = CalcFrameMix(frames, time, m_EndTime, startFrameIdx, nextFrameIdx);
    
    return glm::mix(frames[startFrameIdx].Scale, frames[nextFrameIdx].Scale, mix);
}


AnimationList::AnimationList()
{}


AnimationList& AnimationList::operator=(AnimationList&& other) noexcept
{
    m_Animations = std::move(other.m_Animations);
    m_AnimationNodeMap = std::move(other.m_AnimationNodeMap);
    return *this;
}


AnimationList::AnimationList(std::vector<AnimationData>&& animationDatas)
{
    m_Animations.reserve(animationDatas.size());
    m_AnimationNodeMap.reserve(animationDatas.size());

    // Get all the nodes affected by each animation
    for (size_t animIndex=0;animIndex<animationDatas.size();++animIndex)
    {
        const auto& animationNodes = animationDatas[animIndex].GetNodes();
        for (uint32_t animNodeIndex = 0; animNodeIndex < (uint32_t) animationNodes.size(); ++animNodeIndex)
        {
            const auto& animationNode = animationNodes[animNodeIndex];
            m_AnimationNodeMap.insert({ animationNode.NodeId, {m_Animations.data() + animIndex, animNodeIndex } });
        }
        m_Animations.emplace_back( std::move( animationDatas[animIndex] ) );
    }
}

AnimationList::~AnimationList()
{}

AnimationNodeRef AnimationList::FindNodeAnimation(int NodeId) const
{
    auto it = m_AnimationNodeMap.find(NodeId);
    if (it != m_AnimationNodeMap.end())
        return {it->second};
    else
        return {};
}

float AnimationList::StepTime(const AnimationNodeRef& anim, float currentTime, float elapsedTime) const
{
    currentTime += elapsedTime;
    return fmod(currentTime, anim.animation->GetEndTime());
}

void AnimationList::StepTime(AnimationIterator& iter, float elapsedTime) const
{
    float newTime = iter.time + elapsedTime;
    iter.time = fmod(newTime, iter.animation.GetEndTime());
}

/// @brief Create an allocation iterator
/// @param Animation we want to iterate on
AnimationIterator AnimationList::MakeIterator(const Animation& animation)
{
    AnimationIterator iter{ animation };
    const auto& animationDataNodes = animation.GetAnimationData().GetNodes();
    iter.nodeIterators.resize(animationDataNodes.size(), { 0 });
    return iter;
}

// Recalculate matrix array (one per nodeId)
void AnimationList::UpdateSkeletonMatrixes(const Skeleton& skeleton, AnimationIterator& iterator, tcb::span<glm::mat3x4> nodeMatrixs)
{
    const Animation& animation = iterator.animation;
    float time = iterator.time;

    const auto& animationDataNodes = animation.GetAnimationData().GetNodes();
    assert(animationDataNodes.size() == iterator.nodeIterators.size());

    for (uint32_t animationNodeIndex =0; animationNodeIndex<(uint32_t)iterator.nodeIterators.size(); ++animationNodeIndex)
    {
        const auto& nodeId = animationDataNodes[animationNodeIndex].NodeId; // gltf/nodeMatrixs id/index

        glm::mat4 Matrix = glm::translate(animation.CalcLocalTranslation(animationNodeIndex, time, iterator.nodeIterators[animationNodeIndex].frameIdx ));
        Matrix = Matrix * glm::toMat4(animation.CalcLocalRotation(animationNodeIndex, time, iterator.nodeIterators[animationNodeIndex].frameIdx));
        Matrix = Matrix * glm::scale(animation.CalcLocalScale(animationNodeIndex, time, iterator.nodeIterators[animationNodeIndex].frameIdx));

        nodeMatrixs[nodeId] = glm::transpose(Matrix);

        const SkeletonNodeData* skeletonNodeData = skeleton.GetSkeletonData().GetNodeById(nodeId);
        const SkeletonNodeData* skeletonNodeDataParent = skeletonNodeData->Parent();
        if (skeletonNodeDataParent)
        {
            nodeMatrixs[nodeId] = Matrix * nodeMatrixs[skeletonNodeDataParent->NodeId()];
        }
        for (const auto& child : skeletonNodeData->Children())
        {
            auto childWorld =  Matrix * child.LocalTransform();
            nodeMatrixs[child.NodeId()] = glm::transpose(childWorld);
        }
    }
}
