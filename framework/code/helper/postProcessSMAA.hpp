//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "postProcess.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_support.hpp"
#include "memory/vulkan/uniform.hpp"
#include "system/glm_common.hpp"
#include "texture/vulkan/texture.hpp"
#include <memory>

// Forward declarations
class Computable;
class Shader;
template<typename T_GFXAPI> class MaterialManagerT;


class PostProcessSMAA : public PostProcess
{
    PostProcessSMAA& operator=(const PostProcessSMAA&) = delete;
    PostProcessSMAA(const PostProcessSMAA&) = delete;
public:
    PostProcessSMAA(Vulkan& vulkan);
    ~PostProcessSMAA();

    bool Init(const Shader& shader, MaterialManagerT<Vulkan>& materialManager, TextureVulkan* diffuseRenderTarget, TextureVulkan* depthRenderTarget);
    bool UpdateUniforms(uint32_t WhichFrame, float ElapsedTime) override { return UpdateUniforms(WhichFrame, ElapsedTime, glm::mat4(1.0f)); }
    bool UpdateUniforms(uint32_t WhichFrame, float ElapsedTime, const glm::mat4& clipToPrevClip);
    void UpdateGui() override;

    const Computable* const GetComputable() const override;
    std::span<const TextureVulkan> GetOutput() const;

protected:
    Vulkan&                         m_Vulkan;

    std::unique_ptr<Computable>     m_Computable;
    std::array<TextureVulkan, 2>    m_historyDiffuse;

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
    UniformArrayT<Vulkan, UniformData, NUM_VULKAN_BUFFERS> m_Uniform;
};
