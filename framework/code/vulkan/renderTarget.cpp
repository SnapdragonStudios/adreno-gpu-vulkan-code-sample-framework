//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "renderTarget.hpp"
#include "system/os_common.h"

//=============================================================================
// CRenderTarget
//=============================================================================

//-----------------------------------------------------------------------------
CRenderTarget::CRenderTarget()
//-----------------------------------------------------------------------------
{
    // class is fully initialized by member constructors and member value initilization in the class definition
}

//-----------------------------------------------------------------------------
CRenderTarget::~CRenderTarget()
//-----------------------------------------------------------------------------
{
    Release(true/*assume we own the framebuffers*/);
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
        src.m_DepthFormat = TextureFormat::UNDEFINED;
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
bool CRenderTarget::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, std::span<const VkSampleCountFlagBits> Msaa, const char* pName)
//-----------------------------------------------------------------------------
{
    m_pVulkan = pVulkan;
    m_DepthFormat = DepthFormat;
    m_Msaa.assign(Msaa.begin(), Msaa.end());
    m_Msaa.resize( pLayerFormats.size(), VK_SAMPLE_COUNT_1_BIT );
    m_FilterMode.resize(pLayerFormats.size(), SamplerFilter::Linear);

    m_Width = uiWidth;
    m_Height = uiHeight;

    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    m_ColorAttachments.clear();
    m_ClearColorValues.resize(pLayerFormats.size(), {{ 0.0f, 0.0f, 0.0f, 0.0f }});
    m_ResolveAttachments.clear();

    m_pLayerFormats.assign( pLayerFormats.begin(), pLayerFormats.end() );

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeDepth()
//-----------------------------------------------------------------------------
{
    if (m_DepthFormat != TextureFormat::UNDEFINED)
    {
        char szName[256];
        sprintf(szName, "%s: Depth", m_Name.c_str());
        m_DepthAttachment = CreateTextureObject(*m_pVulkan, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa.empty() ? VK_SAMPLE_COUNT_1_BIT : m_Msaa[0]);
    }
    else
    {
        ReleaseTexture(*m_pVulkan, &m_DepthAttachment);
    }
    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeColor(const std::span<const TEXTURE_TYPE> TextureTypes)
//-----------------------------------------------------------------------------
{
    const auto NumColorLayers = GetNumColorLayers();
    LOGI("Creating Render Target (%s): (%d x %d); %d color layer[s]", m_Name.c_str(), m_Width, m_Height, NumColorLayers);

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve(NumColorLayers);

    m_ClearColorValues.clear();
    m_ClearColorValues.resize(NumColorLayers, {{0.0f,0.0f,0.0f,0.0f}});

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

        m_ColorAttachments.emplace_back(CreateTextureObject(*m_pVulkan, createInfo));
    }

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeResolve(const std::span<const TextureFormat> ResolveTextureFormats)
//-----------------------------------------------------------------------------
{
    m_ResolveAttachments.clear();

    if (!m_Msaa.empty() && !ResolveTextureFormats.empty())
    {
        const auto NumColorLayers = GetNumColorLayers();
        LOGI("Creating Render Target (%s): (%d x %d); %d resolve layer[s]", m_Name.c_str(), m_Width, m_Height, NumColorLayers);

        m_ResolveAttachments.reserve(NumColorLayers);

        // Create texture objects to resolve in to.
        char szName[256];
        for (size_t WhichLayer = 0; WhichLayer < NumColorLayers; WhichLayer++)
        {
            if (m_Msaa[WhichLayer] != VK_SAMPLE_COUNT_1_BIT && WhichLayer < ResolveTextureFormats.size() && ResolveTextureFormats[WhichLayer] != TextureFormat::UNDEFINED)
            {
                sprintf(szName, "%s: Color Resolve", m_Name.c_str());

                const TEXTURE_TYPE TextureType = TT_RENDER_TARGET;
                m_ResolveAttachments.emplace_back(CreateTextureObject(*m_pVulkan, m_Width, m_Height, ResolveTextureFormats[WhichLayer], TextureType, m_Name.c_str()));
            }
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeFrameBuffer(VkRenderPass renderPass, const std::span<const TextureVulkan> ColorAttachments, const TextureVulkan* pDepthAttachment, const std::span<const TextureVulkan> ResolveAttachments, const TextureVulkan* pVRSAttachment, VkFramebuffer* pFramebuffer )
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    // ... then attach them to the render target
    std::vector<VkImageView> attachments;
    attachments.reserve( ColorAttachments.size() + 1/*depth*/ );

    for (const auto& ColorAttachement: ColorAttachments)
    {
        attachments.push_back(ColorAttachement.GetVkImageView());
    }
    if (pDepthAttachment && pDepthAttachment->Format != TextureFormat::UNDEFINED)
    {
        attachments.push_back(pDepthAttachment->GetVkImageView());
    }
    for (const auto& ResolveAttachment : ResolveAttachments)
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
void CRenderTarget::SetClearColors(const std::span<const VkClearColorValue> clearColors)
//-----------------------------------------------------------------------------
{
    assert(clearColors.size() == m_ColorAttachments.size());
    m_ClearColorValues.assign( std::begin(clearColors), std::end(clearColors) );
}

//-----------------------------------------------------------------------------
void CRenderTarget::Release(bool bReleaseFramebuffers)
//-----------------------------------------------------------------------------
{
    if (m_pVulkan == nullptr)
        return;

    for (auto& ColorAttachment : m_ColorAttachments)
    {
        ColorAttachment.Release(m_pVulkan);
    }
    m_ColorAttachments.clear();
    m_ClearColorValues.clear();
    m_pLayerFormats.clear();
    m_FilterMode.clear();

    for (auto& ResolveAttachment : m_ResolveAttachments)
    {
        ResolveAttachment.Release(m_pVulkan);
    }
    m_ResolveAttachments.clear();

    m_Msaa.clear();

    m_DepthAttachment.Release(m_pVulkan);
    m_DepthFormat = TextureFormat::UNDEFINED;

    if (m_FrameBufferDepthOnly != VK_NULL_HANDLE && bReleaseFramebuffers)
        vkDestroyFramebuffer(m_pVulkan->m_VulkanDevice, m_FrameBufferDepthOnly, NULL);
    m_FrameBufferDepthOnly = VK_NULL_HANDLE;

    if (m_FrameBuffer != VK_NULL_HANDLE && bReleaseFramebuffers)
        vkDestroyFramebuffer(m_pVulkan->m_VulkanDevice, m_FrameBuffer, NULL);
    m_FrameBuffer = VK_NULL_HANDLE;

    m_Height = 0;
    m_Width = 0;
    m_Name = std::string{};
}

