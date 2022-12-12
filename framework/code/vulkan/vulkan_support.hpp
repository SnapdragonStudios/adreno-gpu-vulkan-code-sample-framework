//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
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

class CRenderTarget;

/// All the variables needed to create a uniform buffer
struct Uniform
{
    Uniform() = default;
    Uniform(const Uniform&) = delete;
    Uniform& operator=(const Uniform&) = delete;
    Uniform(Uniform&&) noexcept;
    MemoryVmaAllocatedBuffer<VkBuffer> buf;
    VkDescriptorBufferInfo bufferInfo;
};

/// @brief Uniform buffer helper template
/// @tparam T struct type that is contained in the Uniform
template<typename T>
struct UniformT : public Uniform
{
    typedef T type;
};

/// Uniform buffer array that can be updated every frame without stomping the in-flight uniform buffers.
template<size_t T_NUM_BUFFERS>
struct UniformArray
{
    UniformArray() = default;
    UniformArray(const UniformArray&) = delete;
    UniformArray& operator=(const UniformArray&) = delete;
    UniformArray(UniformArray&&) noexcept { assert(0); }
    std::array<MemoryVmaAllocatedBuffer<VkBuffer>, T_NUM_BUFFERS> buf;
    std::array<VkBuffer, T_NUM_BUFFERS> vkBuffers{};  ///< copy of VkBuffer handles (for easy use in bindings etc)
    constexpr size_t size() const { return T_NUM_BUFFERS; }
};

/// @brief UniformArray buffer helper template
/// @tparam T struct type that is contained in the Uniform
template<typename T, size_t T_NUM_BUFFERS>
struct UniformArrayT : public UniformArray<T_NUM_BUFFERS>
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

bool CreateUniformBuffer(Vulkan* pVulkan, Uniform* pNewUniform, size_t dataSize, const void* const pData = NULL, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
void UpdateUniformBuffer(Vulkan* pVulkan, Uniform* pUniform, size_t dataSize, const void* const pNewData);
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
    static_assert(std::is_same<T, TT>::value, "UpdateUniformBuffer, uniform is different type to newData");
    return UpdateUniformBuffer(pVulkan, static_cast<Uniform&>(uniform), newData);
}

bool CreateUniformBuffer(Vulkan* pVulkan, MemoryVmaAllocatedBuffer<VkBuffer>& rNewUniformBuffer, size_t dataSize, const void* const pData, VkBufferUsageFlags usage);
void UpdateUniformBuffer(Vulkan* pVulkan, MemoryVmaAllocatedBuffer<VkBuffer>& rUniform, size_t dataSize, const void* const pNewData);
void ReleaseUniformBuffer(Vulkan* pVulkan, MemoryVmaAllocatedBuffer<VkBuffer>& rUniform);

