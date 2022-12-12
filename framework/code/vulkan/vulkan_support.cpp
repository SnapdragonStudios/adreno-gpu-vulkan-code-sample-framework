//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "system/assetManager.hpp"
#include "system/os_common.h"
#include "memory/memoryManager.hpp"
#include "vulkan_support.hpp"
#include "renderTarget.hpp"

//-----------------------------------------------------------------------------
bool LoadShader(Vulkan* pVulkan, AssetManager& assetManager, ShaderInfo* pShader, const std::string& VertFilename, const std::string& FragFilename)
//-----------------------------------------------------------------------------
{
    if( !VertFilename.empty() )
    {
        if( !pShader->VertShaderModule.Load( *pVulkan, assetManager, VertFilename ) )
        {
            return false;
        }
    }
    if( !FragFilename.empty() )
    {
        if( !pShader->FragShaderModule.Load( *pVulkan, assetManager, FragFilename ) )
        {
            return false;
        }
    }
    return true;
}


//-----------------------------------------------------------------------------
void ReleaseShader( Vulkan* pVulkan, ShaderInfo* pShader )
//-----------------------------------------------------------------------------
{
    pShader->FragShaderModule.Destroy( *pVulkan );
    pShader->VertShaderModule.Destroy( *pVulkan );
}

//-----------------------------------------------------------------------------
bool CreateUniformBuffer(Vulkan* pVulkan, Uniform* pNewUniform, size_t dataSize, const void* const pData, VkBufferUsageFlags usage)
//-----------------------------------------------------------------------------
{
    MemoryManager& memoryManager = pVulkan->GetMemoryManager();

    // Create the memory buffer...
    pNewUniform->buf = memoryManager.CreateBuffer(dataSize, usage, MemoryManager::MemoryUsage::CpuToGpu, &pNewUniform->bufferInfo);

    // If we have initial data, add it now
    if (pData != nullptr)
    {
        UpdateUniformBuffer(pVulkan, pNewUniform, dataSize, pData);
    }

    return true;
}

//-----------------------------------------------------------------------------
void UpdateUniformBuffer(Vulkan* pVulkan, Uniform* pUniform, size_t dataSize, const void* const pNewData)
//-----------------------------------------------------------------------------
{
    MemoryCpuMapped<uint8_t> mapped = pVulkan->GetMemoryManager().Map<uint8_t>(pUniform->buf);
    if (mapped.data()==nullptr)
    {
        return;
    }

    memcpy(mapped.data(), pNewData, dataSize);

    pVulkan->GetMemoryManager().Unmap(pUniform->buf, std::move(mapped));
}

//-----------------------------------------------------------------------------
void ReleaseUniformBuffer(Vulkan* pVulkan, Uniform* pUniform)
//-----------------------------------------------------------------------------
{
    if (pUniform->buf)
    {
        pVulkan->GetMemoryManager().Destroy(std::move(pUniform->buf));
    }
}


//-----------------------------------------------------------------------------
bool CreateUniformBuffer(Vulkan* pVulkan, MemoryVmaAllocatedBuffer<VkBuffer> &rNewUniformBuffer, size_t dataSize, const void* const pData, VkBufferUsageFlags usage)
//-----------------------------------------------------------------------------
{
    MemoryManager& memoryManager = pVulkan->GetMemoryManager();

    // Create the memory buffer...
    rNewUniformBuffer = memoryManager.CreateBuffer(dataSize, usage, MemoryManager::MemoryUsage::CpuToGpu, nullptr);

    // If we have initial data, add it now
    if (pData != nullptr)
    {
        UpdateUniformBuffer(pVulkan, rNewUniformBuffer, dataSize, pData);
    }

    return true;
}

//-----------------------------------------------------------------------------
void UpdateUniformBuffer(Vulkan* pVulkan, MemoryVmaAllocatedBuffer<VkBuffer>& rUniform, size_t dataSize, const void* const pNewData)
//-----------------------------------------------------------------------------
{
    MemoryCpuMapped<uint8_t> mapped = pVulkan->GetMemoryManager().Map<uint8_t>(rUniform);
    if (mapped.data() == nullptr)
    {
        return;
    }

    memcpy(mapped.data(), pNewData, dataSize);

    pVulkan->GetMemoryManager().Unmap(rUniform, std::move(mapped));
}

//-----------------------------------------------------------------------------
void ReleaseUniformBuffer(Vulkan* pVulkan, MemoryVmaAllocatedBuffer<VkBuffer>& rUniform)
//-----------------------------------------------------------------------------
{
    pVulkan->GetMemoryManager().Destroy(std::move(rUniform));
}


