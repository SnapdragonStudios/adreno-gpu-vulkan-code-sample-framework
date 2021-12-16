// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <assert.h>
#include <stdlib.h>
#include <sstream>
#include <array>
#include "tcb/span.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include "material/shaderModule.hpp"

#include "system/os_common.h"

// Need the vulkan wrapper
#include "vulkan.hpp"

// All the variables needed to create a uniform buffer
struct Uniform
{
    Uniform() = default;
    Uniform(const Uniform&) = delete;
    Uniform& operator=(const Uniform&) = delete;
    Uniform(Uniform&&) noexcept;
    MemoryVmaAllocatedBuffer<VkBuffer> buf;
    VkDescriptorBufferInfo bufferInfo;
};

template<typename T>
struct UniformT : public Uniform
{
    typedef T type;
};

typedef struct _ShaderInfo
{
    ShaderModule          VertShaderModule;
    ShaderModule          FragShaderModule;
} ShaderInfo;

// Helper Functions
bool LoadShader(Vulkan* pVulkan, AssetManager&, ShaderInfo* pShader, const std::string& VertFilename, const std::string& FragFilename);
void ReleaseShader(Vulkan* pVulkan, ShaderInfo* pShader);

bool CreateUniformBuffer(Vulkan* pVulkan, Uniform* pNewUniform, uint32_t dataSize, const void* const pData = NULL, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
void UpdateUniformBuffer(Vulkan* pVulkan, Uniform* pUniform, uint32_t dataSize, const void* const pNewData);
void ReleaseUniformBuffer(Vulkan* pVulkan, Uniform* pUniform);

template<typename T>
bool CreateUniformBuffer(Vulkan* pVulkan, UniformT<T>& newUniform, const T* const pData = nullptr, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
{
    return CreateUniformBuffer(pVulkan, &newUniform, sizeof(T), pData, usage);
}
template<typename T>
void UpdateUniformBuffer(Vulkan* pVulkan, Uniform& uniform, const T& newData)
{
    return UpdateUniformBuffer(pVulkan, &uniform, sizeof(T), &newData);
}
template<typename T, typename TT>
void UpdateUniformBuffer(Vulkan* pVulkan, UniformT<T>& uniform, const TT& newData)
{
    static_assert(std::is_same<T,TT>::value, "UpdateUniformBuffer, uniform is different type to newData");
    return UpdateUniformBuffer(pVulkan, static_cast<Uniform&>(uniform), newData);
}

/// Helper enum for setting up a VkPipelineColorBlendAttachmentState
enum class BLENDING_TYPE
{
    BT_NONE,
    BT_ALPHA,
    BT_ADDITIVE
};
// Setup a VkPipelineColorBlendAttachmentState with a simplified BLENDING_TYPE
void VulkanSetBlendingType( BLENDING_TYPE WhichType, VkPipelineColorBlendAttachmentState* pBlendState );

//=============================================================================
// Wrap_VkImage
//=============================================================================
class Wrap_VkImage
{
    // Functions
    Wrap_VkImage(const Wrap_VkImage&) = delete;
    Wrap_VkImage& operator=(const Wrap_VkImage&) = delete;
public:
    Wrap_VkImage();
    ~Wrap_VkImage();

    void HardReset();
    bool Initialize(Vulkan* pVulkan, const VkImageCreateInfo& ImageInfo, MemoryManager::MemoryUsage TypeFlag, const char* pName = NULL);
    void Release();

private:

    // Attributes
public:
    std::string             m_Name;

    MemoryVmaAllocatedBuffer<VkImage> m_VmaImage;

private:
    Vulkan *                m_pVulkan;
    VkImageCreateInfo       m_ImageInfo;
    MemoryManager::MemoryUsage m_Usage;
};

//=============================================================================
// Wrap_VkCommandBuffer
//=============================================================================
class Wrap_VkCommandBuffer
{
    // Functions
public:
    Wrap_VkCommandBuffer();
    ~Wrap_VkCommandBuffer();

    void HardReset();
    bool Initialize(Vulkan* pVulkan, const std::string& Name = {}, VkCommandBufferLevel CmdBuffLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY, bool UseComputeCmdPool = false);
    bool Reset();

    // Begin primary command buffer
    bool Begin(VkCommandBufferUsageFlags CmdBuffUsage = 0);
    // Begin secondary command buffer (with inheritance)
    bool Begin(VkFramebuffer FrameBuffer, VkRenderPass RenderPass, bool IsSwapChainRenderPass = false, uint32_t SubPass = 0, VkCommandBufferUsageFlags CmdBuffUsage = 0);

    bool BeginRenderPass(VkRect2D RenderExtent, float MinDepth, float MaxDepth, VkClearColorValue ClearColor, uint32_t NumColorBuffers, bool HasDepth, VkRenderPass RenderPass, bool IsSwapChainRenderPass, VkFramebuffer FrameBuffer, VkSubpassContents SubContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    bool NextSubpass();
    bool EndRenderPass();

    bool End();
    /// @brief Call vkQueueSubmit for this command buffer.
    /// Will submit to the compute or graphics queue (depending on m_UseComputeCmdPool flag).
    /// @param WaitSemaphores semaphores to wait upon before the gpu can execute this command buffer
    /// @param WaitDstStageMasks pipeline stages that the wait occurs on.  MUST have the same number of entries as the WaitSemaphores parameter.
    /// @param SignalSemaphores semaphores to signal when this commandlist finishes
    void QueueSubmit(tcb::span<VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, tcb::span<VkSemaphore> SignalSemaphores);
    /// @brief Simplified QueueSubmit for cases where we are only waiting and signalling one semaphore.
    void QueueSubmit( VkSemaphore WaitSemaphore, VkPipelineStageFlags WaitDstStageMask, VkSemaphore SignalSemaphore )
    {
        typedef tcb::span<VkSemaphore> tSemSpan;
        tSemSpan WaitSemaphores = (WaitSemaphore != VK_NULL_HANDLE) ? tSemSpan{ &WaitSemaphore, 1 } : tSemSpan{};
        tSemSpan SignalSemaphores = (SignalSemaphore != VK_NULL_HANDLE) ? tSemSpan{ &SignalSemaphore, 1 } : tSemSpan{};
        QueueSubmit( WaitSemaphores, tcb::span<const VkPipelineStageFlags>( &WaitDstStageMask, WaitSemaphores.size() ), SignalSemaphores );
    }
    /// @brief Release the Vulkan resources used by this wrapper and cleanup.
    void Release();

private:

    // Attributes
public:
    std::string         m_Name;

    uint32_t            m_NumDrawCalls;
    uint32_t            m_NumTriangles;

    bool                m_IsPrimary;
    bool                m_UseComputeCmdPool;
    VkCommandBuffer     m_VkCommandBuffer;

private:
    Vulkan*             m_pVulkan;
};

//=============================================================================
// CRenderTarget
//=============================================================================
class CRenderTarget
{
    // Functions
    CRenderTarget(const CRenderTarget&) = delete;
    CRenderTarget& operator=(const CRenderTarget&) = delete;
    CRenderTarget(CRenderTarget&&);
    CRenderTarget& operator=(CRenderTarget&&);

public:
    CRenderTarget();
    ~CRenderTarget();

    uint32_t GetNumColorLayers() const { return (uint32_t)m_pLayerFormats.size(); }

    void HardReset();
    bool Initialize( Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat = VK_FORMAT_D24_UNORM_S8_UINT, tcb::span<const VkSampleCountFlagBits> Msaa = {}, const char* pName = NULL );
    bool Initialize( Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat = VK_FORMAT_D24_UNORM_S8_UINT, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL );
private:
    template<uint32_t T_NUM_BUFFERS> friend class CRenderTargetArray;
    bool InitializeDepth();
    bool InitializeColor(const tcb::span<const TEXTURE_TYPE> TextureTypes);
    bool InitializeFrameBuffer(VkRenderPass renderPass, const tcb::span<const VulkanTexInfo> ColorAttachments, const VulkanTexInfo* pDepthAttachment, VkFramebuffer* pFramebuffer);
    void Release();

    // Attributes
public:
    std::string         m_Name;

    uint32_t            m_Width;
    uint32_t            m_Height;

    std::vector<VkFormat> m_pLayerFormats;
    VkFormat            m_DepthFormat;
    std::vector<VkSampleCountFlagBits> m_Msaa;

    // The Color Attachment
    std::vector<VulkanTexInfo> m_ColorAttachments;

    // The Depth Attachment
    VulkanTexInfo		m_DepthAttachment;

    // The Frame Buffer
    VkFramebuffer       m_FrameBuffer;

    // The Frame Buffer (depth only)
    VkFramebuffer       m_FrameBufferDepthOnly;

private:
    Vulkan* m_pVulkan;
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
    /// @brief initialize the render target array (including depth) with the given dimensions and buffer formats.
    /// Creates VkRenderPasses for color and depth only passes (for the purpose of creating the pipeline).  Creates the pipeline constinig these buffers (and one for depth only rendering).
    /// @return true if successful
    bool Initialize( Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat = VK_FORMAT_D24_UNORM_S8_UINT, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL, const tcb::span<const TEXTURE_TYPE> ColorTypes = {} );
    /// @brief initialize the render target array with the given dimensions and buffer formats.  Creates render passes for the purpose of creating the pipeline.  Pipeline is initialized with the depth buffer passed in inheritDepth parameter.
    /// Creates VkRenderPasses for color and depth only passes.
    /// @return true if successful
    bool Initialize( Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, const CRenderTargetArray<T_NUM_BUFFERS>& inheritDepth, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, const char* pName = NULL, const tcb::span<const TEXTURE_TYPE> ColorTypes = {} );
    /// @brief initialize the render target array with the given dimensions and buffer formats.  DOES take ownership of the passed in render passes.  Because render passes could have a mix of msaa settings  take a span for each color buffer.
    bool Initialize( Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat, VkRenderPass RenderPass, VkRenderPass RenderPassDepthOnly, tcb::span<const VkSampleCountFlagBits> Msaa = {}, const char* pName = NULL, const tcb::span<const TEXTURE_TYPE> ColorTypes = {} );

    void Release();
    const CRenderTarget& operator[](size_t idx) const { return m_RenderTargets[idx]; }
    CRenderTarget& operator[](size_t idx)             { return m_RenderTargets[idx]; }

    /// The Render Pass (shared between buffers).
    VkRenderPass  m_RenderPass = VK_NULL_HANDLE;
    /// Depth only Render Pass (shared between buffers)
    VkRenderPass  m_RenderPassDepthOnly = VK_NULL_HANDLE;
    /// The render target buffers
    std::array<CRenderTarget, T_NUM_BUFFERS> m_RenderTargets;

private:
    Vulkan* m_pVulkan = nullptr;
};

//=============================================================================
// CRenderTargetArray template implementation
//=============================================================================
template<uint32_t T_NUM_BUFFERS>
CRenderTargetArray<T_NUM_BUFFERS>::~CRenderTargetArray()
{
    Release();
}

template<uint32_t T_NUM_BUFFERS>
void CRenderTargetArray<T_NUM_BUFFERS>::Release()
{
    for (int WhichBuffer = T_NUM_BUFFERS - 1; WhichBuffer >= 0; --WhichBuffer)
        m_RenderTargets[WhichBuffer].Release();
    if (m_RenderPassDepthOnly != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_pVulkan->m_VulkanDevice, m_RenderPassDepthOnly, NULL);
    m_RenderPassDepthOnly = VK_NULL_HANDLE;
    if (m_RenderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_pVulkan->m_VulkanDevice, m_RenderPass, NULL);
    m_RenderPass = VK_NULL_HANDLE;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat, VkSampleCountFlagBits Msaa, const char* pName, const tcb::span<const TEXTURE_TYPE> ColorTypes)
{
    m_pVulkan = pVulkan;

    // ... create the render pass...
    if (!pVulkan->CreateRenderPass(pLayerFormats, DepthFormat, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPass))
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

    char szName[128];
    for (uint32_t WhichBuffer = 0; WhichBuffer < T_NUM_BUFFERS; ++WhichBuffer)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!m_RenderTargets[WhichBuffer].Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, Msaa, szName))
        {
            return false;
        }
        m_RenderTargets[WhichBuffer].InitializeDepth();
        m_RenderTargets[WhichBuffer].InitializeColor(ColorTypes);
        m_RenderTargets[WhichBuffer].InitializeFrameBuffer(m_RenderPass, m_RenderTargets[WhichBuffer].m_ColorAttachments, &m_RenderTargets[WhichBuffer].m_DepthAttachment, &m_RenderTargets[WhichBuffer].m_FrameBuffer);
        m_RenderTargets[WhichBuffer].InitializeFrameBuffer(m_RenderPassDepthOnly, {}, &m_RenderTargets[WhichBuffer].m_DepthAttachment, &m_RenderTargets[WhichBuffer].m_FrameBufferDepthOnly);
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat, VkRenderPass RenderPass, VkRenderPass RenderPassDepthOnly, tcb::span<const VkSampleCountFlagBits> Msaa, const char* pName, const tcb::span<const TEXTURE_TYPE> ColorTypes)
{
    m_pVulkan = pVulkan;
    m_RenderPass = RenderPass;
    m_RenderPassDepthOnly = RenderPassDepthOnly;

    // ... create the render pass...
    char szName[128];
    for (uint32_t WhichBuffer = 0; WhichBuffer < T_NUM_BUFFERS; ++WhichBuffer)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!m_RenderTargets[WhichBuffer].Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, Msaa, szName))
        {
            return false;
        }
        m_RenderTargets[WhichBuffer].InitializeDepth();
        m_RenderTargets[WhichBuffer].InitializeColor(ColorTypes);
        if( RenderPass != VK_NULL_HANDLE )
        {
            m_RenderTargets[WhichBuffer].InitializeFrameBuffer( RenderPass, m_RenderTargets[WhichBuffer].m_ColorAttachments, &m_RenderTargets[WhichBuffer].m_DepthAttachment, &m_RenderTargets[WhichBuffer].m_FrameBuffer );
        }
        if( RenderPassDepthOnly != VK_NULL_HANDLE )
        {
            m_RenderTargets[WhichBuffer].InitializeFrameBuffer( RenderPassDepthOnly, {}, &m_RenderTargets[WhichBuffer].m_DepthAttachment, &m_RenderTargets[WhichBuffer].m_FrameBufferDepthOnly );
        }
    }
    return true;
}

