//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shadowVulkan.hpp"
#include "vulkan/vulkan_support.hpp"
#include "vulkan/vulkan.hpp"

ShadowT<Vulkan>::ShadowT() : Shadow()
{
}

bool ShadowT<Vulkan>::Initialize(Vulkan& rGfxApi, uint32_t shadowMapWidth, uint32_t shadowMapHeight, bool addColorTarget)
{
    if (!Shadow::Initialize(shadowMapWidth, shadowMapHeight, addColorTarget))
        return false;

    m_Viewport.width = (float) shadowMapWidth;
    m_Viewport.height = (float) shadowMapHeight;
    m_Viewport.minDepth = 0.0f;
    m_Viewport.maxDepth = 1.0f;
    m_Viewport.x = 0.0f;
    m_Viewport.y = 0.0f;
    m_Scissor.offset.x = 0;
    m_Scissor.offset.y = 0;
    m_Scissor.extent.width = shadowMapWidth;
    m_Scissor.extent.height = shadowMapHeight;

    const TextureFormat colorFormat[]{ TextureFormat::R8G8B8A8_UNORM };
    const std::span<const TextureFormat> emptyColorFormat{};
    TextureFormat depthFormat = TextureFormat::D32_SFLOAT;

    if (!m_ShadowMapRT.Initialize(&rGfxApi, shadowMapWidth, shadowMapHeight, addColorTarget ? colorFormat : emptyColorFormat, depthFormat, VK_SAMPLE_COUNT_1_BIT, "Shadow RT"))
    {
        LOGE("Unable to create shadow render target");
        return false;
    }
    return true;
}

void ShadowT<Vulkan>::Release()
{
    m_ShadowMapRT.Release();
}
