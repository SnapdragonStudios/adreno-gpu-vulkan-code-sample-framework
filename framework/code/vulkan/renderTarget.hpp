//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <assert.h>
#include <array>
#include <span>
#include <algorithm>
#include "graphicsApi/renderTarget.hpp"
#include "vulkan/renderPass.hpp"
#include "texture/vulkan/texture.hpp"
#include "vulkan.hpp"
//#include "TextureFuncts.h"
#include "system/os_common.h"

//=============================================================================
// RenderTarget<Vulkan>
//=============================================================================

/// Container for a single frame render target.
/// Contains multiple color buffers, multiple resolve buffers, and an optional depth buffer.
/// A Framebuffer object is created given a valid render pass is passed on initialization.
template<>
class RenderTarget<Vulkan> final : public RenderTargetBase
{
    // Functions
    RenderTarget(const RenderTarget<Vulkan>&) = delete;
    RenderTarget& operator=(const RenderTarget<Vulkan>&) = delete;

public:
    RenderTarget();
    ~RenderTarget();
    RenderTarget( RenderTarget<Vulkan>&& ) noexcept;
    RenderTarget& operator=( RenderTarget<Vulkan>&& ) noexcept;

    /// @brief Initialize the render target with (potentially) multiple color buffers (including depth and resolve buffers where applicable
    bool Initialize( Vulkan* pVulkan, const RenderTargetInitializeInfo& info, const char* pName, const RenderPass<Vulkan>* renderPass = nullptr, const RenderPass<Vulkan>* renderPassDepthOnly = nullptr );

    /// @brief Initialize the render target with (potentially) multiple color buffers (including depth and resolve buffers where applicable
    /// @param uiWidth 
    /// @param uiHeight 
    /// @param pLayerFormats color buffer formats
    /// @param DepthFormat depth buffer format (optional)
    /// @param pName 
    /// @param TextureTypes initial usage/type of render target (defualts to TT_RENDER_TARGET if empty span)
    /// @param msaa multisampling setting of color buffer (can be one setting, which is copied to all buffers).  Default Sample1 if empty span
    /// @param ResolveTextureFormats resolve buffer formats (if msaa used)
    /// @return true on success
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat = TextureFormat::D24_UNORM_S8_UINT, const char* pName = NULL, const std::span<const TEXTURE_TYPE> TextureTypes = {}, std::span<const Msaa> msaa = {}, const std::span<const TextureFormat> ResolveTextureFormats = {}, const std::span<const SamplerFilter> FilterModes = {});
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat = TextureFormat::D24_UNORM_S8_UINT, Msaa msaa = Msaa::Samples1, const char* pName = NULL)
    {
        return Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, pName, {/* texture type*/}, {&msaa,1});
    }

    /*
    * Helper functions to manually initialize the framebuffers.
    * These functions are intended to be used when the main Initialize() function is called without a render pass.
    * In such cases, the user must explicitly call these functions with a valid render pass to complete framebuffer setup.
    * @param pVulkan : Pointer to the Vulkan context
    * @param renderPass / renderPassDepthOnly : Valid render pass used to create the framebuffer(s)
    * @return true on successful framebuffer creation
    * @note These functions provide manual control over framebuffer creation, useful for advanced or staged initialization flows.
    */
    bool InitializeFrameBuffer(Vulkan* pVulkan, const RenderPass<Vulkan>& renderPass);
    bool InitializeFrameBufferDepthOnly(Vulkan* pVulkan, const RenderPass<Vulkan>& renderPassDepthOnly);

    void Release();

    inline uint32_t GetNumColorLayers() const 
    { 
        return static_cast<uint32_t>(m_pLayerFormats.size());
    }

    /*
    * Get the name of the render target
    * @return std::string reference to the name
    */
    inline const std::string& GetName() const
    {
        return m_Name;
    }

    /*
    * Get the width of the render target
    * @return uint32_t width
    */
    inline uint32_t GetWidth() const
    {
        return m_Width;
    }

    /*
    * Get the height of the render target
    * @return uint32_t height
    */
    inline uint32_t GetHeight() const
    {
        return m_Height;
    }

    /*
    * Get the color layer formats
    * @return const reference to vector of TextureFormat
    */
    inline const std::vector<TextureFormat>& GetLayerFormats() const
    {
        return m_pLayerFormats;
    }

    /*
    * Get the MSAA settings
    * @return const reference to vector of Msaa
    */
    inline const std::vector<Msaa>& GetMsaa() const
    {
        return m_Msaa;
    }

    /*
    * Get the filter modes
    * @return const reference to vector of SamplerFilter
    */
    inline const std::vector<SamplerFilter>& GetFilterModes() const
    {
        return m_FilterMode;
    }

    /*
    * Get the depth format
    * @return TextureFormat depth format
    */
    inline TextureFormat GetDepthFormat() const
    {
        return m_DepthFormat;
    }

    /*
    * Get the color attachments
    * @return const reference to vector of TextureVulkan
    */
    inline const std::vector<TextureVulkan>& GetColorAttachments() const
    {
        return m_ColorAttachments;
    }

    /*
    * Get the clear color values
    * @return const reference to vector of VkClearColorValue
    */
    inline const std::vector<VkClearColorValue>& GetClearColorValues() const
    {
        return m_ClearColorValues;
    }

    /*
    * Get the resolve attachments
    * @return const reference to vector of TextureVulkan
    */
    inline const std::vector<TextureVulkan>& GetResolveAttachments() const
    {
        return m_ResolveAttachments;
    }

    /*
    * Get the depth attachment
    * @return const reference to TextureVulkan
    */
    inline const TextureVulkan& GetDepthAttachment() const
    {
        return m_DepthAttachment;
    }

    /*
    * Get the framebuffer
    * @return const reference to Framebuffer<Vulkan>
    */
    inline const Framebuffer<Vulkan>& GetFrameBuffer() const
    {
        return m_FrameBuffer;
    }

    /*
    * Get the depth-only framebuffer
    * @return const reference to Framebuffer<Vulkan>
    */
    inline const Framebuffer<Vulkan>& GetFrameBufferDepthOnly() const
    {
        return m_FrameBufferDepthOnly;
    }

