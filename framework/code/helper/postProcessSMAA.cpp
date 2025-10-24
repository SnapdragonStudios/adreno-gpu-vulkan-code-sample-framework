//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "postProcessSMAA.hpp"
#include "imgui/imgui.h"
#include "material/vulkan/computable.hpp"
#include "material/vulkan/materialManager.hpp"
#include "material/vulkan/shader.hpp"
#include "system/os_common.h"
#include "texture/vulkan/texture.hpp"
#include "texture/vulkan/textureManager.hpp"


PostProcessSMAA::PostProcessSMAA(Vulkan& vulkan) : m_Vulkan(vulkan)
{}

PostProcessSMAA::~PostProcessSMAA()
{
    m_Computable.reset();
    ReleaseTexture(m_Vulkan, &m_historyDiffuse[0]);
    ReleaseTexture(m_Vulkan, &m_historyDiffuse[1]);
    ReleaseUniformBuffer(&m_Vulkan, m_Uniform);
}

bool PostProcessSMAA::Init(const Shader<Vulkan>& shader, MaterialManager<Vulkan>& materialManager, TextureVulkan* diffuseRenderTarget, TextureVulkan* depthRenderTarget)
{
    assert(diffuseRenderTarget);
    assert(depthRenderTarget);

    //
    // Create the history texture (last frame color)
    m_historyDiffuse[0] = CreateTextureObject(m_Vulkan, diffuseRenderTarget->Width, diffuseRenderTarget->Height, TextureFormat::A2B10G10R10_UNORM_PACK32, TT_COMPUTE_TARGET, "History0");
    m_historyDiffuse[1] = CreateTextureObject(m_Vulkan, diffuseRenderTarget->Width, diffuseRenderTarget->Height, TextureFormat::A2B10G10R10_UNORM_PACK32, TT_COMPUTE_TARGET, "History1");

    //
    // Create the uniform buffer (control)
    if (!CreateUniformBuffer(&m_Vulkan, m_Uniform, &m_UniformData))
        return false;

    auto blitShaderMaterial = materialManager.CreateMaterial(shader, (uint32_t) m_historyDiffuse.size(),
        [&](const std::string& texName) -> const MaterialManager<Vulkan>::tPerFrameTexInfo {
            if (texName == "Diffuse") {
                return { diffuseRenderTarget };
            }
            else if (texName == "Depth") {
                return { depthRenderTarget };
            }
            else if (texName == "HistoryIn") {
                return { &m_historyDiffuse[1], &m_historyDiffuse[0] };
            }
            else if (texName == "OutputColor") {
                return { &m_historyDiffuse[0], &m_historyDiffuse[1] };
            }
            assert(0);
            return {};
        },
        [this](const std::string& bufferName) -> PerFrameBufferVulkan {
            return { m_Uniform.bufferHandles };
        }
        );

    auto computable = std::make_unique<Computable<Vulkan>>(m_Vulkan, std::move(blitShaderMaterial));
    if (!computable->Init())
    {
        return false;
    }
    computable->SetDispatchGroupCount(0, { diffuseRenderTarget->Width/16, diffuseRenderTarget->Height/4, 1});
    m_Computable = std::move(computable);
    return true;
}

bool PostProcessSMAA::UpdateUniforms(uint32_t WhichFrame, float ElapsedTime, const glm::mat4& clipToPrevClip)
{
    m_UniformData = {};

    m_UniformData.ScreenPosToHistoryBufferUV = { 0.50f, -0.50f, 0.50f, 0.50f }; //mjc { 1.0f/*scaleX*/,1.0f/*scaleY*/, 0.0f/*offsetX*/,0.0f/*offsetY*/ };
    m_UniformData.InputSceneColorSize = { m_historyDiffuse[0].Width, m_historyDiffuse[0].Height, 1.0f / m_historyDiffuse[0].Width, 1.0f / m_historyDiffuse[0].Height };
    m_UniformData.HistoryBufferSize = { m_historyDiffuse[0].Width, m_historyDiffuse[0].Height, 1.0f / m_historyDiffuse[0].Width, 1.0f / m_historyDiffuse[0].Height };
    m_UniformData.OutputViewportSize = { m_historyDiffuse[0].Width, m_historyDiffuse[0].Height, 1.0f / m_historyDiffuse[0].Width, 1.0f / m_historyDiffuse[0].Height };
    m_UniformData.HistoryPreExposureCorrection = 1.0f;
    m_UniformData.CurrentFrameWeight = 0.04f; //amount of ghosting vs jitter (0.04 default)
    m_UniformData.bCameraCut = 0;
    m_UniformData.PlusWeights[0] = 0.01659f;
    m_UniformData.PlusWeights[1] = 0.12913f;
    m_UniformData.PlusWeights[2] = 0.60175f;
    m_UniformData.PlusWeights[3] = 0.02876f;
    m_UniformData.PlusWeights[4] = 0.22377f;
    m_UniformData.ClipToPrevClip = clipToPrevClip;

    UpdateUniformBuffer(&m_Vulkan, m_Uniform, m_UniformData, WhichFrame);

    return true;
}

void PostProcessSMAA::UpdateGui()
{
}

const ComputableBase* const PostProcessSMAA::GetComputable() const
{
    return m_Computable.get();
}

std::span<const TextureVulkan> PostProcessSMAA::GetOutput() const
{
    return {m_historyDiffuse.data(), m_historyDiffuse.size()};
}
