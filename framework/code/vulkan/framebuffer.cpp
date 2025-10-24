//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "framebuffer.hpp"
#include "vulkan.hpp"
#include "system/os_common.h"
#include "texture/vulkan/texture.hpp"


//-----------------------------------------------------------------------------
Framebuffer<Vulkan>::Framebuffer()
//-----------------------------------------------------------------------------
{
    // class is fully initialized by member constructors and member value initilization in the class definition
}

//-----------------------------------------------------------------------------
Framebuffer<Vulkan>& Framebuffer<Vulkan>::operator=( const Framebuffer<Vulkan>& other )
//-----------------------------------------------------------------------------
{
    if (&other != this)
    {
        this->m_FrameBuffer = other.m_FrameBuffer;
        this->m_Name = other.m_Name;
        this->m_RenderPassClearData = other.m_RenderPassClearData.Copy();
    }
    return *this;
}

//-----------------------------------------------------------------------------
Framebuffer<Vulkan>::Framebuffer( const Framebuffer<Vulkan>& other)
//-----------------------------------------------------------------------------
{
    *this = other;
}

//-----------------------------------------------------------------------------
bool Framebuffer<Vulkan>::Initialize( Vulkan& vulkan, const RenderPass& renderPass, const std::span<const Texture> ColorAttachments, const Texture* pDepthAttachment, std::string name, const std::span<const Texture> ResolveAttachments, const Texture* pVRSAttachment )
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    if (ColorAttachments.empty() && !pDepthAttachment)
    {
        assert( 0 && "Expect to have at least one color and/or a depth in a framebuffer" );
        return false;
    }

    // check/get dimensions from color and/or depth (we assume/require they all match; change code if we need this to not be the case)
    uint32_t width  = ColorAttachments.empty() ? pDepthAttachment->Width  : ColorAttachments.front().Width;
    uint32_t height = ColorAttachments.empty() ? pDepthAttachment->Height : ColorAttachments.front().Height;
    for (const auto& a: ColorAttachments)
    {
        if (a.Width != width || a.Height != height)
        {
            assert( 0 && "all color and depth buffers in a framebuffer should be same dimensions" );
            return false;
        }
    }
    if (!m_RenderPassClearData.Initialize( ColorAttachments, pDepthAttachment ))
    {
        return false;
    }

    std::vector<VkImageView> attachments;
    attachments.reserve( ColorAttachments.size() + 1/*depth*/ );

    for (const auto& ColorAttachement: ColorAttachments)
    {
        attachments.push_back(ColorAttachement.GetVkImageView());
    }
    if (pDepthAttachment && pDepthAttachment->Format != TextureFormat::UNDEFINED)
    {
        if (pDepthAttachment->Width != width || pDepthAttachment->Height != height)
        {
            assert( 0 && "all color and depth buffers in a framebuffer should be same dimensions" );
            return false;
        }
        attachments.push_back(pDepthAttachment->GetVkImageView());
    }
    if (attachments.empty())
    {
        assert( 0 && "Framebuffer must have color and/or depth buffer(s)" );
        return false;
    }
    for (const auto& ResolveAttachment : ResolveAttachments)
    {
        attachments.push_back( ResolveAttachment.GetVkImageView() );
    }
    if (pVRSAttachment)
    {
        attachments.push_back( pVRSAttachment->GetVkImageView() );
    }

    fvk::VkFramebufferCreateInfo BufferInfo{{
        .flags = 0,
        .renderPass = renderPass.mRenderPass,
        .attachmentCount = (uint32_t)attachments.size(),
        .pAttachments = attachments.data(),
        .width = width,
        .height = height,
        .layers = 1
    }};

    VkFramebuffer vkFramebuffer = VK_NULL_HANDLE;
    RetVal = vkCreateFramebuffer(vulkan.m_VulkanDevice, &BufferInfo, NULL, &vkFramebuffer);
    if (!CheckVkError("vkCreateFramebuffer()", RetVal))
    {
        return false;
    }
    m_Name = std::move(name);
    vulkan.SetDebugObjectName( vkFramebuffer, m_Name.c_str() );
    m_FrameBuffer = {vulkan.m_VulkanDevice, vkFramebuffer};

    return true;
}

//-----------------------------------------------------------------------------
void Framebuffer<Vulkan>::Release()
//-----------------------------------------------------------------------------
{
    m_FrameBuffer = {};
}

//-----------------------------------------------------------------------------
bool RenderPassClearData::Initialize( const std::span<const Texture> ColorAttachments,
                                      const Texture* pDepthAttachment )
//-----------------------------------------------------------------------------
{
    if (ColorAttachments.empty() && !pDepthAttachment)
    {
        assert( 0 && "Expect to have at least one color and/or a depth in a framebuffer" );
        return false;
    }
    // check/get dimensions from color and/or depth (we assume/require they all match; change code if we need this to not be the case)
    uint32_t width = ColorAttachments.empty() ? pDepthAttachment->Width : ColorAttachments.front().Width;
    uint32_t height = ColorAttachments.empty() ? pDepthAttachment->Height : ColorAttachments.front().Height;
    for (const auto& a : ColorAttachments)
    {
        if (a.Width != width || a.Height != height)
        {
            assert( 0 && "all color and depth buffers in a renderpass should be same dimensions" );
            return false;
        }
    }

    std::vector<VkClearValue> clearValues;
    clearValues.reserve( ColorAttachments.size() + 1/*potentially depth*/ );

    for (const auto& ColorAttachement : ColorAttachments)
    {
        clearValues.push_back( {ColorAttachement.GetVkClearValue()});
    }
    if (pDepthAttachment && pDepthAttachment->Format != TextureFormat::UNDEFINED)
    {
        if (pDepthAttachment->Width != width || pDepthAttachment->Height != height)
        {
            assert( 0 && "all color and depth buffers in a renderpass should be same dimensions" );
            return false;
        }
        clearValues.push_back( pDepthAttachment->GetVkClearValue() );
    }
    if (clearValues.empty())
    {
        assert( 0 && "RenderpassClearData must have color and/or depth buffer(s)" );
        return false;
    }
    this->clearValues = std::move( clearValues );

    // Viewport/scissor is set to defaults (up to end user to change if needed)
    this->scissor = {.extent {.width = width, .height = height}};
    this->viewport = {
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,   // sensible defaults but application may wish to override
        .maxDepth = 1.0f    // sensible defaults but application may wish to override
    };

    return true;
}
