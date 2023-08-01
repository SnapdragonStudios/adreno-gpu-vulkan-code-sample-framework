//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "commandBuffer.hpp"
#include "renderTarget.hpp"
#include "timerPool.hpp"
#include "vulkan_support.hpp"

//-----------------------------------------------------------------------------
CommandListT<Vulkan>::CommandListT() noexcept
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
CommandListT<Vulkan>::~CommandListT()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
CommandListT<Vulkan>::CommandListT(CommandListT<Vulkan>&& other) noexcept
//---------------------------------------------------------------------------
{
    assert(0);  // Currently move is not implemented for this class, but std::vector will not compile if a move (or copy) is not provided, we currently expect that the move will not get called (ie vector of CommandListT<Vulkan> will not resize)
}

//-----------------------------------------------------------------------------
CommandListT<Vulkan>& CommandListT<Vulkan>::operator=(CommandListT<Vulkan> && other) noexcept
//-----------------------------------------------------------------------------
{
    assert(0);  // Currently move is not implemented for this class, but std::vector will not compile if a move (or copy) is not provided, we currently expect that the move will not get called (ie vector of CommandListT<Vulkan> will not resize)
    return *this;
}


//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::Initialize(Vulkan* pVulkan, const std::string& Name, VkCommandBufferLevel CmdBuffLevel, uint32_t QueueIndex, TimerPoolBase* pGpuTimerPool)
//-----------------------------------------------------------------------------
{
    m_Name = Name;

    // Reset draw counts
    m_NumDrawCalls = 0;
    m_NumTriangles = 0;

    // Need Vulkan objects to release ourselves
    m_pVulkan = pVulkan;

    // Timer queries
    m_GpuTimerPool = pGpuTimerPool;
    assert(m_GpuTimerQueries.empty());

    // What type of command buffer are we dealing with
    if (CmdBuffLevel == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        m_IsPrimary = true;
    }
    else if (CmdBuffLevel == VK_COMMAND_BUFFER_LEVEL_SECONDARY)
    {
        m_IsPrimary = false;
    }
    else
    {
        LOGE("Command Buffer initialization with unknown level! (%d)", CmdBuffLevel);
    }

    // Store which queue this command list is using (index in to Vulkan::m_VulkanQueues) so we can submit to the associated device queue.
    assert(pVulkan->m_VulkanQueues[QueueIndex].Queue != VK_NULL_HANDLE);
    m_QueueIndex = QueueIndex;
    pVulkan->AllocateCommandBuffer(CmdBuffLevel, QueueIndex, &m_VkCommandBuffer);
    pVulkan->SetDebugObjectName(m_VkCommandBuffer, m_Name.c_str());
    return true;
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::Reset()
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    if (m_VkCommandBuffer == VK_NULL_HANDLE)
    {
        LOGE("Error! Trying to reset command buffer before it has been initialized: %s", m_Name.c_str());
        return false;
    }

    // vkBeginCommandBuffer should reset the command buffer, but Reset can be called
    // to make it more explicit.
    RetVal = vkResetCommandBuffer(m_VkCommandBuffer, 0);
    if (!CheckVkError("vkResetCommandBuffer()", RetVal))
    {
        LOGE("Unable to reset command buffer: %s", m_Name.c_str());
        return false;
    }

    // Reset draw counts
    m_NumDrawCalls = 0;
    m_NumTriangles = 0;

    // Reset queries
    m_GpuTimerPool->TimerCommandBufferReset(m_GpuTimerQueries);
    m_GpuTimerQueries.clear();

    return true;
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::Begin(VkCommandBufferUsageFlags CmdBuffUsage)
//-----------------------------------------------------------------------------
{
    assert(m_IsPrimary);
    if (!m_IsPrimary)
    {
        LOGE("Error! Trying to begin secondary command buffer without Framebuffer or RenderPass: %s", m_Name.c_str());
        return false;
    }

    return Begin(VK_NULL_HANDLE, VK_NULL_HANDLE, false, 0, CmdBuffUsage);
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::Begin( VkFramebuffer FrameBuffer, VkRenderPass RenderPass, bool IsSwapChainRenderPass, uint32_t SubPass, VkCommandBufferUsageFlags CmdBuffUsage)
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    if (m_VkCommandBuffer == VK_NULL_HANDLE)
    {
        LOGE("Error! Trying to begin command buffer before it has been initialized: %s", m_Name.c_str());
        return false;
    }

    VkCommandBufferBeginInfo CmdBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CmdBeginInfo.flags = CmdBuffUsage;

    VkCommandBufferInheritanceInfo InheritanceInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
    VkCommandBufferInheritanceRenderPassTransformInfoQCOM InheritanceInfoRenderPassTransform = {};

    // If this is a secondary command buffer it has inheritance information.
    // If primary, the inheritance stuff is ignored.
    if (!m_IsPrimary)
    {
        InheritanceInfo.framebuffer = FrameBuffer;
        InheritanceInfo.occlusionQueryEnable = VK_FALSE;
        InheritanceInfo.queryFlags = 0;
        InheritanceInfo.pipelineStatistics = 0;

        // If this is a secondary command buffer it MAY BE inside another render pass (compute can be in a secondary buffer and not have/inherit a renderder pass)
        if (RenderPass != VK_NULL_HANDLE)
        {
            CmdBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

            InheritanceInfo.renderPass = RenderPass;
            InheritanceInfo.subpass = SubPass;

            // We may also have to pass the inherited render pass transform down to the secondary buffer
            if (IsSwapChainRenderPass && m_pVulkan->FillCommandBufferInheritanceRenderPassTransformInfoQCOM(InheritanceInfoRenderPassTransform))
            {
                LOGI("VkCommandBufferInheritanceRenderPassTransformInfoQCOM (%s): Extents = (%d x %d)", m_Name.c_str(), InheritanceInfoRenderPassTransform.renderArea.extent.width, InheritanceInfoRenderPassTransform.renderArea.extent.height);
                InheritanceInfo.pNext = &InheritanceInfoRenderPassTransform;
            }
        }
        CmdBeginInfo.pInheritanceInfo = &InheritanceInfo;
    }

    // By calling vkBeginCommandBuffer, cmdBuffer is put into the recording state.
    RetVal = vkBeginCommandBuffer(m_VkCommandBuffer, &CmdBeginInfo);
    if (!CheckVkError("vkBeginCommandBuffer()", RetVal))
    {
        LOGE("Unable to begin command buffer: %s", m_Name.c_str());
        return false;
    }

    // Reset draw counts
    m_NumDrawCalls = 0;
    m_NumTriangles = 0;

    // Reset queries
    m_GpuTimerPool->TimerCommandBufferReset(m_GpuTimerQueries);
    m_GpuTimerQueries.clear();

    return true;
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::BeginRenderPass(VkRect2D RenderExtent, float MinDepth, float MaxDepth, const std::span<const VkClearColorValue> ClearColors, uint32_t NumColorBuffers, bool HasDepth, VkRenderPass RenderPass, bool IsSwapChainRenderPass, VkFramebuffer FrameBuffer, VkSubpassContents SubContents)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ... set Viewport and Scissor for this pass ...
    VkViewport Viewport = {};
    Viewport.x = (float)RenderExtent.offset.x;
    Viewport.y = (float)RenderExtent.offset.y;
    Viewport.width = (float)RenderExtent.extent.width;
    Viewport.height = (float)RenderExtent.extent.height;
    Viewport.minDepth = MinDepth;
    Viewport.maxDepth = MaxDepth;

    vkCmdSetViewport(m_VkCommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(m_VkCommandBuffer, 0, 1, &RenderExtent);

    // ... set clear values ...
    std::array<VkClearValue, 9> ClearValues;
    uint32_t ClearValuesCount = 0;

    for (uint32_t i=0; i< NumColorBuffers; ++i)
    {
        ClearValues[ClearValuesCount].color = ClearColors[ClearColors.size() > NumColorBuffers ? ClearValuesCount : 0];
        ++ClearValuesCount;
    }
    if (HasDepth)
    {
        ClearValues[ClearValuesCount].depthStencil.depth = MaxDepth;
        ClearValues[ClearValuesCount].depthStencil.stencil = 0;
        ++ClearValuesCount;
    }

    // ... begin render pass ...
    VkRenderPassBeginInfo RPBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    RPBeginInfo.renderPass = RenderPass;
    RPBeginInfo.renderArea = RenderExtent;
    RPBeginInfo.clearValueCount = ClearValuesCount;
    RPBeginInfo.pClearValues = ClearValues.data();
    VkRenderPassTransformBeginInfoQCOM RPTransformBeginInfoQCOM = {};

    RPBeginInfo.framebuffer = FrameBuffer;

    if (IsSwapChainRenderPass && m_pVulkan->FillRenderPassTransformBeginInfoQCOM(RPTransformBeginInfoQCOM))
    {
        //LOGI("RPTransformBeginInfoQCOM(%s): Pre swap Extents = (%d x %d) transform = %d", m_Name.c_str(), RPBeginInfo.renderArea.extent.width, RPBeginInfo.renderArea.extent.height, RPTransformBeginInfoQCOM.transform);
        RPBeginInfo.pNext = &RPTransformBeginInfoQCOM;
    }

    //LOGI("BeginRenderPass(%s): Extents = (%d x %d) %s", m_Name.c_str(), RPBeginInfo.renderArea.extent.width, RPBeginInfo.renderArea.extent.height, (RPBeginInfo.pNext == &RPTransformBeginInfoQCOM) ? " (transformed)" : "");

    vkCmdBeginRenderPass(m_VkCommandBuffer, &RPBeginInfo, SubContents);
    return true;
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::BeginRenderPass(const CRenderTarget& renderTarget, VkRenderPass RenderPass, VkSubpassContents SubContents)
//-----------------------------------------------------------------------------
{
    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = renderTarget.m_Width;
    Scissor.extent.height = renderTarget.m_Height;

    return BeginRenderPass(Scissor, 0.0f, 1.0f, { renderTarget.m_ClearColorValues.data(), renderTarget.m_ClearColorValues.size() }, renderTarget.GetNumColorLayers(), renderTarget.m_DepthFormat != TextureFormat::UNDEFINED, RenderPass, false, renderTarget.m_FrameBuffer, SubContents);
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::NextSubpass( VkSubpassContents SubContents )
//-----------------------------------------------------------------------------
{
    vkCmdNextSubpass(m_VkCommandBuffer, SubContents);
    return true;
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::EndRenderPass()
//-----------------------------------------------------------------------------
{
    vkCmdEndRenderPass(m_VkCommandBuffer);
    return true;
}

//-----------------------------------------------------------------------------
int CommandListT<Vulkan>::StartGpuTimer(const std::string_view& timerName)
//-----------------------------------------------------------------------------
{
    if (!m_GpuTimerPool)
        return -1;
    int deviceQueueFamilyIndex = m_pVulkan->m_VulkanQueues[m_QueueIndex].QueueFamilyIndex;
    assert(deviceQueueFamilyIndex >= 0);
    const auto timerId = m_GpuTimerPool->StartGpuTimer(m_VkCommandBuffer, timerName, deviceQueueFamilyIndex);
    if (timerId>=0)
        m_GpuTimerQueries.push_back(timerId);
    return timerId;
}

//-----------------------------------------------------------------------------
void CommandListT<Vulkan>::StopGpuTimer(int timerId)
//-----------------------------------------------------------------------------
{
    if (timerId < 0)
        return;
    // assume timerId is in m_GpuTimerQueries (we are in trouble if it is not, likely didnt call StartGpuTimer)
    assert(m_GpuTimerPool);
    m_GpuTimerPool->StopGpuTimer(m_VkCommandBuffer, timerId);
}

//-----------------------------------------------------------------------------
bool CommandListT<Vulkan>::End()
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    if (m_VkCommandBuffer == VK_NULL_HANDLE)
    {
        LOGE("Error! Trying to end command buffer before it has been initialized: %s", m_Name.c_str());
        return false;
    }

    // By ending the command buffer, it is put out of record mode.
    RetVal = vkEndCommandBuffer(m_VkCommandBuffer);
    if (!CheckVkError("vkEndCommandBuffer()", RetVal))
    {
        LOGE("Unable to end command buffer: %s", m_Name.c_str());
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
void CommandListT<Vulkan>::QueueSubmit( const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores, VkFence CompletionFence ) const
//-----------------------------------------------------------------------------
{
    assert( m_VkCommandBuffer != VK_NULL_HANDLE );
    m_pVulkan->QueueSubmit( { &m_VkCommandBuffer, 1 }, WaitSemaphores, WaitDstStageMasks, SignalSemaphores, m_QueueIndex, CompletionFence );
}

//-----------------------------------------------------------------------------
void CommandListT<Vulkan>::QueueSubmit(const std::span<const SemaphoreWait> WaitSemaphores, const std::span<const SemaphoreSignal> SignalSemaphores, VkFence CompletionFence) const
//-----------------------------------------------------------------------------
{
    constexpr uint32_t cMaxSemaphores = 8;
    std::array<VkSemaphoreSubmitInfoKHR, cMaxSemaphores> SemaphoreInfos;
    assert(WaitSemaphores.size() + SignalSemaphores.size() < SemaphoreInfos.size());
    uint32_t SemaphoreCount = 0, WaitSemaphoreInfosCount = 0;
    for (WaitSemaphoreInfosCount = 0; SemaphoreCount < SemaphoreInfos.size() && SemaphoreCount < WaitSemaphores.size(); ++SemaphoreCount, ++WaitSemaphoreInfosCount)
    {
        SemaphoreInfos[SemaphoreCount] = { .sType = (VkStructureType) VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
                                           .semaphore = WaitSemaphores[WaitSemaphoreInfosCount].semaphore.GetVkSemaphore(),
                                           .value = WaitSemaphores[WaitSemaphoreInfosCount].value,
                                           .stageMask = WaitSemaphores[WaitSemaphoreInfosCount].stageMask,
                                           .deviceIndex = 0 };
    }

    uint32_t SignalSemaphoreInfosCount = 0;
    for (SignalSemaphoreInfosCount = 0; SemaphoreCount < SemaphoreInfos.size() && SignalSemaphoreInfosCount < SignalSemaphores.size(); ++SemaphoreCount, ++SignalSemaphoreInfosCount)
    {
        SemaphoreInfos[SemaphoreCount] = { .sType = (VkStructureType) VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
                                           .semaphore = SignalSemaphores[SignalSemaphoreInfosCount].semaphore.GetVkSemaphore(),
                                           .value = SignalSemaphores[SignalSemaphoreInfosCount].value,
                                           .stageMask = SignalSemaphores[SignalSemaphoreInfosCount].stageMask,
                                           .deviceIndex = 0 };
    }

    std::array<VkCommandBufferSubmitInfo, 1> CommandBufferInfos;
    CommandBufferInfos[0] = {.sType = (VkStructureType) VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
                             .commandBuffer = m_VkCommandBuffer };

    VkSubmitInfo2KHR SubmitInfo{ .sType = (VkStructureType) VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
                                 .waitSemaphoreInfoCount = WaitSemaphoreInfosCount,
                                 .pWaitSemaphoreInfos = SemaphoreInfos.data(),
                                 .commandBufferInfoCount = (uint32_t) CommandBufferInfos.size(),
                                 .pCommandBufferInfos = CommandBufferInfos.data(),
                                 .signalSemaphoreInfoCount = SignalSemaphoreInfosCount,
                                 .pSignalSemaphoreInfos = SemaphoreInfos.data() + WaitSemaphoreInfosCount };

    m_pVulkan->QueueSubmit({ &SubmitInfo,1}, m_QueueIndex, CompletionFence);
}

//-----------------------------------------------------------------------------
void CommandListT<Vulkan>::QueueSubmit( const Vulkan::BufferIndexAndFence& CurrentVulkanBuffer, VkSemaphore renderCompleteSemaphore ) const
//-----------------------------------------------------------------------------
{
    assert( m_VkCommandBuffer != VK_NULL_HANDLE );
    QueueSubmit( CurrentVulkanBuffer.semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, renderCompleteSemaphore, CurrentVulkanBuffer.fence );
}


//-----------------------------------------------------------------------------
void CommandListT<Vulkan>::Release()
//-----------------------------------------------------------------------------
{
    if (m_VkCommandBuffer != VK_NULL_HANDLE)
    {
        // Do not need to worry about the device or pool being NULL since we could 
        // not have created the command buffer!
        m_pVulkan->FreeCommandBuffer(m_QueueIndex, m_VkCommandBuffer);
        m_VkCommandBuffer = nullptr;
    }

    m_Name.clear();
    m_NumDrawCalls = 0;
    m_NumTriangles = 0;
    m_IsPrimary = true;
    m_QueueIndex = 0;
    m_GpuTimerPool = nullptr;
    m_GpuTimerQueries.clear();
    m_pVulkan = nullptr;
}
