//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <assert.h>
#include <stdlib.h>
#include <sstream>
#include <array>
#include <span>
#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include "texture/vulkan/texture.hpp"
#include "material/vulkan/shaderModule.hpp"

#include "system/os_common.h"

// Need the vulkan wrapper
#include "vulkan.hpp"

// Forward declarations
class TimerPoolBase;
class CRenderTarget;
typedef uint64_t VkFlags64;
typedef VkFlags64 VkPipelineStageFlags2;
typedef VkPipelineStageFlags2 VkPipelineStageFlags2KHR;

//=============================================================================
// Structures, Enums and Typedefs
//=============================================================================

struct ShaderInfo
{
    ShaderModuleT<Vulkan>  VertShaderModule;
    ShaderModuleT<Vulkan>  FragShaderModule;
};

/// Helper enum for setting up a VkPipelineColorBlendAttachmentState
enum class BLENDING_TYPE
{
    BT_NONE,
    BT_ALPHA,
    BT_ADDITIVE
};

typedef std::function<void(uint32_t width, uint32_t height, TextureFormat format, uint32_t span, const void* data)> tDumpImageOutputFn;

//=============================================================================
// Helpers
//=============================================================================

// Simplified shader management
bool LoadShader(Vulkan* pVulkan, AssetManager&, ShaderInfo* pShader, const std::string& VertFilename, const std::string& FragFilename);
void ReleaseShader(Vulkan* pVulkan, ShaderInfo* pShader);

// Setup a VkPipelineColorBlendAttachmentState with a simplified BLENDING_TYPE
void VulkanSetBlendingType( BLENDING_TYPE WhichType, VkPipelineColorBlendAttachmentState* pBlendState );

// Extracts pixel data from the target image, which can be manipuled via outputFunction
// Assume targetImage has transfer capabilities (it will be used as transfer src)
bool DumpImagePixelData(
    Vulkan& vulkan,
    VkImage targetImage,
    TextureFormat targetFormat,
    uint32_t targetWidth,
    uint32_t targetHeight,
    bool dumpColor,
    uint32_t mipLevel,
    uint32_t arrayLayer,
    const tDumpImageOutputFn& outputFunction);

#if 0
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

    bool Initialize(Vulkan* pVulkan, const VkImageCreateInfo& ImageInfo, MemoryUsage TypeFlag, const char* pName = nullptr);
    void Release();

    const auto& GetImageInfo() const { return m_ImageInfo; }

    // Attributes
public:
    std::string                             m_Name;
    MemoryAllocatedBuffer<Vulkan, VkImage>  m_VmaImage;

private:
    Vulkan *                                m_pVulkan = nullptr;
    MemoryUsage                             m_Usage { MemoryUsage::Unknown };
    VkImageCreateInfo                       m_ImageInfo{};
};
#endif

///
/// Wrapper around VkSemaphore.
/// Simplifies creation, use and destruction of VkSemaphore.
/// Handles timeline and 'regular' GPU semaphores along with externally created semaphores (eg swapchain etc)
/// 
class Wrap_VkSemaphore
{
public:
    // Functions
    Wrap_VkSemaphore(const Wrap_VkSemaphore&) = delete;
    Wrap_VkSemaphore& operator=(const Wrap_VkSemaphore&) = delete;
    Wrap_VkSemaphore(Wrap_VkSemaphore&&) noexcept;
    Wrap_VkSemaphore& operator=(Wrap_VkSemaphore&&) noexcept;

    Wrap_VkSemaphore();
    ~Wrap_VkSemaphore();

    bool Initialize(Vulkan& vulkan, const char* pName = nullptr);
    bool InitializeTimeline(Vulkan& vulkan, uint64_t initialValue, const char* pName = nullptr);
    bool InitializeExternal(VkSemaphore externalBinarySemaphore, const char* pName = nullptr);
    void Release(Vulkan& vulkan);

    VkSemaphore GetVkSemaphore() const { return m_Semaphore; }
    bool IsTimelineSemaphore() const { return m_TimelineSemaphore; }

private:
    VkSemaphore                         m_Semaphore = VK_NULL_HANDLE;
    std::string                         m_Name;
    bool                                m_TimelineSemaphore = false;
    bool                                m_IsOwned = false;  // only set if we own the m_Semaphore (was not passed in to Initialize via externalSemaphore).
};

