//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "renderTarget.hpp"
#include "system/os_common.h"
//#include "vulkan_support.hpp"
//#include "system/assetManager.hpp"
//#include "memory/memoryManager.hpp"

//=============================================================================
// CRenderTarget
//=============================================================================

//-----------------------------------------------------------------------------
CRenderTarget::CRenderTarget() :
//-----------------------------------------------------------------------------
    m_FrameBuffer(VK_NULL_HANDLE),
    m_FrameBufferDepthOnly(VK_NULL_HANDLE)
{
    HardReset();
}

//-----------------------------------------------------------------------------
CRenderTarget::~CRenderTarget()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
CRenderTarget::CRenderTarget( CRenderTarget&& src ) noexcept
{
    *this = std::move( src );
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CRenderTarget& CRenderTarget::operator=( CRenderTarget&& src) noexcept
//-----------------------------------------------------------------------------
{
    if (this != &src)
    {
        m_Name = std::move(src.m_Name);
        m_Width = src.m_Width;
        src.m_Width = 0;
        m_Height = src.m_Height;
        src.m_Height = 0;
        m_pLayerFormats = std::move( src.m_pLayerFormats );
        m_Msaa = std::move( src.m_Msaa );
        m_FilterMode = std::move( src.m_FilterMode );
        m_DepthFormat = src.m_DepthFormat;
        src.m_DepthFormat = VK_FORMAT_UNDEFINED;
        m_ColorAttachments = std::move( src.m_ColorAttachments );
        m_ClearColorValues = std::move( src.m_ClearColorValues );
        m_ResolveAttachments = std::move( src.m_ResolveAttachments );
        m_DepthAttachment = std::move( src.m_DepthAttachment );
        m_FrameBuffer = src.m_FrameBuffer;
        src.m_FrameBuffer = VK_NULL_HANDLE;
        m_FrameBufferDepthOnly = src.m_FrameBufferDepthOnly;
        src.m_FrameBufferDepthOnly = VK_NULL_HANDLE;

        m_pVulkan = src.m_pVulkan;
        src.m_pVulkan = nullptr;
    }

    return *this;
}

//-----------------------------------------------------------------------------
void CRenderTarget::HardReset()
//-----------------------------------------------------------------------------
{
    m_Name = "RenderTarget";

    m_Width = 0;
    m_Height = 0;

    m_pLayerFormats.clear();
    m_Msaa = {};
    m_FilterMode = {};
    m_DepthFormat = VK_FORMAT_UNDEFINED;

    m_ColorAttachments.clear();
    m_ClearColorValues.clear();
    m_ResolveAttachments.clear();
    m_DepthAttachment = VulkanTexInfo();

    assert(m_FrameBuffer == VK_NULL_HANDLE);
    m_FrameBuffer = VK_NULL_HANDLE;
    assert(m_FrameBufferDepthOnly == VK_NULL_HANDLE);
    m_FrameBufferDepthOnly = VK_NULL_HANDLE;

    m_pVulkan = nullptr;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat, tcb::span<const VkSampleCountFlagBits> Msaa, const char* pName)
//-----------------------------------------------------------------------------
{
    m_pVulkan = pVulkan;
    m_DepthFormat = DepthFormat;
    m_Msaa.assign(Msaa.begin(), Msaa.end());
    m_Msaa.resize( pLayerFormats.size(), VK_SAMPLE_COUNT_1_BIT );
    m_FilterMode.resize(pLayerFormats.size(), VK_FILTER_LINEAR);

    m_Width = uiWidth;
    m_Height = uiHeight;

    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    m_ColorAttachments.clear();
    m_ClearColorValues.resize(pLayerFormats.size(), { 0.0f, 0.0f, 0.0f, 0.0f });
    m_ResolveAttachments.clear();

    m_pLayerFormats.assign( pLayerFormats.begin(), pLayerFormats.end() );

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeDepth()
//-----------------------------------------------------------------------------
{
    if (m_DepthFormat != VK_FORMAT_UNDEFINED)
    {
        char szName[256];
        sprintf(szName, "%s: Depth", m_Name.c_str());
        m_DepthAttachment = CreateTextureObject(m_pVulkan, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa.empty() ? VK_SAMPLE_COUNT_1_BIT : m_Msaa[0]);
    }
    else
    {
        ReleaseTexture(m_pVulkan, &m_DepthAttachment);
    }
    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeColor(const tcb::span<const TEXTURE_TYPE> TextureTypes)
//-----------------------------------------------------------------------------
{
    const auto NumColorLayers = GetNumColorLayers();
    LOGI("Creating Render Target (%s): (%d x %d); %d color layer[s]", m_Name.c_str(), m_Width, m_Height, NumColorLayers);

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve(NumColorLayers);

    m_ClearColorValues.clear();
    m_ClearColorValues.resize(NumColorLayers, {0.0f,0.0f,0.0f,0.0f});

    CreateTexObjectInfo createInfo{};
    createInfo.uiWidth = m_Width;
    createInfo.uiHeight = m_Height;

    // First create the actual texture objects...
    char szName[256];
    for (size_t WhichLayer = 0; WhichLayer < NumColorLayers; WhichLayer++)
    {
        sprintf(szName, "%s: Color", m_Name.c_str());

        createInfo.Format = m_pLayerFormats[WhichLayer];
        createInfo.TexType = (WhichLayer < TextureTypes.size()) ? TextureTypes[WhichLayer] : TT_RENDER_TARGET;
        createInfo.pName = m_Name.c_str();
        createInfo.Msaa = m_Msaa[WhichLayer];
        createInfo.FilterMode = m_FilterMode[WhichLayer];

        m_ColorAttachments.emplace_back(CreateTextureObject(m_pVulkan, createInfo));
    }

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeResolve(const tcb::span<const TEXTURE_TYPE> TextureTypes)
//-----------------------------------------------------------------------------
{
    m_ResolveAttachments.clear();

    if (!m_Msaa.empty())
    {
        const auto NumColorLayers = GetNumColorLayers();
        LOGI("Creating Render Target (%s): (%d x %d); %d resolve layer[s]", m_Name.c_str(), m_Width, m_Height, NumColorLayers);

        m_ResolveAttachments.reserve(NumColorLayers);

        // Create texture objects to resolve in to.
        char szName[256];
        for (size_t WhichLayer = 0; WhichLayer < NumColorLayers; WhichLayer++)
        {
            if (m_Msaa[WhichLayer] != VK_SAMPLE_COUNT_1_BIT)
            {
                sprintf(szName, "%s: Color Resolve", m_Name.c_str());

                const TEXTURE_TYPE TextureType = (WhichLayer < TextureTypes.size()) ? TextureTypes[WhichLayer] : TT_RENDER_TARGET;
                m_ResolveAttachments.emplace_back(CreateTextureObject(m_pVulkan, m_Width, m_Height, m_pLayerFormats[WhichLayer], TextureType, m_Name.c_str()));
            }
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeColor(const SwapchainBuffers& SwapchainBuffer)
//-----------------------------------------------------------------------------
{
    LOGI("Creating Swapchain Render Target (%s): (%d x %d)", m_Name.c_str(), m_Width, m_Height);

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve(1);
    m_ClearColorValues.clear();
    m_ClearColorValues.resize(1, { 0.0f,0.0f,0.0f,0.0f });

    char szName[256];

    sprintf(szName, "%s: Swapchain", m_Name.c_str());

    m_ColorAttachments.emplace_back(VulkanTexInfo(m_Width, m_Height, 1, 0, m_pVulkan->m_SurfaceFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SwapchainBuffer.image, VK_NULL_HANDLE, VK_NULL_HANDLE, SwapchainBuffer.view));

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeResolve(const SwapchainBuffers& SwapchainBuffer)
//-----------------------------------------------------------------------------
{
    m_ResolveAttachments.clear();

    if (!m_Msaa.empty())
    {
        LOGI("Creating Swapchain resolved Render Target (%s): (%d x %d)", m_Name.c_str(), m_Width, m_Height);

        m_ResolveAttachments.reserve(1);

        char szName[256];

        sprintf(szName, "%s: resolve Swapchain", m_Name.c_str());

        m_ResolveAttachments.emplace_back(VulkanTexInfo(m_Width, m_Height, 1, 0, m_pVulkan->m_SurfaceFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SwapchainBuffer.image, VK_NULL_HANDLE, VK_NULL_HANDLE, SwapchainBuffer.view));
    }

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeFrameBuffer(VkRenderPass renderPass, const tcb::span<const VulkanTexInfo> ColorAttachments, const VulkanTexInfo* pDepthAttachment, const tcb::span<const VulkanTexInfo> ResolveAttachments, const VulkanTexInfo* pVRSAttachment, VkFramebuffer* pFramebuffer )
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    // ... then attach them to the render target
    std::vector<VkImageView> attachments;
    attachments.reserve( ColorAttachments.size() + 1/*depth*/ );

    for (const VulkanTexInfo& ColorAttachement: ColorAttachments)
    {
        attachments.push_back(ColorAttachement.GetVkImageView());
    }
    if (pDepthAttachment && pDepthAttachment->Format != VK_FORMAT_UNDEFINED)
    {
        attachments.push_back(pDepthAttachment->GetVkImageView());
    }
    for (const VulkanTexInfo& ResolveAttachment : ResolveAttachments)
    {
        attachments.push_back( ResolveAttachment.GetVkImageView() );
    }
    if (pVRSAttachment)
    {
        attachments.push_back( pVRSAttachment->GetVkImageView() );
    }

    VkFramebufferCreateInfo BufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    BufferInfo.flags = 0;
    BufferInfo.renderPass = renderPass;
    BufferInfo.attachmentCount = (uint32_t)attachments.size();
    BufferInfo.pAttachments = attachments.data();
    BufferInfo.width = m_Width;
    BufferInfo.height = m_Height;
    BufferInfo.layers = 1;

    RetVal = vkCreateFramebuffer(m_pVulkan->m_VulkanDevice, &BufferInfo, NULL, pFramebuffer);
    if (!CheckVkError("vkCreateFramebuffer()", RetVal))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
void CRenderTarget::SetClearColors(const tcb::span<const VkClearColorValue> clearColors)
//-----------------------------------------------------------------------------
{
    assert(clearColors.size() == m_ColorAttachments.size());
    m_ClearColorValues.assign( std::begin(clearColors), std::end(clearColors) );
}

//-----------------------------------------------------------------------------
void CRenderTarget::Release()
//-----------------------------------------------------------------------------
{
    if (m_pVulkan == nullptr)
        return;

    for (auto& ColorAttachment : m_ColorAttachments)
    {
        ReleaseTexture(m_pVulkan, &ColorAttachment);
    }
    m_ColorAttachments.clear();
    m_ClearColorValues.clear();

    for (auto& ResolveAttachment : m_ResolveAttachments)
    {
        ReleaseTexture(m_pVulkan, &ResolveAttachment);
    }
    m_ResolveAttachments.clear();

    m_pLayerFormats.clear();

    ReleaseTexture(m_pVulkan, &m_DepthAttachment);

    if (m_FrameBufferDepthOnly != VK_NULL_HANDLE)
        vkDestroyFramebuffer(m_pVulkan->m_VulkanDevice, m_FrameBufferDepthOnly, NULL);
    m_FrameBufferDepthOnly = VK_NULL_HANDLE;

    if (m_FrameBuffer != VK_NULL_HANDLE)
        vkDestroyFramebuffer(m_pVulkan->m_VulkanDevice, m_FrameBuffer, NULL);
    m_FrameBuffer = VK_NULL_HANDLE;

    // Clear everything back to starting state
    HardReset();
}