//-----------------------------------------------------------------------------
void VulkanSetBlendingType(BLENDING_TYPE WhichType, VkPipelineColorBlendAttachmentState* pBlendState)
//-----------------------------------------------------------------------------
{
    if (pBlendState == nullptr)
    {
        LOGE("VulkanSetBlendingType() called with nullptr parameter!");
        return;
    }

    switch (WhichType)
    {
    case BLENDING_TYPE::BT_NONE:
        // Blending Off
        // Should only have to disable the main flag
        pBlendState->blendEnable = VK_FALSE;
        pBlendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pBlendState->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        pBlendState->colorBlendOp = VK_BLEND_OP_ADD;
        pBlendState->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pBlendState->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pBlendState->alphaBlendOp = VK_BLEND_OP_ADD;
        pBlendState->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        break;
    case BLENDING_TYPE::BT_ALPHA:
        // Alpha Blending
        pBlendState->blendEnable = VK_TRUE;
        pBlendState->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pBlendState->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pBlendState->colorBlendOp = VK_BLEND_OP_ADD;
        pBlendState->srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;             // VK_BLEND_FACTOR_ONE;
        pBlendState->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;   // VK_BLEND_FACTOR_ZERO;
        pBlendState->alphaBlendOp = VK_BLEND_OP_ADD;
        pBlendState->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        break;
    case BLENDING_TYPE::BT_ADDITIVE:
        // Additive blending
        pBlendState->blendEnable = VK_TRUE;
        pBlendState->colorBlendOp = VK_BLEND_OP_ADD;
        pBlendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pBlendState->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pBlendState->alphaBlendOp = VK_BLEND_OP_ADD;
        pBlendState->srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pBlendState->dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        pBlendState->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        break;

    default:
        LOGE("VulkanSetBlendingType() called with unknown type (%d)! Defaulting to BT_NONE", WhichType);
        pBlendState->blendEnable = VK_FALSE;
        pBlendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pBlendState->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        pBlendState->colorBlendOp = VK_BLEND_OP_ADD;
        pBlendState->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pBlendState->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pBlendState->alphaBlendOp = VK_BLEND_OP_ADD;
        pBlendState->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        break;
    }
}

//=============================================================================
// Uniform
//=============================================================================
Uniform::Uniform( Uniform&& other) noexcept
    : buf(std::move(other.buf))
    , bufferInfo( other.bufferInfo )
{
    other.bufferInfo = {};
}


//=============================================================================
// Wrap_VkImage
//=============================================================================
//-----------------------------------------------------------------------------
Wrap_VkImage::Wrap_VkImage()
    : m_pVulkan(nullptr)
    , m_ImageInfo()
    , m_Usage(MemoryManager::MemoryUsage::Unknown)
//-----------------------------------------------------------------------------
{
    m_Name = "Wrap_VkImage";
}

//-----------------------------------------------------------------------------
Wrap_VkImage::~Wrap_VkImage()
//-----------------------------------------------------------------------------
{
    if (m_pVulkan)
    {
        MemoryManager& memoryManager = m_pVulkan->GetMemoryManager();
        memoryManager.Destroy( std::move(m_VmaImage) );
    }
}

//-----------------------------------------------------------------------------
bool Wrap_VkImage::Initialize(Vulkan* pVulkan, const VkImageCreateInfo& ImageInfo, MemoryManager::MemoryUsage Usage, const char* pName)
//-----------------------------------------------------------------------------
{
    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    // Need Vulkan objects to release ourselves
    m_pVulkan = pVulkan;
    m_ImageInfo = ImageInfo;
    m_Usage = Usage;

    MemoryManager& memoryManager = pVulkan->GetMemoryManager();
    m_VmaImage = memoryManager.CreateImage(ImageInfo, Usage);

    if( m_VmaImage )
    {
        pVulkan->SetDebugObjectName( m_VmaImage.GetVkBuffer(), pName );
    }

    return !!m_VmaImage;
}

//-----------------------------------------------------------------------------
void Wrap_VkImage::Release()
//-----------------------------------------------------------------------------
{
    if (m_pVulkan)
    {
        MemoryManager& memoryManager = m_pVulkan->GetMemoryManager();
        memoryManager.Destroy(std::move(m_VmaImage));
    }
    m_pVulkan = nullptr;
    m_ImageInfo = {};
    m_Name.clear();
}

//=============================================================================
// Wrap_VkCommandBuffer
//=============================================================================
//-----------------------------------------------------------------------------
Wrap_VkCommandBuffer::Wrap_VkCommandBuffer()
//-----------------------------------------------------------------------------
{
    HardReset();
}

