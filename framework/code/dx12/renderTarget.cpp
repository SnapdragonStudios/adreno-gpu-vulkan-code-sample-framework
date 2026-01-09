//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "renderTarget.hpp"
#include "system/os_common.h"
#include "commandList.hpp"


//-----------------------------------------------------------------------------
RenderTarget<Dx12>::RenderTarget()
//-----------------------------------------------------------------------------
{
    // class is fully initialized by member constructors and member value initilization in the class definition
}

//-----------------------------------------------------------------------------
RenderTarget<Dx12>::~RenderTarget()
//-----------------------------------------------------------------------------
{
    Release(true/*assume we own the framebuffers*/);
}

//-----------------------------------------------------------------------------
RenderTarget<Dx12>::RenderTarget( RenderTarget<Dx12>&& src ) noexcept
//-----------------------------------------------------------------------------
{
    *this = std::move( src );
}

//-----------------------------------------------------------------------------
RenderTarget<Dx12>& RenderTarget<Dx12>::operator=( RenderTarget<Dx12>&& src) noexcept
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
        m_ClearColors = std::move( src.m_ClearColors );
        m_ResolveAttachments = std::move( src.m_ResolveAttachments );
        m_DepthAttachment = std::move( src.m_DepthAttachment );
        //m_FrameBuffer = src.m_FrameBuffer;
        //src.m_FrameBuffer = VK_NULL_HANDLE;
        //m_FrameBufferDepthOnly = src.m_FrameBufferDepthOnly;
        //src.m_FrameBufferDepthOnly = VK_NULL_HANDLE;

        m_pGfxApi = src.m_pGfxApi;
        src.m_pGfxApi = nullptr;
    }

    return *this;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Dx12>::Initialize( Dx12* pGfxApi, const RenderTargetInitializeInfo& info, const char* pName )
//-----------------------------------------------------------------------------
{
    const size_t numColorAttachments = info.LayerFormats.size();

    m_pGfxApi = pGfxApi;
    m_DepthFormat = info.DepthFormat;

    m_Msaa.assign( info.Msaa.begin(), info.Msaa.end() );
    if (info.Msaa.empty())
        m_Msaa.resize( numColorAttachments, Msaa::Samples1 );
    else
        m_Msaa.resize( numColorAttachments, info.Msaa.back() );

    m_FilterMode.assign( info.FilterModes.begin(), info.FilterModes.end() );
    if (info.FilterModes.empty())
        m_FilterMode.resize( numColorAttachments, SamplerFilter::Linear );
    else
        m_FilterMode.resize( numColorAttachments, info.FilterModes.back() );

    m_Width = info.Width;
    m_Height = info.Height;

    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve( numColorAttachments );
    m_ClearColors.resize( numColorAttachments, {{ 0.0f, 0.0f, 0.0f, 0.0f }} );
    m_ResolveAttachments.clear();

    m_pLayerFormats.assign( info.LayerFormats.begin(), info.LayerFormats.end() );

    for (auto i = 0; i < numColorAttachments; ++i)
    {
        m_ColorAttachments.emplace_back( CreateTextureObject( *m_pGfxApi, m_Width, m_Height, info.LayerFormats[i], i < info.TextureTypes.size() ? info.TextureTypes[i] : TT_RENDER_TARGET, m_Name.c_str(), m_Msaa[i]));
    }

    if (m_DepthFormat != TextureFormat::UNDEFINED)
        m_DepthAttachment = CreateTextureObject( *m_pGfxApi, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa[0] );
    else
        m_DepthFormat = {};


    // Make a descriptor heap for the swapchain render targets
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (GetNumColorLayers() > 0)
    {
        uint32_t rtvHandleSize = 0;
        descriptorHeap = m_pGfxApi->CreateDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, GetNumColorLayers(), &rtvHandleSize );
        D3D12_CPU_DESCRIPTOR_HANDLE descHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();

        // Color
        for (UINT i = 0; i < m_ColorAttachments.size(); ++i)
        {
            const auto& attachment = m_ColorAttachments[i];
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{.Format = attachment.GetResourceViewDesc().Format,
                                                  .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D};
            m_pGfxApi->GetDevice()->CreateRenderTargetView( attachment.GetResource(), &rtvDesc, descHandle );
            descHandle.ptr += rtvHandleSize;
        }
    }

    // Depth
    ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap;
    if (!m_DepthAttachment.IsEmpty())
    {
        uint32_t dsvHandleSize = 0;
        depthDescriptorHeap = m_pGfxApi->CreateDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, &dsvHandleSize );
        D3D12_CPU_DESCRIPTOR_HANDLE descHandle = depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

        const auto& attachment = m_DepthAttachment;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{.Format = attachment.GetResourceViewDesc().Format,
                                              .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                                              .Flags = D3D12_DSV_FLAG_NONE};
        m_pGfxApi->GetDevice()->CreateDepthStencilView( attachment.GetResource(), &dsvDesc, descHandle );
    }

    m_descriptorHeap = std::move( descriptorHeap );
    m_depthDescriptorHeap = std::move( depthDescriptorHeap );
    return true;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Dx12>::Initialize(Dx12* pGfxApi, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, std::span<const Msaa> Msaa, const char* pName)