template<size_t T_NUM_BUFFERS>
bool CreateUniformBuffer(Vulkan* pVulkan, UniformArray<T_NUM_BUFFERS>& rNewUniform, size_t dataSize, const void* const pData = NULL, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
{
    for (size_t i = 0; i < T_NUM_BUFFERS; ++i)
    {
        if (!CreateUniformBuffer(pVulkan, rNewUniform.buf[i], dataSize, pData, usage))
            return false;
        rNewUniform.vkBuffers[i] = rNewUniform.buf[i].GetVkBuffer();
    }
    return true;
}
template<size_t T_NUM_BUFFERS>
void UpdateUniformBuffer(Vulkan* pVulkan, UniformArray<T_NUM_BUFFERS>& rUniform, size_t dataSize, const void* const pNewData, uint32_t bufferIdx)
{
    UpdateUniformBuffer(pVulkan, rUniform.buf[bufferIdx], dataSize, pNewData);
}
template<size_t T_NUM_BUFFERS>
void ReleaseUniformBuffer(Vulkan* pVulkan, UniformArray<T_NUM_BUFFERS>& rUniform)
{
    for (size_t i = 0; i < T_NUM_BUFFERS; ++i)
    {
        rUniform.vkBuffers[i] = VK_NULL_HANDLE;
        ReleaseUniformBuffer(pVulkan, rUniform.buf[i]);
    }
}

template<typename T, size_t T_NUM_BUFFERS>
bool CreateUniformBuffer(Vulkan* pVulkan, UniformArrayT<T, T_NUM_BUFFERS>& newUniform, const T* const pData = nullptr, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
{
    return CreateUniformBuffer(pVulkan, newUniform, sizeof(T), pData, usage);
}
template<typename T, size_t T_NUM_BUFFERS>
void UpdateUniformBuffer(Vulkan* pVulkan, UniformArrayT<T, T_NUM_BUFFERS>& uniform, const T& newData, uint32_t bufferIdx)
{
    return UpdateUniformBuffer(pVulkan, uniform, sizeof(T), &newData, bufferIdx);
}
template<typename T, typename TT, size_t T_NUM_BUFFERS>
void UpdateUniformBuffer(Vulkan* pVulkan, UniformArrayT<T, T_NUM_BUFFERS>& uniform, const TT& newData, uint32_t bufferIdx)
{
    static_assert(std::is_same<T,TT>::value, "UpdateUniformBuffer, uniform is different type to newData");
    return UpdateUniformBuffer(pVulkan, static_cast<UniformArray<T_NUM_BUFFERS>&>(uniform), newData, bufferIdx);
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

    // Need access to the image info for sizes and format
    // Rather than make getters make this guy public.
    VkImageCreateInfo       m_ImageInfo;

private:
    Vulkan *                m_pVulkan;
    MemoryManager::MemoryUsage m_Usage;
};


///
/// Wrapper around VkCommandBuffer.
/// Simplifies creation, use and destruction of VkCommandBuffer.
/// 
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

    bool BeginRenderPass(VkRect2D RenderExtent, float MinDepth, float MaxDepth, const tcb::span<const VkClearColorValue> ClearColors, uint32_t NumColorBuffers, bool HasDepth, VkRenderPass RenderPass, bool IsSwapChainRenderPass, VkFramebuffer FrameBuffer, VkSubpassContents SubContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    bool BeginRenderPass(const CRenderTarget& renderTarget, VkRenderPass RenderPass, VkSubpassContents SubContents);
    bool NextSubpass( VkSubpassContents SubContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS );
    bool EndRenderPass();

    bool End();
    /// @brief Call vkQueueSubmit for this command buffer.
    /// Will submit to the compute or graphics queue (depending on m_UseComputeCmdPool flag).
    /// @param WaitSemaphores semaphores to wait upon before the gpu can execute this command buffer
    /// @param WaitDstStageMasks pipeline stages that the wait occurs on.  MUST have the same number of entries as the WaitSemaphores parameter.
    /// @param SignalSemaphores semaphores to signal when this commandlist finishes
    /// @param CompletionFence fence to set when the commandlist finishes (for CPU syncronization)
    void QueueSubmit(const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, VkFence CompletionFence = VK_NULL_HANDLE) const;
    /// @brief Simplified QueueSubmit for cases where we are only waiting and signalling one semaphore.
    void QueueSubmit( VkSemaphore WaitSemaphore, VkPipelineStageFlags WaitDstStageMask, VkSemaphore SignalSemaphore, VkFence CompletionFence = VK_NULL_HANDLE ) const
    {
        typedef tcb::span<VkSemaphore> tSemSpan;
        tSemSpan WaitSemaphores = (WaitSemaphore != VK_NULL_HANDLE) ? tSemSpan{ &WaitSemaphore, 1 } : tSemSpan{};
        tSemSpan SignalSemaphores = (SignalSemaphore != VK_NULL_HANDLE) ? tSemSpan{ &SignalSemaphore, 1 } : tSemSpan{};
        QueueSubmit( WaitSemaphores, tcb::span<const VkPipelineStageFlags>( &WaitDstStageMask, WaitSemaphores.size() ), SignalSemaphores, CompletionFence );
    }
    template<typename T_SEMA_CONTAINER, typename T_STAGEFLAGS_CONTAINER>
    void QueueSubmitT(T_SEMA_CONTAINER WaitSemaphores, T_STAGEFLAGS_CONTAINER WaitDstStageMasks, T_SEMA_CONTAINER SignalSemaphores, VkFence CompletionFence = VK_NULL_HANDLE)
    {
        QueueSubmit( {WaitSemaphores}, {WaitDstStageMasks}, {SignalSemaphores}, CompletionFence );
    }
    /// @brief Call vkQueueSubmit and vkQueuePresentKHR for the given 'frame'
    /// Call when this is the last (or only) commandlist to be submitted for the current frame.
    /// @param CurrentVulkanBuffer
    void QueueSubmit( const Vulkan::BufferIndexAndFence& CurrentVulkanBuffer, VkSemaphore renderCompleteSemaphore ) const;

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
