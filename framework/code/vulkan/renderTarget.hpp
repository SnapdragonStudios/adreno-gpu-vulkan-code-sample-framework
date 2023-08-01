//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <assert.h>
#include <array>
#include <span>
#include <algorithm>
#include "texture/vulkan/texture.hpp"
#include "vulkan.hpp"
//#include "TextureFuncts.h"
#include "system/os_common.h"

//=============================================================================
// CRenderTarget
//=============================================================================

/// Container for a single frame render target.
/// Contains multiple color buffers, multiple resolve buffers, and an optional depth buffer.
class CRenderTarget
{
    // Functions
    CRenderTarget(const CRenderTarget&) = delete;
    CRenderTarget& operator=(const CRenderTarget&) = delete;

public:
    CRenderTarget();
    ~CRenderTarget();
    CRenderTarget( CRenderTarget&& ) noexcept;
    CRenderTarget& operator=( CRenderTarget&& ) noexcept;

    uint32_t GetNumColorLayers() const { return (uint32_t)m_pLayerFormats.size(); }

    bool Initialize( Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat = TextureFormat::D24_UNORM_S8_UINT, std::span<const VkSampleCountFlagBits> Msaa = {}, const char* pName = NULL);
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat = TextureFormat::D24_UNORM_S8_UINT, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL)
    {
        return Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, { &Msaa,1 }, pName);
    }
private:
    template<uint32_t T_NUM_BUFFERS> friend class CRenderTargetArray;
    bool InitializeDepth();
    bool InitializeColor(const std::span<const TEXTURE_TYPE> TextureTypes);
    bool InitializeResolve(const std::span<const TextureFormat> ResolveTextureFormats);

    // Allow buffers to be initialized from the swapchain (for render target that writes to the swapchain)
    bool InitializeColor(const SwapchainBuffers& SwapchainBuffer);
    bool InitializeResolve(const SwapchainBuffers& SwapchainBuffer);

    bool InitializeFrameBuffer(VkRenderPass renderPass, const std::span<const TextureVulkan> ColorAttachments, const TextureVulkan* pDepthAttachment, const std::span<const TextureVulkan> ResolveAttachments, const TextureVulkan* pVRSAttachment, VkFramebuffer* pFramebuffer);

    void SetClearColors(const std::span<const VkClearColorValue> clearColors);

    void Release(bool bReleaseFramebuffers /*set true if we are the owner of the framebuffers (and so want to clean them up)*/);

    // Attributes
public:
    std::string         m_Name;

    uint32_t            m_Width = 0;
    uint32_t            m_Height = 0;

    std::vector<TextureFormat> m_pLayerFormats;
    std::vector<VkSampleCountFlagBits> m_Msaa;
    std::vector<SamplerFilter> m_FilterMode;
    TextureFormat            m_DepthFormat = TextureFormat::UNDEFINED;

    // The Color Attachments

    std::vector<TextureVulkan> m_ColorAttachments;
    std::vector<VkClearColorValue> m_ClearColorValues;

    // The Resolve Attachments
    std::vector<TextureVulkan> m_ResolveAttachments;

    // The Depth Attachment
    TextureVulkan		m_DepthAttachment;

    // The Frame Buffer
    VkFramebuffer       m_FrameBuffer = VK_NULL_HANDLE;

    // The Frame Buffer (depth only)
    VkFramebuffer       m_FrameBufferDepthOnly = VK_NULL_HANDLE;

private:
    Vulkan* m_pVulkan = nullptr;
};

/// Fixed size array of CRenderTargets (eg one per 'frame') that share RenderPass objects
template<uint32_t T_NUM_BUFFERS>
class CRenderTargetArray
{
    CRenderTargetArray(const CRenderTargetArray&) = delete;
    CRenderTargetArray& operator=(const CRenderTargetArray&) = delete;
public:
    CRenderTargetArray() = default;
    ~CRenderTargetArray();
    CRenderTargetArray( CRenderTargetArray<T_NUM_BUFFERS>&& src ) noexcept;
    CRenderTargetArray& operator=(CRenderTargetArray &&) noexcept;