///
/// Helpers for simplifying semaphore signalling and writing (timeline and binary semaphores).
/// 
class SemaphoreState
{
    SemaphoreState(const SemaphoreState&) = delete;
    SemaphoreState& operator=(const SemaphoreState&) = delete;
protected:
    SemaphoreState(const Wrap_VkSemaphore& _semaphore, VkPipelineStageFlags2KHR _stageMask) noexcept : semaphore(_semaphore), stageMask(_stageMask) {}
    SemaphoreState(const Wrap_VkSemaphore& _semaphore, VkPipelineStageFlags2KHR _stageMask, uint64_t _timelineValue) noexcept : semaphore(_semaphore), stageMask(_stageMask), value(_timelineValue) {}
public:
    const Wrap_VkSemaphore&         semaphore;
    const VkPipelineStageFlags2KHR  stageMask;
    const uint64_t                  value = 0;
};

class SemaphoreSignal final : public SemaphoreState
{
public:
    SemaphoreSignal(const Wrap_VkSemaphore& _semaphore, VkPipelineStageFlags2KHR _signalStageMask) noexcept : SemaphoreState(_semaphore, _signalStageMask) { assert(!semaphore.IsTimelineSemaphore()); }
    SemaphoreSignal(const Wrap_VkSemaphore& _semaphore, VkPipelineStageFlags2KHR _signalStageMask, uint64_t _waitValue) noexcept : SemaphoreState(_semaphore, _signalStageMask, _waitValue) { assert(semaphore.IsTimelineSemaphore()); }
};

class SemaphoreWait final : public SemaphoreState
{
public:
    SemaphoreWait(const Wrap_VkSemaphore& _semaphore, VkPipelineStageFlags2KHR _waitStageMask) noexcept : SemaphoreState(_semaphore, _waitStageMask) { assert(!semaphore.IsTimelineSemaphore()); }
    SemaphoreWait(const Wrap_VkSemaphore& _semaphore, VkPipelineStageFlags2KHR _waitStageMask, uint64_t _waitValue) noexcept : SemaphoreState(_semaphore, _waitStageMask, _waitValue) { assert(semaphore.IsTimelineSemaphore()); }
    explicit SemaphoreWait(const SemaphoreSignal& signalWeAreWaitingFor) noexcept : SemaphoreState(signalWeAreWaitingFor.semaphore, signalWeAreWaitingFor.stageMask, signalWeAreWaitingFor.value) {}
};