template<uint32_t T_NUM_BUFFERS>
bool CRenderTargetArray<T_NUM_BUFFERS>::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, const CRenderTargetArray<T_NUM_BUFFERS>& depthBuffer, VkSampleCountFlagBits Msaa, const char* pName, const tcb::span<const TEXTURE_TYPE> ColorTypes)
{
    m_pVulkan = pVulkan;

    // ... create the render pass (color only)...
    if (!pVulkan->CreateRenderPass(pLayerFormats, depthBuffer[0].m_DepthFormat, Msaa, RenderPassInputUsage::Clear, RenderPassOutputUsage::StoreReadOnly, true, RenderPassOutputUsage::StoreReadOnly, &m_RenderPass))
    {
        LOGE("Unable to create render pass: %s", pName);
        return false;
    }

    // Create the render targets
    char szName[128];
    for (uint32_t WhichBuffer = 0; WhichBuffer < T_NUM_BUFFERS; ++WhichBuffer)
    {
        snprintf(szName, sizeof(szName), "%s (Buffer %d of %d)", pName, WhichBuffer + 1, T_NUM_BUFFERS);  szName[sizeof(szName) - 1] = 0;
        if (!m_RenderTargets[WhichBuffer].Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, depthBuffer[WhichBuffer].m_DepthFormat, Msaa, szName))
        {
            return false;
        }
        if (!m_RenderTargets[WhichBuffer].InitializeColor(ColorTypes))
        {
            return false;
        }
        if (!m_RenderTargets[WhichBuffer].InitializeFrameBuffer(m_RenderPass, m_RenderTargets[WhichBuffer].m_ColorAttachments, &depthBuffer[WhichBuffer].m_DepthAttachment, &m_RenderTargets[WhichBuffer].m_FrameBuffer))
        {
            return false;
        }
    }
    return true;
}