    /// @brief initialize the render target array (including depth) with the given dimensions and buffer formats.
    /// Creates VkRenderPasses for color and depth only passes (for the purpose of creating the pipeline).  Creates the pipeline referencing these buffers (and one for depth only rendering).
    /// @return true if successful
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat = TextureFormat::D24_UNORM_S8_UINT, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE, std::span<const TextureFormat> ResolveFormats = {}/*default no resolve*/);
    /// @brief initialize the render target array with the given dimensions and buffer formats.  Creates render passes for the purpose of creating the pipeline.  Pipeline is initialized with the depth buffer passed in inheritDepth parameter.  If Present is true will write output to the backbuffer (with resolve as appropriate)
    /// Creates VkRenderPasses for color and depth only passes.
    /// @return true if successful
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const CRenderTargetArray<T_NUM_BUFFERS>& inheritDepth, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE);
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const std::array<const TextureVulkan*, T_NUM_BUFFERS>& inheritDepth, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE);
    /// @brief initialize the render target array with the given dimensions and buffer formats.  DOES take ownership of the passed in render passes.  Because render passes could have a mix of msaa settings  take a span for each color buffer.
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, VkRenderPass RenderPass, VkRenderPass RenderPassDepthOnly, std::span<const VkSampleCountFlagBits> Msaa = {}, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE);
    /// @brief initialize the render target array using the vulkan swapchain pipeline and swapchain resolution/format.  Use as a helper to render to the swapchain  
    bool InitializeFromSwapchain( Vulkan* pVulkan );

    /// @brief Set the clear colors for all the render target buffers (all targets set to the same set of clear colors)
    void SetClearColors(const std::span<const VkClearColorValue> clearColors);

    void Release();
    const CRenderTarget& operator[](size_t idx) const { return m_RenderTargets[idx]; }
    CRenderTarget& operator[](size_t idx)             { return m_RenderTargets[idx]; }

    /// The Render Pass (shared between buffers).
    VkRenderPass  m_RenderPass = VK_NULL_HANDLE;
    /// Depth only Render Pass (shared between buffers)
    VkRenderPass  m_RenderPassDepthOnly = VK_NULL_HANDLE;
    /// The render target buffers
    std::array<CRenderTarget, T_NUM_BUFFERS> m_RenderTargets;
    /// Set if the framebuffers in m_RenderTargets are owned (created) by CRenderTargetArray or are owned externally (eg by @Vulkan in the case of @InitializeFromSwapchain)
    bool m_FramebufferOwner = false;

private:
    Vulkan* m_pVulkan = nullptr;
};

//=============================================================================
// CRenderTargetArray template implementation
//=============================================================================

template<uint32_t T_NUM_BUFFERS>
CRenderTargetArray<T_NUM_BUFFERS>& CRenderTargetArray<T_NUM_BUFFERS>::operator=( CRenderTargetArray<T_NUM_BUFFERS>&& src ) noexcept
{
    if (this != &src)
    {
        m_RenderPass = src.m_RenderPass;
        src.m_RenderPass = VK_NULL_HANDLE;
        m_RenderPassDepthOnly = src.m_RenderPassDepthOnly;
        src.m_RenderPassDepthOnly = VK_NULL_HANDLE;
        m_RenderTargets = std::move( src.m_RenderTargets );
        m_FramebufferOwner = src.m_FramebufferOwner;
        src.m_FramebufferOwner = false;
        m_pVulkan = src.m_pVulkan;
        src.m_pVulkan = nullptr;
    }
    return *this;
}

template<uint32_t T_NUM_BUFFERS>
CRenderTargetArray<T_NUM_BUFFERS>::CRenderTargetArray( CRenderTargetArray<T_NUM_BUFFERS>&& src ) noexcept
{
    *this = std::move(src);
}

template<uint32_t T_NUM_BUFFERS>
CRenderTargetArray<T_NUM_BUFFERS>::~CRenderTargetArray()
{
    Release();
}