///
/// Wrapper around VkSubmitInfo.
/// Contains all the data, semaphores etc for VkSubmitInfo to make it easier to create one of these and sumbit later in the (cpu) frame.
/// 
class Wrap_VkSubmitInfo
{
public:
    Wrap_VkSubmitInfo(VkCommandBuffer CommandBuffer, std::vector<VkSemaphore> WaitSemaphores, std::vector<VkPipelineStageFlags> WaitDstStageMasks, std::vector<VkSemaphore> SignalSemaphores, uint32_t QueueIndex, VkFence CompletedFence) noexcept
        : m_SubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO }
        , m_QueueIndex(QueueIndex)
        , m_CompletedFence(CompletedFence)
        , m_SubmitInfoTimeline{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO }
        , m_CommandBuffer(CommandBuffer)
        , m_WaitSemaphores(std::move(WaitSemaphores))
        , m_WaitDstStageMasks(std::move(WaitDstStageMasks))
        , m_SignalSemaphores(std::move(SignalSemaphores))
    {
        assert(m_WaitSemaphores.size() == m_WaitDstStageMasks.size());  // expects wait semaphores to have a corresponding mask
        assert(m_CommandBuffer != VK_NULL_HANDLE);
        CreateSubmitInfo(false);
    }

    Wrap_VkSubmitInfo(VkCommandBuffer CommandBuffer, const std::span<const SemaphoreWait> WaitSemaphores, const std::span<const SemaphoreSignal> SignalSemaphores, uint32_t QueueIndex, VkFence CompletedFence) noexcept
        : m_SubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr }
        , m_QueueIndex(QueueIndex)
        , m_CompletedFence(CompletedFence)
        , m_SubmitInfoTimeline{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO }
        , m_CommandBuffer(CommandBuffer)
    {
        m_WaitSemaphores.reserve(WaitSemaphores.size());
        m_WaitDstStageMasks.reserve(WaitSemaphores.size());
        m_WaitSemaphoreValues.reserve(SignalSemaphores.size());
        bool hasTimeline = false;
        for (auto& sema : WaitSemaphores)
        {
            assert( (sema.stageMask & 0xffffffffULL) == sema.stageMask ); // we need to support VkSubmitInfo2 (from VK_KHR_synchronization2 or Vulkan 1.3) if we need 64bit stageMasks 
            m_WaitSemaphores.push_back(sema.semaphore.GetVkSemaphore());
            m_WaitDstStageMasks.push_back((VkPipelineStageFlags) sema.stageMask);
            m_WaitSemaphoreValues.push_back(sema.value);
            if (sema.semaphore.IsTimelineSemaphore())
                hasTimeline = true;
        }
        m_SignalSemaphores.reserve(SignalSemaphores.size());
        m_SignalSemaphoreValues.reserve(SignalSemaphores.size());
        for (auto& sema : SignalSemaphores)
        {
            m_SignalSemaphores.push_back(sema.semaphore.GetVkSemaphore());
            m_SignalSemaphoreValues.push_back(sema.value);
            if (sema.semaphore.IsTimelineSemaphore())
                hasTimeline = true;
        }
        CreateSubmitInfo(hasTimeline);
    }

    void CreateSubmitInfo(bool hasTimeline)
    {
        m_SubmitInfo.commandBufferCount = 1;
        m_SubmitInfo.pCommandBuffers = &m_CommandBuffer;

        if (!m_WaitSemaphores.empty())
        {
            m_SubmitInfo.waitSemaphoreCount = (uint32_t)m_WaitSemaphores.size();
            m_SubmitInfo.pWaitSemaphores = m_WaitSemaphores.data();
            m_SubmitInfo.pWaitDstStageMask = m_WaitDstStageMasks.data();
        }

        if (!m_SignalSemaphores.empty())
        {
            m_SubmitInfo.signalSemaphoreCount = (uint32_t)m_SignalSemaphores.size();
            m_SubmitInfo.pSignalSemaphores = m_SignalSemaphores.data();
        }

        if (hasTimeline)
        {
            if (!m_WaitSemaphoreValues.empty())
            {
                m_SubmitInfoTimeline.waitSemaphoreValueCount = m_SubmitInfo.waitSemaphoreCount;
                m_SubmitInfoTimeline.pWaitSemaphoreValues = m_WaitSemaphoreValues.data();
            }
            if (!m_SignalSemaphoreValues.empty())
            {
                m_SubmitInfoTimeline.signalSemaphoreValueCount = m_SubmitInfo.signalSemaphoreCount;
                m_SubmitInfoTimeline.pSignalSemaphoreValues = m_SignalSemaphoreValues.data();
            }
            // Attach the timeline information only if we have timeline semaphores (spec says the driver should ignore for all non-timeline semaphores but we want to be sure!).
            m_SubmitInfo.pNext = &m_SubmitInfoTimeline;
        }
        else
        {
            m_WaitSemaphoreValues.clear();
            m_SignalSemaphoreValues.clear();
            // No timeline semaphores so dont attach the timeline data.
            m_SubmitInfo.pNext = nullptr;
        }
    }

    Wrap_VkSubmitInfo(Wrap_VkSubmitInfo&& src) noexcept
        : m_WaitSemaphores(std::move(src.m_WaitSemaphores))
        , m_WaitDstStageMasks(std::move(src.m_WaitDstStageMasks))
        , m_SignalSemaphores(std::move(src.m_SignalSemaphores))
        , m_WaitSemaphoreValues(std::move(src.m_WaitSemaphoreValues))
        , m_SignalSemaphoreValues(std::move(src.m_SignalSemaphoreValues))
    {
        m_CommandBuffer = src.m_CommandBuffer;
        assert(m_CommandBuffer != VK_NULL_HANDLE);
        src.m_CommandBuffer = VK_NULL_HANDLE;

        m_SubmitInfo = src.m_SubmitInfo;
        m_SubmitInfoTimeline = src.m_SubmitInfoTimeline;
        if (m_SubmitInfo.pNext)
            m_SubmitInfo.pNext = &m_SubmitInfoTimeline;
        m_SubmitInfo.pCommandBuffers = &m_CommandBuffer;
        src.m_SubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        src.m_SubmitInfoTimeline = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };

        m_QueueIndex = src.m_QueueIndex;
        m_CompletedFence = src.m_CompletedFence;
        src.m_CompletedFence = VK_NULL_HANDLE;
    }
    Wrap_VkSubmitInfo(const Wrap_VkSubmitInfo&) = delete;
    Wrap_VkSubmitInfo& operator=(const Wrap_VkSubmitInfo&) = delete;
public:
    VkSubmitInfo m_SubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    uint32_t m_QueueIndex = 0;
    VkFence m_CompletedFence = VK_NULL_HANDLE;

private:
    VkTimelineSemaphoreSubmitInfoKHR m_SubmitInfoTimeline{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
    VkCommandBuffer m_CommandBuffer;
    std::vector<VkSemaphore> m_WaitSemaphores;
    std::vector<VkPipelineStageFlags> m_WaitDstStageMasks;
    std::vector<VkSemaphore> m_SignalSemaphores;
    std::vector<uint64_t> m_WaitSemaphoreValues;
    std::vector<uint64_t> m_SignalSemaphoreValues;
};