//-----------------------------------------------------------------------------
Wrap_VkCommandBuffer::~Wrap_VkCommandBuffer()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
void Wrap_VkCommandBuffer::HardReset()
//-----------------------------------------------------------------------------
{
    m_Name = "CmdBuffer";

    m_NumDrawCalls = 0;
    m_NumTriangles = 0;

    m_IsPrimary = true;
    m_VkCommandBuffer = VK_NULL_HANDLE;

    m_pVulkan = nullptr;
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::Initialize(Vulkan* pVulkan, const std::string& Name, VkCommandBufferLevel CmdBuffLevel, bool UseComputeCmdPool)
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    m_Name = Name;

    // Reset draw counts
    m_NumDrawCalls = 0;
    m_NumTriangles = 0;

    // Need Vulkan objects to release ourselves
    m_pVulkan = pVulkan;

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

    // If we have an independant compute pool indicate this command list is using it (and must be submitted to the associated device queue).
    m_UseComputeCmdPool = UseComputeCmdPool && pVulkan->m_VulkanComputeCmdPool;

    // Allocate the command buffer from the pool
    VkCommandBufferAllocateInfo AllocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    AllocInfo.commandPool = m_UseComputeCmdPool ? pVulkan->m_VulkanComputeCmdPool : pVulkan->m_VulkanCmdPool;
    AllocInfo.level = CmdBuffLevel;
    AllocInfo.commandBufferCount = 1;

    RetVal = vkAllocateCommandBuffers(pVulkan->m_VulkanDevice, &AllocInfo, &m_VkCommandBuffer);
    if (!CheckVkError("vkAllocateCommandBuffers()", RetVal))
    {
        LOGE("Unable to allocate command buffer: %s", m_Name.c_str());
        return false;
    }
    pVulkan->SetDebugObjectName(m_VkCommandBuffer, m_Name.c_str());

    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::Reset()
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

    return true;
}


//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::Begin(VkCommandBufferUsageFlags CmdBuffUsage)
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
bool Wrap_VkCommandBuffer::Begin( VkFramebuffer FrameBuffer, VkRenderPass RenderPass, bool IsSwapChainRenderPass, uint32_t SubPass, VkCommandBufferUsageFlags CmdBuffUsage)
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

    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::BeginRenderPass(VkRect2D RenderExtent, float MinDepth, float MaxDepth, const tcb::span<const VkClearColorValue> ClearColors, uint32_t NumColorBuffers, bool HasDepth, VkRenderPass RenderPass, bool IsSwapChainRenderPass, VkFramebuffer FrameBuffer, VkSubpassContents SubContents)
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
bool Wrap_VkCommandBuffer::BeginRenderPass(const CRenderTarget& renderTarget, VkRenderPass RenderPass, VkSubpassContents SubContents)
//-----------------------------------------------------------------------------
{
    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = renderTarget.m_Width;
    Scissor.extent.height = renderTarget.m_Height;

    return BeginRenderPass(Scissor, 0.0f, 1.0f, { renderTarget.m_ClearColorValues.data(), renderTarget.m_ClearColorValues.size() }, renderTarget.GetNumColorLayers(), renderTarget.m_DepthFormat != VK_FORMAT_UNDEFINED, RenderPass, false, renderTarget.m_FrameBuffer, SubContents);
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::NextSubpass( VkSubpassContents SubContents )
//-----------------------------------------------------------------------------
{
    vkCmdNextSubpass(m_VkCommandBuffer, SubContents);
    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::EndRenderPass()
//-----------------------------------------------------------------------------
{
    vkCmdEndRenderPass(m_VkCommandBuffer);
    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::End()
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
void Wrap_VkCommandBuffer::QueueSubmit( const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, VkFence CompletionFence ) const
//-----------------------------------------------------------------------------
{
    assert( m_VkCommandBuffer != VK_NULL_HANDLE );
    m_pVulkan->QueueSubmit( { &m_VkCommandBuffer, 1 }, WaitSemaphores, WaitDstStageMasks, SignalSemaphores, m_UseComputeCmdPool, CompletionFence );
}

//-----------------------------------------------------------------------------
void Wrap_VkCommandBuffer::QueueSubmit( const Vulkan::BufferIndexAndFence& CurrentVulkanBuffer, VkSemaphore renderCompleteSemaphore ) const
//-----------------------------------------------------------------------------
{
    assert( m_VkCommandBuffer != VK_NULL_HANDLE );
    QueueSubmit( CurrentVulkanBuffer.semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, renderCompleteSemaphore, CurrentVulkanBuffer.fence );
}


//-----------------------------------------------------------------------------
void Wrap_VkCommandBuffer::Release()
//-----------------------------------------------------------------------------
{
    if (m_VkCommandBuffer != VK_NULL_HANDLE)
    {
        // Do not need to worry about the device or pool being NULL since we could 
        // not have created the command buffer!
        auto* cmdPool = m_UseComputeCmdPool ? m_pVulkan->m_VulkanComputeCmdPool : m_pVulkan->m_VulkanCmdPool;
        vkFreeCommandBuffers(m_pVulkan->m_VulkanDevice, cmdPool, 1, &m_VkCommandBuffer);
    }

    // Clear everything back to starting state
    HardReset();
}