template<uint32_t T_NUM_BUFFERS>
void CRenderTargetArray<T_NUM_BUFFERS>::Release()
{
    for (int WhichBuffer = T_NUM_BUFFERS - 1; WhichBuffer >= 0; --WhichBuffer)
        m_RenderTargets[WhichBuffer].Release(m_FramebufferOwner);
    if (m_RenderPassDepthOnly != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_pVulkan->m_VulkanDevice, m_RenderPassDepthOnly, NULL);
    m_RenderPassDepthOnly = VK_NULL_HANDLE;
    if (m_RenderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_pVulkan->m_VulkanDevice, m_RenderPass, NULL);
    m_RenderPass = VK_NULL_HANDLE;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, VkSampleCountFlagBits Msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap, std::span<const TextureFormat> ResolveFormats)
{
    m_pVulkan = pVulkan;

    if (pVRSMap)
    {
        // ... create the render pass...
        if (!pVulkan->CreateRenderPassVRS( pLayerFormats, DepthFormat, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPass, ResolveFormats ))
        {
            LOGE( "Unable to create render pass: %s", pName );
            return false;
        }

        // ... create the depth only render pass...
        if (!pVulkan->CreateRenderPassVRS( {}, DepthFormat, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPassDepthOnly ))
        {
            LOGE( "Unable to create render pass: %s", pName );
            return false;
        }

    }
    else
    {
        // ... create the render pass...
        if (!pVulkan->CreateRenderPass(pLayerFormats, DepthFormat, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPass, ResolveFormats))
        {
            LOGE("Unable to create render pass: %s", pName);
            return false;
        }
        // ... create the depth only render pass...
        if (!pVulkan->CreateRenderPass({}, DepthFormat, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPassDepthOnly))
        {
            LOGE("Unable to create render pass: %s", pName);
            return false;
        }
    }

    // ... create the render targets and framebuffers
    m_FramebufferOwner = true;
    char szName[128];
    uint32_t WhichBuffer = 0;
    for (auto& RenderTarget : m_RenderTargets)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!RenderTarget.Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, Msaa, szName))
        {
            return false;
        }
        if (FilterModes.empty())
            std::fill(RenderTarget.m_FilterMode.begin(), RenderTarget.m_FilterMode.end(), SamplerFilter::Linear);
        else
            RenderTarget.m_FilterMode.assign(std::begin(FilterModes), std::end(FilterModes));
        RenderTarget.InitializeDepth();
        RenderTarget.InitializeColor(ColorTypes);
        if (!ResolveFormats.empty())
            RenderTarget.InitializeResolve(ResolveFormats);
        RenderTarget.InitializeFrameBuffer(m_RenderPass, RenderTarget.m_ColorAttachments, &RenderTarget.m_DepthAttachment, RenderTarget.m_ResolveAttachments, pVRSMap, &RenderTarget.m_FrameBuffer);
        RenderTarget.InitializeFrameBuffer(m_RenderPassDepthOnly, {}, &RenderTarget.m_DepthAttachment, {}, pVRSMap, &RenderTarget.m_FrameBufferDepthOnly);
        pVulkan->SetDebugObjectName(RenderTarget.m_FrameBuffer, szName);
        ++WhichBuffer;
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, VkRenderPass RenderPass, VkRenderPass RenderPassDepthOnly, std::span<const VkSampleCountFlagBits> Msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
{
    m_pVulkan = pVulkan;
    m_RenderPass = RenderPass;
    m_RenderPassDepthOnly = RenderPassDepthOnly;

    // ... create the render targets and framebuffers
    m_FramebufferOwner = true;
    char szName[128];
    uint32_t WhichBuffer = 0;
    for (auto& RenderTarget : m_RenderTargets)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!RenderTarget.Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, Msaa, szName))
        {
            return false;
        }
        if (FilterModes.empty())
            std::fill(RenderTarget.m_FilterMode.begin(), RenderTarget.m_FilterMode.end(), SamplerFilter::Linear);
        else
            RenderTarget.m_FilterMode.assign(std::begin(FilterModes), std::end(FilterModes));
        RenderTarget.InitializeDepth();
        RenderTarget.InitializeColor(ColorTypes);
        if( RenderPass != VK_NULL_HANDLE )
        {
            RenderTarget.InitializeFrameBuffer( RenderPass, RenderTarget.m_ColorAttachments, &RenderTarget.m_DepthAttachment, RenderTarget.m_ResolveAttachments, pVRSMap, &RenderTarget.m_FrameBuffer );
        }
        if( RenderPassDepthOnly != VK_NULL_HANDLE )
        {
            RenderTarget.InitializeFrameBuffer(RenderPassDepthOnly, {}, &RenderTarget.m_DepthAttachment, {}, pVRSMap, &RenderTarget.m_FrameBufferDepthOnly);
        }
        ++WhichBuffer;
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const CRenderTargetArray<T_NUM_BUFFERS>& inheritDepth, VkSampleCountFlagBits Msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
{
    std::array<const TextureVulkan*, T_NUM_BUFFERS> pReferencedDepthTextures;
    std::transform(std::begin(inheritDepth.m_RenderTargets), std::end(inheritDepth.m_RenderTargets), std::begin(pReferencedDepthTextures), [](const CRenderTarget& rt) { return &rt.m_DepthAttachment; });
    return Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, pReferencedDepthTextures, Msaa, pName, ColorTypes, FilterModes, pVRSMap);
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const std::array<const TextureVulkan*, T_NUM_BUFFERS>& inheritDepth, VkSampleCountFlagBits Msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
{
    m_pVulkan = pVulkan;

    if (pVRSMap)
    {
        // ... create the render pass (color only)...
        if (!pVulkan->CreateRenderPassVRS( pLayerFormats, inheritDepth[0]->Format, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPass ))
        {
            LOGE( "Unable to create render pass: %s", pName );
            return false;
        }
    }
    else
    {
        // ... create the render pass (color only)...
        if (!pVulkan->CreateRenderPass(pLayerFormats, inheritDepth[0]->Format, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPass))
        {
            LOGE("Unable to create render pass: %s", pName);
            return false;
        }
    }

    // Create the render target and framebuffers
    m_FramebufferOwner = true;
    char szName[128];
    uint32_t WhichBuffer = 0;
    for (auto& RenderTarget : m_RenderTargets)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!RenderTarget.Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, inheritDepth[WhichBuffer]->Format, Msaa, szName))
        {
            return false;
        }
        if (FilterModes.empty())
            std::fill(RenderTarget.m_FilterMode.begin(), RenderTarget.m_FilterMode.end(), SamplerFilter::Linear);
        else
            RenderTarget.m_FilterMode.assign(std::begin(FilterModes), std::end(FilterModes));
        if (!RenderTarget.InitializeColor(ColorTypes))
        {
            return false;
        }
        if (!RenderTarget.InitializeFrameBuffer(m_RenderPass, RenderTarget.m_ColorAttachments, inheritDepth[WhichBuffer], RenderTarget.m_ResolveAttachments, pVRSMap, &RenderTarget.m_FrameBuffer))
        {
            return false;
        }
        ++WhichBuffer;
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::InitializeFromSwapchain(Vulkan* pVulkan)
{
    m_pVulkan = pVulkan;
    m_FramebufferOwner = false;
    const TextureFormat surfaceFormat = pVulkan->m_SurfaceFormat;
    const TextureFormat depthFormat = pVulkan->m_SwapchainDepth.format;

    size_t WhichFrame = 0;
    for (auto& RenderTarget : m_RenderTargets)
    {
        if (!RenderTarget.Initialize(pVulkan, pVulkan->m_SurfaceWidth, pVulkan->m_SurfaceHeight, { &surfaceFormat, 1 }, depthFormat, VK_SAMPLE_COUNT_1_BIT, "Swapchain"))
            return false;
        if (WhichFrame < pVulkan->m_SwapchainBuffers.size())
            RenderTarget.m_FrameBuffer = pVulkan->m_SwapchainBuffers[WhichFrame].framebuffer;
        else
            RenderTarget.m_FrameBuffer = VK_NULL_HANDLE;
        ++WhichFrame;
    }
    return true;
}


template<uint32_t T_NUM_BUFFERS>
void CRenderTargetArray<T_NUM_BUFFERS>::SetClearColors(const std::span<const VkClearColorValue> clearColors)
{
    for (auto& RenderTarget : m_RenderTargets)
        RenderTarget.SetClearColors(clearColors);
}
