// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "vulkan_support.hpp"
#include "system/assetManager.hpp"
#include "system/os_common.h"
#include "memory/memoryManager.hpp"

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
bool CreateUniformBuffer(Vulkan* pVulkan, Uniform* pNewUniform, uint32_t dataSize, const void* const pData, VkBufferUsageFlags usage)
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
void UpdateUniformBuffer(Vulkan* pVulkan, Uniform* pUniform, uint32_t dataSize, const void* const pNewData)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    void* pMappedData = NULL;
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

    // If we have an independant async compute pool indicate this command list is using it (and must be submitted to the associated device queue).
    m_UseComputeCmdPool = UseComputeCmdPool && pVulkan->m_VulkanAsyncComputeCmdPool;

    // Allocate the command buffer from the pool
    VkCommandBufferAllocateInfo AllocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    AllocInfo.commandPool = m_UseComputeCmdPool ? pVulkan->m_VulkanAsyncComputeCmdPool : pVulkan->m_VulkanCmdPool;
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
bool Wrap_VkCommandBuffer::BeginRenderPass(VkRect2D RenderExtent, float MinDepth, float MaxDepth, VkClearColorValue ClearColor, uint32_t NumColorBuffers, bool HasDepth, VkRenderPass RenderPass, bool IsSwapChainRenderPass, VkFramebuffer FrameBuffer, VkSubpassContents SubContents)
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
        ClearValues[ClearValuesCount].color = ClearColor;
        ++ClearValuesCount;
    }
    if (HasDepth)
    {
        ClearValues[ClearValuesCount].depthStencil.depth = MaxDepth;
        ClearValues[ClearValuesCount].depthStencil.stencil = 0;
        ++ClearValuesCount;
    }

    // ... begin render pass ...
    VkRenderPassBeginInfo RPBeginInfo = {};
    RPBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RPBeginInfo.renderPass = RenderPass;
    RPBeginInfo.renderArea = RenderExtent;
    RPBeginInfo.clearValueCount = ClearValuesCount;
    RPBeginInfo.pClearValues = ClearValues.data();
    VkRenderPassTransformBeginInfoQCOM RPTransformBeginInfoQCOM = {};

    RPBeginInfo.framebuffer = FrameBuffer;

    if (IsSwapChainRenderPass && m_pVulkan->FillRenderPassTransformBeginInfoQCOM(RPTransformBeginInfoQCOM))
    {
        //LOGI("RPTransformBeginInfoQCOM(%s): Pre swap Extents = (%d x %d)", m_Name.c_str(), RPBeginInfo.renderArea.extent.width, RPBeginInfo.renderArea.extent.height);
        std::swap(RPBeginInfo.renderArea.extent.width, RPBeginInfo.renderArea.extent.height);
        RPBeginInfo.pNext = &RPTransformBeginInfoQCOM;
    }

    //LOGI("BeginRenderPass(%s): Extents = (%d x %d) %s", m_Name.c_str(), RPBeginInfo.renderArea.extent.width, RPBeginInfo.renderArea.extent.height, (RPBeginInfo.pNext == &RPTransformBeginInfoQCOM) ? " (transformed)" : "");

    vkCmdBeginRenderPass(m_VkCommandBuffer, &RPBeginInfo, SubContents);
    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkCommandBuffer::NextSubpass()