//-----------------------------------------------------------------------------
{
    m_pGfxApi = pGfxApi;
    m_DepthFormat = DepthFormat;
    m_Msaa.assign(Msaa.begin(), Msaa.end());
    m_Msaa.resize(pLayerFormats.size(), Msaa::Samples1);
    m_FilterMode.resize(pLayerFormats.size(), SamplerFilter::Linear);

    m_Width = uiWidth;
    m_Height = uiHeight;

    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve(pLayerFormats.size());
    m_ClearColors.resize(pLayerFormats.size(), {{ 0.0f, 0.0f, 0.0f, 0.0f }});
    m_ResolveAttachments.clear();

    m_pLayerFormats.assign(pLayerFormats.begin(), pLayerFormats.end());

    for(auto i=0;i< pLayerFormats.size(); ++i)
    {
        m_ColorAttachments.emplace_back( CreateTextureObject( *m_pGfxApi, m_Width, m_Height, pLayerFormats[i], TT_RENDER_TARGET, m_Name.c_str(), m_Msaa.empty() ? Msaa::Samples1 : m_Msaa[0] ) );
    }

    if (m_DepthFormat != TextureFormat::UNDEFINED)
        m_DepthAttachment = CreateTextureObject( *m_pGfxApi, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa.empty() ? Msaa::Samples1 : m_Msaa[0] );
    else
        m_DepthFormat = {};


    // Make a descriptor heap for the swapchain render targets
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (GetNumColorLayers() > 0)
    {
        uint32_t rtvHandleSize = 0;
        descriptorHeap = m_pGfxApi->CreateDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, GetNumColorLayers(), &rtvHandleSize );
        D3D12_CPU_DESCRIPTOR_HANDLE descHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();

        // Color
        for (UINT i = 0; i < m_ColorAttachments.size(); ++i)
        {
            const auto& attachment = m_ColorAttachments[i];
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{.Format = attachment.GetResourceViewDesc().Format,
                                                  .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D};
            m_pGfxApi->GetDevice()->CreateRenderTargetView( attachment.GetResource(), &rtvDesc, descHandle );
            descHandle.ptr += rtvHandleSize;
        }
    }

    // Depth
    ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap;
    if (!m_DepthAttachment.IsEmpty())
    {
        uint32_t dsvHandleSize = 0;
        depthDescriptorHeap = m_pGfxApi->CreateDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, &dsvHandleSize );
        D3D12_CPU_DESCRIPTOR_HANDLE descHandle = depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

        const auto& attachment = m_DepthAttachment;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{.Format = attachment.GetResourceViewDesc().Format,
                                              .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                                              .Flags = D3D12_DSV_FLAG_NONE};
        m_pGfxApi->GetDevice()->CreateDepthStencilView( attachment.GetResource(), &dsvDesc, descHandle );
    }

    m_descriptorHeap = std::move(descriptorHeap);
    m_depthDescriptorHeap = std::move( depthDescriptorHeap );
    return true;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Dx12>::InitializeDepth()
//-----------------------------------------------------------------------------
{
    if (m_DepthFormat != TextureFormat::UNDEFINED)
    {
        char szName[256];
        sprintf(szName, "%s: Depth", m_Name.c_str());
        m_DepthAttachment = CreateTextureObject(*m_pGfxApi, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa.empty() ? Msaa::Samples1 : m_Msaa[0]);
    }
    else
    {
        ReleaseTexture(*m_pGfxApi, &m_DepthAttachment);
    }
    return true;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Dx12>::InitializeColor(const std::span<const TEXTURE_TYPE> TextureTypes)
//-----------------------------------------------------------------------------
{
    const auto NumColorLayers = GetNumColorLayers();
    LOGI("Creating Render Target (%s): (%d x %d); %d color layer[s]", m_Name.c_str(), m_Width, m_Height, NumColorLayers);

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve(NumColorLayers);

    m_ClearColors.clear();
    m_ClearColors.resize(NumColorLayers, {{0.0f,0.0f,0.0f,0.0f}});

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

        m_ColorAttachments.emplace_back(CreateTextureObject(*m_pGfxApi, createInfo));
    }

    return true;
}

