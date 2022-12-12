//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shadowVsm.hpp"
#include "shadow.hpp"
#include "material/computable.hpp"
#include "material/materialManager.hpp"
#include "material/shaderManager.hpp"
#include "vulkan/TextureFuncts.h"
#include "vulkan/vulkan_support.hpp"
#include <cassert>

ShadowVSM::ShadowVSM()
{
}

bool ShadowVSM::Initialize(Vulkan& vulkan, const ShaderManager& shaderManager, const MaterialManager& materialManager, const Shadow& shadow )
{
    // Variant Shadow Map Computable
    // Computables

    const auto* pComputeShader = shaderManager.GetShader("VarianceShadowMap");
    if (!pComputeShader)
    {
        assert(pComputeShader);
        return false;
    }

    uint32_t width, height;
    shadow.GetTargetSize(width, height);

    // Create the render target
    VulkanTexInfo vsmTarget = CreateTextureObject(&vulkan, width, height, VK_FORMAT_R16G16_SFLOAT, TEXTURE_TYPE::TT_COMPUTE_TARGET, "VSM");
    m_VsmTarget = std::make_unique<VulkanTexInfo>(std::move(vsmTarget));
    // Create the intermediate render target (result of 1st pass), width is half of output width (xy and zw channels hold alternating columns)
    VulkanTexInfo vsmTargetIntermediate = CreateTextureObject(&vulkan, width / 2, height, VK_FORMAT_R16G16B16A16_SFLOAT, TEXTURE_TYPE::TT_COMPUTE_TARGET, "VSM Intermediate");
    m_VsmTargetIntermediate = std::make_unique<VulkanTexInfo>(std::move(vsmTargetIntermediate));

    // Make a material (from the vsm shader)
    auto material = materialManager.CreateMaterial(vulkan, *pComputeShader, 1,
        [this, &shadow](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
            if (texName == "ShadowDepth")
                return { &shadow.GetDepthTexture(0) };
            else if (texName == "VarianceShadowMap")
                return { m_VsmTarget.get() };
            else if (texName == "VarianceShadowMapIntermediate")
                return { m_VsmTargetIntermediate.get() };
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
        m_Computable->SetDispatchThreadCount(0, { m_VsmTarget->Width, m_VsmTarget->Height, 1 });
        m_Computable->SetDispatchThreadCount(1, { m_VsmTarget->Width, m_VsmTarget->Height, 1 });
    }
       return true;
}

void ShadowVSM::Release(Vulkan* vulkan)
{
    assert(vulkan);
    m_Computable.reset();
    if (m_VsmTarget)
        ReleaseTexture(vulkan, m_VsmTarget.get());
    m_VsmTarget.reset();
    if (m_VsmTargetIntermediate)
        ReleaseTexture(vulkan, m_VsmTargetIntermediate.get());
    m_VsmTargetIntermediate.reset();
}

void ShadowVSM::AddComputableToCmdBuffer(Wrap_VkCommandBuffer& cmdBuffer)
{
    const auto& computable = *m_Computable;
    for (const auto& computablePass : computable.GetPasses())
    {
        computable.DispatchPass(cmdBuffer.m_VkCommandBuffer, computablePass, 0);
    }
}
