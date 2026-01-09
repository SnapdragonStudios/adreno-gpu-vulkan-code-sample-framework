//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "renderTarget.hpp"
#include "system/os_common.h"


//-----------------------------------------------------------------------------
RenderTarget<Vulkan>::RenderTarget()
//-----------------------------------------------------------------------------
{
    // class is fully initialized by member constructors and member value initilization in the class definition
}

//-----------------------------------------------------------------------------
RenderTarget<Vulkan>::~RenderTarget()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
RenderTarget<Vulkan>::RenderTarget( RenderTarget<Vulkan>&& src ) noexcept
//-----------------------------------------------------------------------------
{
    *this = std::move( src );
}

//-----------------------------------------------------------------------------
RenderTarget<Vulkan>& RenderTarget<Vulkan>::operator=( RenderTarget<Vulkan>&& src) noexcept
//-----------------------------------------------------------------------------
{
    if (this != &src)
    {
        Release();  // Release first so textures get freed!

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
        m_InheritedDepthAttachment = std::move( src.m_InheritedDepthAttachment);
        src.m_InheritedDepthAttachment = nullptr;
        m_FrameBuffer = std::move( src.m_FrameBuffer );
        m_FrameBufferDepthOnly = std::move( src.m_FrameBufferDepthOnly );

        m_pVulkan = src.m_pVulkan;
        src.m_pVulkan = nullptr;
    }

    return *this;
}


//-----------------------------------------------------------------------------
bool RenderTarget<Vulkan>::Initialize( Vulkan* pVulkan, const RenderTargetInitializeInfo& info, const char* pName, const RenderPass<Vulkan>* renderPass, const RenderPass<Vulkan>* renderPassDepthOnly )
//-----------------------------------------------------------------------------
{
    const size_t numColorAttachments = info.LayerFormats.size();

    m_pVulkan = pVulkan;
    m_DepthFormat = info.DepthFormat;
    m_Msaa.assign( info.Msaa.begin(), info.Msaa.end() );
    if (info.Msaa.empty())
        m_Msaa.resize( numColorAttachments, Msaa::Samples1 );
    else
        m_Msaa.resize( numColorAttachments, m_Msaa.back() );

    m_FilterMode.assign( info.FilterModes.begin(), info.FilterModes.end() );
    if (info.FilterModes.empty())
        m_FilterMode.resize( numColorAttachments, SamplerFilter::Linear );
    else
        m_FilterMode.resize( numColorAttachments, info.FilterModes.back() );

    std::vector<TEXTURE_TYPE> colorTextureTypes {info.TextureTypes.begin(), info.TextureTypes.end()};
    if (info.TextureTypes.empty())
        colorTextureTypes.resize( numColorAttachments, TT_RENDER_TARGET );
    else
        colorTextureTypes.resize( numColorAttachments, info.TextureTypes.back() );

    std::optional<TEXTURE_TYPE> depthTextureType = info.DepthTextureType;

    m_Width = info.Width;
    m_Height = info.Height;

    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    m_ColorAttachments.clear();
    m_ClearColorValues.resize( numColorAttachments, {{ 0.0f, 0.0f, 0.0f, 0.0f }} );
    m_ResolveAttachments.clear();

    m_pLayerFormats.assign( info.LayerFormats.begin(), info.LayerFormats.end() );

    if (info.InheritedDepthAttachment)
    {
        auto* inheritedDepthAttachment = apiCast<Vulkan>(info.InheritedDepthAttachment);
        if (!inheritedDepthAttachment->m_DepthAttachment)
        {
            return false;
        }

        m_InheritedDepthAttachment = &inheritedDepthAttachment->m_DepthAttachment;
        m_DepthFormat = m_InheritedDepthAttachment->Format;
    }
    else
    {
        if (!InitializeDepth(depthTextureType))
            return false;
    }

    if (!InitializeColor(colorTextureTypes))
        return false;
    if (!InitializeResolve( info.ResolveTextureFormats ))
        return false;
    if (renderPass && *renderPass && !CreateFrameBuffer( *renderPass, m_ColorAttachments, m_InheritedDepthAttachment ? m_InheritedDepthAttachment : &m_DepthAttachment, m_ResolveAttachments, nullptr/*pVRSAttachment*/, &m_FrameBuffer ))
        return false;
    if (renderPassDepthOnly && *renderPassDepthOnly && (m_InheritedDepthAttachment || m_DepthAttachment) && !CreateFrameBuffer( *renderPassDepthOnly, {}, m_InheritedDepthAttachment ? m_InheritedDepthAttachment : &m_DepthAttachment, m_ResolveAttachments, nullptr/*pVRSAttachment*/, &m_FrameBufferDepthOnly ))
        return false;

    return true;
}