private:
    template<uint32_t T_NUM_BUFFERS> friend class RenderTargetArray;
    bool InitializeDepth(std::optional<TEXTURE_TYPE> textureType);
    bool InitializeColor(const std::span<const TEXTURE_TYPE> TextureTypes);
    bool InitializeResolve(const std::span<const TextureFormat> ResolveTextureFormats);

    bool CreateFrameBuffer(const RenderPass<Vulkan>& renderPass, const std::span<const TextureVulkan> ColorAttachments, const TextureVulkan* pDepthAttachment, const std::span<const TextureVulkan> ResolveAttachments, const TextureVulkan* pVRSAttachment, Framebuffer<Vulkan>* pFrameBuffer);

    void SetClearColors(const std::span<const VkClearColorValue> clearColors);

    // Attributes
public:
    std::string                 m_Name;

    uint32_t                    m_Width = 0;
    uint32_t                    m_Height = 0;

    std::vector<TextureFormat>  m_pLayerFormats;
    std::vector<Msaa>           m_Msaa;
    std::vector<SamplerFilter>  m_FilterMode;
    TextureFormat               m_DepthFormat = TextureFormat::UNDEFINED;

    // The Color Attachments

    std::vector<TextureVulkan>  m_ColorAttachments;
    std::vector<VkClearColorValue> m_ClearColorValues;

    // The Resolve Attachments
    std::vector<TextureVulkan>  m_ResolveAttachments;

    // The Depth Attachment
    TextureVulkan               m_DepthAttachment;
    TextureVulkan*              m_InheritedDepthAttachment = nullptr; // Note: Not owning

    // The Frame Buffer
    Framebuffer<Vulkan>         m_FrameBuffer;

    // The Frame Buffer (depth only)
    Framebuffer<Vulkan>         m_FrameBufferDepthOnly;

private:
    Vulkan*                     m_pVulkan = nullptr;
};

