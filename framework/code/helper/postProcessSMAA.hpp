//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "postProcess.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_support.hpp"
#include "system/glm_common.hpp"
#include <memory>

// Forward declarations
class Computable;
class MaterialManager;
class Shader;
class VulkanTexInfo;


class PostProcessSMAA : public PostProcess
{
    PostProcessSMAA& operator=(const PostProcessSMAA&) = delete;
    PostProcessSMAA(const PostProcessSMAA&) = delete;
public:
    PostProcessSMAA(Vulkan& vulkan);
    ~PostProcessSMAA();

    bool Init(const Shader& shader, MaterialManager& materialManager, VulkanTexInfo* diffuseRenderTarget, VulkanTexInfo* depthRenderTarget);
    bool UpdateUniforms(uint32_t WhichFrame, float ElapsedTime) override { return UpdateUniforms(WhichFrame, ElapsedTime, glm::mat4(1.0f)); }
    bool UpdateUniforms(uint32_t WhichFrame, float ElapsedTime, const glm::mat4& clipToPrevClip);
    void UpdateGui() override;

    const Computable* const GetComputable() const override;
    tcb::span<const VulkanTexInfo> GetOutput() const;

protected:
    Vulkan&                         m_Vulkan;

    std::unique_ptr<Computable>     m_Computable;
    std::array<VulkanTexInfo, 2>    m_historyDiffuse;

    struct UniformData {
        glm::mat4 ClipToPrevClip;
        glm::vec4 ScreenPosToHistoryBufferUV;        //scaleXY,offsetXY
        glm::vec4 InputSceneColorSize;               //width,height, inv width,inv height
        glm::vec4 HistoryBufferSize;                 //width,height, inv width,inv height
        glm::vec4 HistoryBufferUVMinMax;
        glm::vec4 OutputViewportSize;
        float     HistoryPreExposureCorrection = 1.0f;
        float     CurrentFrameWeight = 1.0f;
        int       bCameraCut;
        int       _Pad0;
        float     PlusWeights[5];
    } m_UniformData;
    UniformArrayT<UniformData, NUM_VULKAN_BUFFERS> m_Uniform;
};