//-----------------------------------------------------------------------------
bool RenderTarget<Vulkan>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, const char* pName, const std::span<const TEXTURE_TYPE> TextureTypes, std::span<const Msaa> Multisample, const std::span<const TextureFormat> ResolveTextureFormats, const std::span<const SamplerFilter> FilterModes)
//-----------------------------------------------------------------------------
{
    const RenderTargetInitializeInfo info{
        .Width = uiWidth,
        .Height = uiHeight,
        .LayerFormats = pLayerFormats,
        .DepthFormat = DepthFormat,
        .TextureTypes = TextureTypes,
        .Msaa = Multisample,
        .ResolveTextureFormats = ResolveTextureFormats,
        .FilterModes = FilterModes
    };
    return Initialize( pVulkan, info, pName, nullptr, nullptr );
}

////-----------------------------------------------------------------------------
//bool RenderTarget<Vulkan>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureVulkan* inheritDepth, Msaa msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
////-----------------------------------------------------------------------------
//{
//    if (!Initialize( pVulkan, uiWidth, uiHeight, pLayerFormats, TextureFormat::UNDEFINED/*depth*/, msaa, pName ))
//        return false;
//    if (inheritDepth)
//        m_DepthAttachment = std::move( TextureVulkan( inheritDepth->Width, inheritDepth->Height, inheritDepth->Depth, inheritDepth->MipLevels, inheritDepth->FirstMip, inheritDepth->Faces, inheritDepth->FirstFace, inheritDepth->Format, inheritDepth->ImageLayout, inheritDepth->ClearValue, inheritDepth->Image, inheritDepth->Sampler, inheritDepth->ImageView ) );
//}


//-----------------------------------------------------------------------------
bool RenderTarget<Vulkan>::InitializeFrameBuffer( Vulkan* pVulkan, const RenderPass<Vulkan>& renderPass )
//-----------------------------------------------------------------------------
{
    bool success = CreateFrameBuffer( renderPass, m_ColorAttachments, m_InheritedDepthAttachment ? m_InheritedDepthAttachment : &m_DepthAttachment, m_ResolveAttachments, nullptr, &m_FrameBuffer );
    return success;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Vulkan>::InitializeFrameBufferDepthOnly( Vulkan* pVulkan, const RenderPass<Vulkan>& renderPassDepthOnly )
//-----------------------------------------------------------------------------
{
    bool success = CreateFrameBuffer( renderPassDepthOnly, {}, m_InheritedDepthAttachment ? m_InheritedDepthAttachment : &m_DepthAttachment, {}, nullptr, &m_FrameBufferDepthOnly );
    return success;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Vulkan>::InitializeDepth(std::optional<TEXTURE_TYPE> textureType)
//-----------------------------------------------------------------------------
{
    if (m_DepthFormat != TextureFormat::UNDEFINED)
    {
        char szName[256];
        sprintf(szName, "%s: Depth", m_Name.c_str());
        m_DepthAttachment = CreateTextureObject(*m_pVulkan, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa.empty() ? Msaa::Samples1 : m_Msaa[0]);
    }
    else
    {
        ReleaseTexture(*m_pVulkan, &m_DepthAttachment);
    }
    return true;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Vulkan>::InitializeColor(const std::span<const TEXTURE_TYPE> TextureTypes)
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
bool RenderTarget<Vulkan>::InitializeResolve(const std::span<const TextureFormat> ResolveTextureFormats)
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
            if (m_Msaa[WhichLayer] != Msaa::Samples1 && WhichLayer < ResolveTextureFormats.size() && ResolveTextureFormats[WhichLayer] != TextureFormat::UNDEFINED)
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
bool RenderTarget<Vulkan>::CreateFrameBuffer(const RenderPass<Vulkan>& renderPass, const std::span<const TextureVulkan> ColorAttachments, const TextureVulkan* pDepthAttachment, const std::span<const TextureVulkan> ResolveAttachments, const TextureVulkan* pVRSAttachment, Framebuffer<Vulkan>* pFramebuffer )
//-----------------------------------------------------------------------------
{
    VkResult RetVal;
    assert( pFramebuffer );

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

    fvk::VkFramebufferCreateInfo BufferInfo{{
        .flags = 0,
        .renderPass = renderPass.mRenderPass,
        .attachmentCount = (uint32_t)attachments.size(),
        .pAttachments = attachments.data(),
        .width = m_Width,
        .height = m_Height,
        .layers = 1
    }};

    return pFramebuffer->Initialize( *m_pVulkan, renderPass, ColorAttachments, pDepthAttachment, m_Name, ResolveAttachments, pVRSAttachment );
}

//-----------------------------------------------------------------------------
void RenderTarget<Vulkan>::SetClearColors(const std::span<const VkClearColorValue> clearColors)
//-----------------------------------------------------------------------------
{
    assert(clearColors.size() == m_ColorAttachments.size());
    m_ClearColorValues.assign( std::begin(clearColors), std::end(clearColors) );
}

//-----------------------------------------------------------------------------
void RenderTarget<Vulkan>::Release()
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
    m_InheritedDepthAttachment = nullptr;
    m_DepthFormat = TextureFormat::UNDEFINED;

    m_FrameBufferDepthOnly = {};
    m_FrameBuffer = {};

    m_Height = 0;
    m_Width = 0;
    m_Name = std::string{};
}