#if 0
/// Fixed size array of CRenderTargets (eg one per 'frame') that share RenderPass objects
template<uint32_t T_NUM_BUFFERS>
class RenderTargetArray
{
    RenderTargetArray(const RenderTargetArray&) = delete;
    RenderTargetArray& operator=(const RenderTargetArray&) = delete;
public:
    RenderTargetArray() = default;
    ~RenderTargetArray();
    RenderTargetArray( RenderTargetArray<T_NUM_BUFFERS>&& src ) noexcept;
    RenderTargetArray& operator=(RenderTargetArray &&) noexcept;

    /// @brief initialize the render target array (including depth) with the given dimensions and buffer formats.
    /// Creates VkRenderPasses for color and depth only passes (for the purpose of creating the pipeline).  Creates the pipeline referencing these buffers (and one for depth only rendering).
    /// @return true if successful
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat = TextureFormat::D24_UNORM_S8_UINT, Msaa msaa = Msaa::Samples1, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE, std::span<const TextureFormat> ResolveFormats = {}/*default no resolve*/);
    /// @brief initialize the render target array with the given dimensions and buffer formats.  Creates render passes for the purpose of creating the pipeline.  Pipeline is initialized with the depth buffer passed in inheritDepth parameter.  If Present is true will write output to the backbuffer (with resolve as appropriate)
    /// Creates VkRenderPasses for color and depth only passes.
    /// @return true if successful
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const RenderTargetArray<T_NUM_BUFFERS>& inheritDepth, Msaa msaa = Msaa::Samples1, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE);
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const std::array<const TextureVulkan*, T_NUM_BUFFERS>& inheritDepth, Msaa msaa = Msaa::Samples1, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE);
    /// @brief initialize the render target array with the given dimensions and buffer formats.  DOES take ownership of the passed in render passes.  Because render passes could have a mix of msaa settings  take a span for each color buffer.
    bool Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, RenderPass<Vulkan> renderPass, RenderPass<Vulkan> renderPassDepthOnly, std::span<const Msaa> msaa = {}, const char* pName = NULL, const std::span<const TEXTURE_TYPE> ColorTypes = {}, const std::span<const SamplerFilter> FilterModes = {}, const TextureVulkan* pVRSMap = VK_NULL_HANDLE);
    /// @brief initialize the render target array using the vulkan swapchain pipeline and swapchain resolution/format.  Use as a helper to render to the swapchain  
    bool InitializeFromSwapchain( Vulkan* pVulkan );

    /// @brief Set the clear colors for all the render target buffers (all targets set to the same set of clear colors)
    void SetClearColors(const std::span<const VkClearColorValue> clearColors);

    void Release();
    const RenderTarget<Vulkan>& operator[](size_t idx) const { return m_RenderTargets[idx]; }
    RenderTarget<Vulkan>& operator[](size_t idx)             { return m_RenderTargets[idx]; }

    /// The Render Pass (shared between buffers).
    RenderPass<Vulkan>                          m_RenderPass;
    /// Depth only Render Pass (shared between buffers)
    RenderPass<Vulkan>                          m_RenderPassDepthOnly;
    /// The render target buffers
    std::array<RenderTarget<Vulkan>, T_NUM_BUFFERS>    m_RenderTargets;

private:
    Vulkan* m_pVulkan = nullptr;
};

//=============================================================================
// RenderTargetArray template implementation
//=============================================================================

template<uint32_t T_NUM_BUFFERS>
RenderTargetArray<T_NUM_BUFFERS>& RenderTargetArray<T_NUM_BUFFERS>::operator=( RenderTargetArray<T_NUM_BUFFERS>&& src ) noexcept
{
    if (this != &src)
    {
        m_RenderPass = std::move(src.m_RenderPass);
        m_RenderPassDepthOnly = std::move(src.m_RenderPassDepthOnly);
        m_RenderTargets = std::move( src.m_RenderTargets );
        m_pVulkan = src.m_pVulkan;
        src.m_pVulkan = nullptr;
    }
    return *this;
}

template<uint32_t T_NUM_BUFFERS>
RenderTargetArray<T_NUM_BUFFERS>::RenderTargetArray( RenderTargetArray<T_NUM_BUFFERS>&& src ) noexcept
{
    *this = std::move(src);
}

