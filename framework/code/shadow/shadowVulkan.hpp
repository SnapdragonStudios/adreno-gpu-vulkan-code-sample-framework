//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"
#include "vulkan/vulkan_support.hpp"
#include "vulkan/renderTarget.hpp"
#include "shadow.hpp"

/// Vulkan specialization implementation of Shadow.
template<>
class ShadowT<Vulkan> : public Shadow
{
public:
    ShadowT();

    bool Initialize(Vulkan& vulkan, uint32_t shadowMapWidth, uint32_t shadowMapHeight, bool addColorTarget);
    void Release();

    const auto& GetViewport() const
    {
        return m_Viewport;
    }
    const auto& GetScissor() const
    {
        return m_Scissor;
    }

    auto GetRenderPass() const
    {
        return m_ShadowMapRT.m_RenderPass;
    }
    auto GetFramebuffer(uint32_t idx = 0) const
    {
        return m_ShadowMapRT[idx].m_FrameBuffer;
    }
    void GetTargetSize(uint32_t& width, uint32_t& height) const
    {
        width = m_ShadowMapRT[0].m_Width;
        height = m_ShadowMapRT[0].m_Height;
    }

    auto GetDepthFormat(uint32_t frameBufferIndex = 0) const
    {
        return m_ShadowMapRT[frameBufferIndex].m_DepthAttachment.Format;
    }
    const auto& GetDepthTexture(uint32_t frameBufferIndex = 0) const
    {
        return m_ShadowMapRT[frameBufferIndex].m_DepthAttachment;
    }
    const TextureT<Vulkan>* GetColorTexture(uint32_t frameBufferIndex = 0) const
    {
        if (frameBufferIndex < m_ShadowMapRT[frameBufferIndex].m_ColorAttachments.size())
            return &m_ShadowMapRT[frameBufferIndex].m_ColorAttachments[frameBufferIndex];
        return nullptr;
    }
    const auto& GetRenderTarget(uint32_t frameBufferIndex = 0) const
    {
        return m_ShadowMapRT[frameBufferIndex];
    }

protected:
    VkViewport              m_Viewport = {};
    VkRect2D                m_Scissor = {};

    // Render targets and renderpass
    CRenderTargetArray<1>   m_ShadowMapRT;
};

using ShadowVulkan = ShadowT<Vulkan>;