//-----------------------------------------------------------------------------
bool RenderTarget<Dx12>::InitializeResolve(const std::span<const TextureFormat> ResolveTextureFormats)
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
                m_ResolveAttachments.emplace_back(CreateTextureObject(*m_pGfxApi, m_Width, m_Height, ResolveTextureFormats[WhichLayer], TextureType, m_Name.c_str()));
            }
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
void RenderTarget<Dx12>::SetClearColors(const std::span<const ClearColor> clearColors)
//-----------------------------------------------------------------------------
{
    assert(clearColors.size() == m_ColorAttachments.size());
    m_ClearColors.assign( std::begin(clearColors), std::end(clearColors) );
}

//-----------------------------------------------------------------------------
void RenderTarget<Dx12>::Release(bool bReleaseFramebuffers)
//-----------------------------------------------------------------------------
{
    if (m_pGfxApi == nullptr)
        return;

    for (auto& ColorAttachment : m_ColorAttachments)
    {
        ColorAttachment.Release(m_pGfxApi);
    }
    m_ColorAttachments.clear();
    m_ClearColors.clear();
    m_pLayerFormats.clear();
    m_FilterMode.clear();

    for (auto& ResolveAttachment : m_ResolveAttachments)
    {
        ResolveAttachment.Release(m_pGfxApi);
    }
    m_ResolveAttachments.clear();

    m_Msaa.clear();

    m_DepthAttachment.Release(m_pGfxApi);
    m_DepthFormat = TextureFormat::UNDEFINED;

//    if (m_FrameBufferDepthOnly != VK_NULL_HANDLE && bReleaseFramebuffers)
//        vkDestroyFramebuffer(m_pGfxApi->m_VulkanDevice, m_FrameBufferDepthOnly, NULL);
//    m_FrameBufferDepthOnly = VK_NULL_HANDLE;

//    if (m_FrameBuffer != VK_NULL_HANDLE && bReleaseFramebuffers)
//        vkDestroyFramebuffer(m_pGfxApi->m_VulkanDevice, m_FrameBuffer, NULL);
//    m_FrameBuffer = VK_NULL_HANDLE;

    m_Height = 0;
    m_Width = 0;
    m_Name = std::string{};
}

//-----------------------------------------------------------------------------
void RenderTarget<Dx12>::SetRenderTarget(CommandList<Dx12>& commandList)
//-----------------------------------------------------------------------------
{
    const D3D12_VIEWPORT viewport{
    .TopLeftX = 0.0f,
    .TopLeftY = 0.0f,
    .Width = (float)m_Width,
    .Height = (float)m_Height,
    .MinDepth = 0.0f,
    .MaxDepth = 1.0f
    };
    const D3D12_RECT scissor{
        .left = 0,
        .top = 0,
        .right = (LONG)m_Width,
        .bottom = (LONG)m_Height
    };

    commandList->RSSetViewports( 1, &viewport );
    commandList->RSSetScissorRects( 1, &scissor );

    //D3D12_RESOURCE_BARRIER backbufferBarrier{
    //    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    //    .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE ,
    //    .Transition = {
    //        .pResource = m_FrameBuffers[whichFrame].Get(),
    //        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
    //        .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
    //        .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
    //    }
    //};
    //pCommandList->ResourceBarrier( 1, &backbufferBarrier );

    D3D12_CPU_DESCRIPTOR_HANDLE colorDescriptorHandle{.ptr = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr};
    D3D12_CPU_DESCRIPTOR_HANDLE depthDescriptorHandle{.ptr = m_depthDescriptorHeap ? m_depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr : 0};
    commandList->OMSetRenderTargets( GetNumColorLayers(), &colorDescriptorHandle, FALSE, m_DepthAttachment.IsEmpty() ? nullptr : &depthDescriptorHandle);

    const float clearColor[4] {};
    commandList->ClearRenderTargetView( colorDescriptorHandle, clearColor, 0, nullptr );

    if (!m_DepthAttachment.IsEmpty())
    {
        commandList->ClearDepthStencilView( depthDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
    }
}
