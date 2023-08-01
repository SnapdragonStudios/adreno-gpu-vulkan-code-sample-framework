//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shadowVsm.hpp"
#include "shadow.hpp"
#include "shadowVulkan.hpp"
#include "material/computable.hpp"
#include "material/materialManager.hpp"
#include "material/shaderManager.hpp"
#include "vulkan/commandBuffer.hpp"
#include <cassert>

ShadowVSM::ShadowVSM()
{
}

bool ShadowVSM::Initialize(GraphicsApiBase& gfxApi, const ShaderManager& shaderManager, const MaterialManager& materialManager, const Shadow& shadow )
{
    // Variant Shadow Map Computable
    // Computables

    const auto* pComputeShader = shaderManager.GetShader("VarianceShadowMap");
    if (!pComputeShader)
    {
        assert(pComputeShader);
        return false;
    }

    const auto& shadowVulkan = static_cast<const ShadowT<Vulkan>&>(shadow);
    auto& vulkan = static_cast<Vulkan&>(gfxApi);

    uint32_t width, height;
    shadowVulkan.GetTargetSize(width, height);

    // Create the render target
    auto vsmTarget = CreateTextureObject(vulkan, width, height, TextureFormat::R16G16_SFLOAT, TEXTURE_TYPE::TT_COMPUTE_TARGET, "VSM");
    //// Create the intermediate render target (result of 1st pass), width is half of output width (xy and zw channels hold alternating columns)
    auto vsmTargetIntermediate = CreateTextureObject(vulkan, width / 2, height, TextureFormat::R16G16B16A16_SFLOAT, TEXTURE_TYPE::TT_COMPUTE_TARGET, "VSM Intermediate");

    // Make a material (from the vsm shader)
    auto material = materialManager.CreateMaterial(gfxApi, *pComputeShader, 1,
        [this, &shadowVulkan, &vsmTarget, &vsmTargetIntermediate](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
            if (texName == "ShadowDepth")
                return { &shadowVulkan.GetDepthTexture(0) };
            else if (texName == "VarianceShadowMap")
                return { &vsmTarget };
            else if (texName == "VarianceShadowMapIntermediate")
                return { &vsmTargetIntermediate };
            assert(0);
            return {};
        }, nullptr);

    // Create the computable to execute the material
    m_Computable = std::make_unique<Computable>(vulkan, std::move(material));
    if (!m_Computable->Init())
    {
        LOGE("Error Creating VSM computable...");
        m_Computable.reset();
    }
    else
    {
        m_Computable->SetDispatchThreadCount(0, { vsmTarget.Width, vsmTarget.Height, 1 });
        m_Computable->SetDispatchThreadCount(1, { vsmTarget.Width, vsmTarget.Height, 1 });
    }
    m_VsmTarget = std::make_unique<TextureVulkan>( std::move(vsmTarget) );
    m_VsmTargetIntermediate = std::make_unique<TextureVulkan>( std::move(vsmTargetIntermediate) );

    return true;
}

void ShadowVSM::Release(GraphicsApiBase& gfxApi)
{
    m_Computable.reset();
    if (m_VsmTarget)
        m_VsmTarget->Release(&gfxApi);
    m_VsmTarget.reset();
    if (m_VsmTargetIntermediate)
        m_VsmTargetIntermediate->Release(&gfxApi);
    m_VsmTargetIntermediate.reset();
}

void ShadowVSM::AddComputableToCmdBuffer(CommandList& cmdBuffer)
{
    const auto& computable = *m_Computable;
    for (const auto& computablePass : computable.GetPasses())
    {
        auto& cmdBufferVulkan = apiCast<Vulkan>(cmdBuffer);
        computable.DispatchPass(cmdBufferVulkan.m_VkCommandBuffer, computablePass, 0);
    }
}