//-----------------------------------------------------------------------------
{
    vkCmdNextSubpass(m_VkCommandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
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
void Wrap_VkCommandBuffer::QueueSubmit(tcb::span<VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, tcb::span<VkSemaphore> SignalSemaphores)
//-----------------------------------------------------------------------------
{
    assert( m_VkCommandBuffer != VK_NULL_HANDLE );
    m_pVulkan->QueueSubmit( { &m_VkCommandBuffer, 1 }, WaitSemaphores, WaitDstStageMasks, SignalSemaphores, m_UseComputeCmdPool );
}

//-----------------------------------------------------------------------------
void Wrap_VkCommandBuffer::Release()
//-----------------------------------------------------------------------------
{
    if (m_VkCommandBuffer != VK_NULL_HANDLE)
    {
        // Do not need to worry about the device or pool being NULL since we could 
        // not have created the command buffer!
        auto* cmdPool = m_UseComputeCmdPool ? m_pVulkan->m_VulkanAsyncComputeCmdPool : m_pVulkan->m_VulkanCmdPool;
        vkFreeCommandBuffers(m_pVulkan->m_VulkanDevice, cmdPool, 1, &m_VkCommandBuffer);
    }

    // Clear everything back to starting state
    HardReset();
}



//=============================================================================
// CRenderTarget
//=============================================================================
//-----------------------------------------------------------------------------
CRenderTarget::CRenderTarget()
//-----------------------------------------------------------------------------
{
    HardReset();
}

//-----------------------------------------------------------------------------
CRenderTarget::~CRenderTarget()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
void CRenderTarget::HardReset()
//-----------------------------------------------------------------------------
{
    m_Name = "RenderTarget";

    m_Width = 0;
    m_Height = 0;

    m_pLayerFormats.clear();
    m_DepthFormat = VK_FORMAT_UNDEFINED;
    m_Msaa = {};

    m_ColorAttachments.clear();
    m_DepthAttachment = std::move(VulkanTexInfo());

    m_FrameBuffer = VK_NULL_HANDLE;
    m_FrameBufferDepthOnly = VK_NULL_HANDLE;

    m_pVulkan = nullptr;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat, tcb::span<const VkSampleCountFlagBits> Msaa, const char* pName)
//-----------------------------------------------------------------------------
{
    m_pVulkan = pVulkan;
    m_DepthFormat = DepthFormat;
    m_Msaa.assign(Msaa.begin(), Msaa.end());
    m_Msaa.resize( pLayerFormats.size(), VK_SAMPLE_COUNT_1_BIT );

    m_Width = uiWidth;
    m_Height = uiHeight;

    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    m_ColorAttachments.clear();

    m_pLayerFormats.assign( pLayerFormats.begin(), pLayerFormats.end() );

    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::Initialize(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, const tcb::span<const VkFormat> pLayerFormats, VkFormat DepthFormat, VkSampleCountFlagBits Msaa, const char* pName)
//-----------------------------------------------------------------------------
{
    m_Msaa.resize( pLayerFormats.size(), Msaa );

    return Initialize(pVulkan, uiWidth, uiHeight, pLayerFormats, DepthFormat, m_Msaa, pName);
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeDepth()
//-----------------------------------------------------------------------------
{
    if (m_DepthFormat != VK_FORMAT_UNDEFINED)
    {
        char szName[256];
        sprintf(szName, "%s: Depth", m_Name.c_str());
        m_DepthAttachment = CreateTextureObject(m_pVulkan, m_Width, m_Height, m_DepthFormat, TT_DEPTH_TARGET, m_Name.c_str(), m_Msaa.empty() ? VK_SAMPLE_COUNT_1_BIT : m_Msaa[0]);
    }
    else
    {
        ReleaseTexture(m_pVulkan, &m_DepthAttachment);
    }
    return true;
}

//-----------------------------------------------------------------------------
bool CRenderTarget::InitializeColor(const tcb::span<const TEXTURE_TYPE> TextureTypes)
//-----------------------------------------------------------------------------
{
    const auto NumColorLayers = GetNumColorLayers();
    LOGI("Creating Render Target (%s): (%d x %d); %d color layer[s]", m_Name.c_str(), m_Width, m_Height, NumColorLayers);

    m_ColorAttachments.clear();
    m_ColorAttachments.reserve(NumColorLayers);

    // First create the actual texture objects...
    char szName[256];
    for (size_t WhichLayer = 0; WhichLayer < NumColorLayers; WhichLayer++)
    {
        sprintf(szName, "%s: Color", m_Name.c_str());

        const TEXTURE_TYPE TextureType = (WhichLayer < TextureTypes.size()) ? TextureTypes[WhichLayer] : TT_RENDER_TARGET;
        m_ColorAttachments.emplace_back( CreateTextureObject(m_pVulkan, m_Width, m_Height, m_pLayerFormats[WhichLayer], TextureType, m_Name.c_str(), m_Msaa[WhichLayer]) );
    }
    return true;
}

bool CRenderTarget::InitializeFrameBuffer(VkRenderPass renderPass, const tcb::span<const VulkanTexInfo> ColorAttachments, const VulkanTexInfo* pDepthAttachment, VkFramebuffer* pFramebuffer )
{
    VkResult RetVal;

    // ... then attach them to the render target
    std::vector<VkImageView> attachments;
    attachments.reserve( ColorAttachments.size() + 1/*depth*/ );

    for (const VulkanTexInfo& ColorAttachement: ColorAttachments)
    {
        attachments.push_back(ColorAttachement.GetVkImageView());
    }
    if (pDepthAttachment && pDepthAttachment->Format != VK_FORMAT_UNDEFINED)
    {
        attachments.push_back(pDepthAttachment->GetVkImageView());
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
void CRenderTarget::Release()
//-----------------------------------------------------------------------------
{
    if (m_pVulkan == nullptr)
        return;

    for (auto& ColorAttachment: m_ColorAttachments)
    {
        ReleaseTexture(m_pVulkan, &ColorAttachment);
    }
    m_ColorAttachments.clear();

    m_pLayerFormats.clear();

    ReleaseTexture(m_pVulkan, &m_DepthAttachment);

    if (m_FrameBuffer != VK_NULL_HANDLE)
        vkDestroyFramebuffer(m_pVulkan->m_VulkanDevice, m_FrameBuffer, NULL);

    // Clear everything back to starting state
    HardReset();
}