template<uint32_t T_NUM_BUFFERS>
RenderTargetArray<T_NUM_BUFFERS>::~RenderTargetArray()
{
    Release();
}

template<uint32_t T_NUM_BUFFERS>
void RenderTargetArray<T_NUM_BUFFERS>::Release()
{
    for (int WhichBuffer = T_NUM_BUFFERS - 1; WhichBuffer >= 0; --WhichBuffer)
        m_RenderTargets[WhichBuffer].Release();
    m_RenderPassDepthOnly = {};
    m_RenderPass = {};
}

template<uint32_t T_NUM_BUFFERS>
bool RenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, Msaa msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap, std::span<const TextureFormat> ResolveFormats)
{
    m_pVulkan = pVulkan;

    // Allow 0 Msaa (same as 1 bit)
    if (msaa < Msaa::Samples1)
        msaa = Msaa::Samples1;

    if (pVRSMap)
    {
        // ... create the render pass...
        if (!pVulkan->CreateRenderPassVRS( pLayerFormats, DepthFormat, msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, m_RenderPass, ResolveFormats ))
        {
            LOGE( "Unable to create render pass: %s", pName );
            return false;
        }

        // ... create the depth only render pass...
        if (!pVulkan->CreateRenderPassVRS( {}, DepthFormat, msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, m_RenderPassDepthOnly ))
        {
            LOGE( "Unable to create render pass: %s", pName );
            return false;
        }

    }
    else
    {
        // ... create the render pass...
        if (!pVulkan->CreateRenderPass(pLayerFormats, DepthFormat, msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, m_RenderPass, ResolveFormats))
        {
            LOGE("Unable to create render pass: %s", pName);
            return false;
        }
        // ... create the depth only render pass...
        if (!pVulkan->CreateRenderPass({}, DepthFormat, msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, m_RenderPassDepthOnly))
        {
            LOGE("Unable to create render pass: %s", pName);
            return false;
        }
    }

    // ... create the render targets and framebuffers
    char szName[128];
    uint32_t WhichBuffer = 0;

    RenderTargetInitializeInfo renderTargetInitInfo{
        .Width = uiWidth,
        .Height = uiHeight,
        .LayerFormats = pLayerFormats,
        .DepthFormat = DepthFormat,
        .TextureTypes = ColorTypes,
        .Msaa = {&msaa, 1},
        .ResolveTextureFormats = ResolveFormats,
        .FilterModes = FilterModes
    };
    for (auto& RenderTarget : m_RenderTargets)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, (int)m_RenderTargets.size() );  szName[sizeof(szName) - 1] = 0;
        if (!RenderTarget.Initialize(pVulkan, renderTargetInitInfo, szName, &m_RenderPass, &m_RenderPassDepthOnly))
        {
            return false;
        }
        //RenderTarget.CreateFrameBuffer(m_RenderPass, RenderTarget.m_ColorAttachments, &RenderTarget.m_DepthAttachment, RenderTarget.m_ResolveAttachments, pVRSMap, &RenderTarget.m_FrameBuffer);
        //RenderTarget.CreateFrameBuffer(m_RenderPassDepthOnly, {}, &RenderTarget.m_DepthAttachment, {}, pVRSMap, &RenderTarget.m_FrameBufferDepthOnly);
        pVulkan->SetDebugObjectName(RenderTarget.m_FrameBuffer, szName);
        ++WhichBuffer;
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool RenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, TextureFormat DepthFormat, RenderPass<Vulkan> renderPass, RenderPass<Vulkan> RenderPassDepthOnly, std::span<const Msaa> msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
{
    m_pVulkan = pVulkan;
    m_RenderPass = std::move(renderPass);
    m_RenderPassDepthOnly = std::move(RenderPassDepthOnly);

    // ... create the render targets and framebuffers
    char szName[128];
    uint32_t WhichBuffer = 0;

    const RenderTargetInitializeInfo renderTargetInitInfo {
        .Width = uiWidth,
        .Height = uiHeight,
        .LayerFormats = pLayerFormats,
        .DepthFormat = DepthFormat,
        .Msaa = msaa,
        .FilterModes = FilterModes
    };

    for (auto& RenderTarget : m_RenderTargets)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!RenderTarget.Initialize(pVulkan, renderTargetInitInfo, szName, &m_RenderPass, &m_RenderPassDepthOnly))
        {
            return false;
        }
        ++WhichBuffer;
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool RenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const RenderTargetArray<T_NUM_BUFFERS>& inheritDepth, Msaa msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
{
    std::array<const TextureVulkan*, T_NUM_BUFFERS> pReferencedDepthTextures;
    std::transform(std::begin(inheritDepth.m_RenderTargets), std::end(inheritDepth.m_RenderTargets), std::begin(pReferencedDepthTextures), [](const RenderTarget<Vulkan>& rt) { return &rt.m_DepthAttachment; });
    return Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, pReferencedDepthTextures, msaa, pName, ColorTypes, FilterModes, pVRSMap);
}

template<uint32_t T_NUM_BUFFERS>
bool RenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const std::span<const TextureFormat> pLayerFormats, const std::array<const TextureVulkan*, T_NUM_BUFFERS>& inheritDepth, Msaa msaa, const char* pName, const std::span<const TEXTURE_TYPE> ColorTypes, const std::span<const SamplerFilter> FilterModes, const TextureVulkan* pVRSMap)
{
    m_pVulkan = pVulkan;

    if (pVRSMap)
    {
        // ... create the render pass (color only)...
        if (!pVulkan->CreateRenderPassVRS( pLayerFormats, inheritDepth[0]->Format, msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, m_RenderPass ))
        {
            LOGE( "Unable to create render pass: %s", pName );
            return false;
        }
    }
    else
    {
        // ... create the render pass (color only)...
        if (!pVulkan->CreateRenderPass(pLayerFormats, inheritDepth[0]->Format, msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, m_RenderPass))
        {
            LOGE("Unable to create render pass: %s", pName);
            return false;
        }
    }

    // Create the render target and framebuffers
    char szName[128];
    uint32_t WhichBuffer = 0;
    for (auto& RenderTarget : m_RenderTargets)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!RenderTarget.Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, inheritDepth[WhichBuffer]->Format, msaa, szName))
        {
            return false;
        }
        if (FilterModes.empty())
            std::fill(RenderTarget.m_FilterMode.begin(), RenderTarget.m_FilterMode.end(), SamplerFilter::Linear);
        else
            RenderTarget.m_FilterMode.assign(std::begin(FilterModes), std::end(FilterModes));
        if (!RenderTarget.CreateFrameBuffer(m_RenderPass, RenderTarget.m_ColorAttachments, inheritDepth[WhichBuffer], RenderTarget.m_ResolveAttachments, pVRSMap, &RenderTarget.m_FrameBuffer))
        {
            return false;
        }
        ++WhichBuffer;
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool RenderTargetArray<T_NUM_BUFFERS>::InitializeFromSwapchain(Vulkan* pVulkan)
{
    m_pVulkan = pVulkan;
    const TextureFormat surfaceFormat = pVulkan->m_SurfaceFormat;
    const TextureFormat depthFormat = pVulkan->m_SwapchainDepth.format;

    for(size_t whichFrame = 0; whichFrame< pVulkan->m_SwapchainBuffers.size() && whichFrame < T_NUM_BUFFERS; ++whichFrame)
    {
        RenderTarget<Vulkan>& renderTarget = m_RenderTargets[whichFrame];
        if (!renderTarget.Initialize( pVulkan, pVulkan->m_SurfaceWidth, pVulkan->m_SurfaceHeight, {&surfaceFormat, 1}, depthFormat, Msaa::Samples1, "Swapchain" ))
            return false;
        renderTarget.m_FrameBuffer = pVulkan->m_SwapchainBuffers[whichFrame].framebuffer;
    }
    return true;
}


template<uint32_t T_NUM_BUFFERS>
void RenderTargetArray<T_NUM_BUFFERS>::SetClearColors(const std::span<const VkClearColorValue> clearColors)
{
    for (auto& renderTarget : m_RenderTargets)
        renderTarget.SetClearColors(clearColors);
}
#endif //0
