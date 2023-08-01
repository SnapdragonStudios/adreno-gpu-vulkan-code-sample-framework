//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "system/assetManager.hpp"
#include "system/os_common.h"
#include "vulkan_support.hpp"
#include "renderTarget.hpp"
#include "timerPool.hpp"

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

//-----------------------------------------------------------------------------
bool DumpImagePixelData(
    Vulkan& vulkan,
    VkImage targetImage,
    TextureFormat targetFormat,
    uint32_t targetWidth,
    uint32_t targetHeight,
    bool dumpColor,
    uint32_t mipLevel,
    uint32_t arrayLayer,
    const Vulkan::tDumpSwapChainOutputFn& outputFunction)
    //-----------------------------------------------------------------------------
{
#if OS_WINDOWS

    TextureFormat format;
    VkImageAspectFlags aspectFlags;
    if (dumpColor)
    {
        format = FormatIsSrgb(targetFormat) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
        aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    else
    {
        format = TextureFormat::D24_UNORM_S8_UINT;
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    
    const uint32_t width = targetWidth >> mipLevel;
    const uint32_t height = targetHeight >> mipLevel;

    const VkFormat vkTargetFormat = TextureFormatToVk(format);
    const VkFormatProperties& targetFormatProps = vulkan.GetFormatProperties(vkTargetFormat);

    const bool blitSupportLinear = targetFormatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const bool blitSupportOptimal = targetFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const bool useTemporaryImage = !blitSupportLinear || !blitSupportOptimal;

    MemoryAllocatedBuffer<Vulkan, VkImage> firstImage;
    MemoryAllocatedBuffer<Vulkan, VkImage> secondImage;

    auto Cleanup = [&]()
    {
        if (firstImage) vulkan.GetMemoryManager().Destroy(std::move(firstImage));
        if (secondImage) vulkan.GetMemoryManager().Destroy(std::move(secondImage));
    };

    auto CreateImageAndSetupMemory = [&vulkan](
        VkCommandBuffer command_buffer,
        const VkImageCreateInfo& imageInfo,
        VkImageAspectFlags aspectFlags,
        MemoryUsage memoryUsage,
        MemoryAllocatedBuffer<Vulkan, VkImage>& targetImage) -> bool
    {
        targetImage = vulkan.GetMemoryManager().CreateImage(imageInfo, memoryUsage);
        if (!targetImage)
        {
            return false;
        }

        // Put the image on a layout we can work with

        VkImageMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        memoryBarrier.image = targetImage.GetVkBuffer();
        memoryBarrier.subresourceRange.aspectMask = aspectFlags;
        memoryBarrier.subresourceRange.baseMipLevel = 0;
        memoryBarrier.subresourceRange.levelCount = 1;
        memoryBarrier.subresourceRange.baseArrayLayer = 0;
        memoryBarrier.subresourceRange.layerCount = 1;
        memoryBarrier.srcAccessMask = 0;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &memoryBarrier);

        return true;
    };

    VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags = 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkTargetFormat;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = useTemporaryImage ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
    imageInfo.usage = useTemporaryImage ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto setupCommandBuffer = vulkan.StartSetupCommandBuffer();

    if (!CreateImageAndSetupMemory(
        setupCommandBuffer,
        imageInfo,
        aspectFlags,
        useTemporaryImage ? MemoryUsage::GpuExclusive : MemoryUsage::GpuToCpu,
        firstImage))
    {
        Cleanup();
        return false;
    }

    if (useTemporaryImage)
    {
        imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (!CreateImageAndSetupMemory(
            setupCommandBuffer,
            imageInfo,
            aspectFlags,
            MemoryUsage::GpuToCpu,
            secondImage))
        {
            Cleanup();
            return false;
        }
    }

    // Transition the swapchain image from present to transfer source
    {
        VkImageMemoryBarrier presentMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        presentMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        presentMemoryBarrier.image = targetImage;
        presentMemoryBarrier.subresourceRange.aspectMask = aspectFlags;
        presentMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
        presentMemoryBarrier.subresourceRange.levelCount = 1;
        presentMemoryBarrier.subresourceRange.baseArrayLayer = arrayLayer;
        presentMemoryBarrier.subresourceRange.layerCount = 1;
        presentMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        presentMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(
            setupCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &presentMemoryBarrier);
    }

    // Copy swapchain to the first image (which might be a temporary one depending if tilling/optional is supported by blit)
    {
        VkImageBlit blitRegion{};
        blitRegion.srcSubresource.aspectMask = aspectFlags;
        blitRegion.srcSubresource.baseArrayLayer = arrayLayer;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = mipLevel;
        blitRegion.srcOffsets[1].x = width;
        blitRegion.srcOffsets[1].y = height;
        blitRegion.srcOffsets[1].z = 1;
        blitRegion.dstSubresource.aspectMask = aspectFlags;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;
        blitRegion.dstOffsets[1].x = width;
        blitRegion.dstOffsets[1].y = height;
        blitRegion.dstOffsets[1].z = 1;

        vkCmdBlitImage(
            setupCommandBuffer,
            targetImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            firstImage.GetVkBuffer(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blitRegion,
            VK_FILTER_NEAREST);

        // Transition the swapchain back to the layout needed by the upstream

        VkImageMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        memoryBarrier.image = targetImage;
        memoryBarrier.subresourceRange.aspectMask = aspectFlags;
        memoryBarrier.subresourceRange.baseMipLevel = 0;
        memoryBarrier.subresourceRange.levelCount = 1;
        memoryBarrier.subresourceRange.baseArrayLayer = 0;
        memoryBarrier.subresourceRange.layerCount = 1;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        memoryBarrier.dstAccessMask = 0;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(
            setupCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &memoryBarrier);
    }

    // If necessary, untile the image data by copying the first image (containing the swapchain data) into the 
    // secondary image (which we will use to read the data from)
    if (useTemporaryImage)
    {
        VkImageMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        memoryBarrier.image = firstImage.GetVkBuffer();
        memoryBarrier.subresourceRange.aspectMask = aspectFlags;
        memoryBarrier.subresourceRange.baseMipLevel = 0;
        memoryBarrier.subresourceRange.levelCount = 1;
        memoryBarrier.subresourceRange.baseArrayLayer = 0;
        memoryBarrier.subresourceRange.layerCount = 1;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(
            setupCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &memoryBarrier);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = copyRegion.dstSubresource.aspectMask = aspectFlags;
        copyRegion.srcSubresource.mipLevel = copyRegion.dstSubresource.mipLevel = 0;
        copyRegion.srcSubresource.baseArrayLayer = copyRegion.dstSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.layerCount = copyRegion.dstSubresource.layerCount = 1;
        copyRegion.srcOffset = copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent.width = width;
        copyRegion.extent.height = height;
        copyRegion.extent.depth = 1;

        vkCmdCopyImage(
            setupCommandBuffer,
            firstImage.GetVkBuffer(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            secondImage.GetVkBuffer(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);
    }

    // Transition whatever image we are going to read the screenshot data to general layout
    {
        VkImageMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        memoryBarrier.image = useTemporaryImage ? secondImage.GetVkBuffer() : firstImage.GetVkBuffer();
        memoryBarrier.subresourceRange.aspectMask = aspectFlags;
        memoryBarrier.subresourceRange.baseMipLevel = 0;
        memoryBarrier.subresourceRange.levelCount = 1;
        memoryBarrier.subresourceRange.baseArrayLayer = 0;
        memoryBarrier.subresourceRange.layerCount = 1;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(
            setupCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &memoryBarrier);
    }

    vulkan.FinishSetupCommandBuffer(setupCommandBuffer);

    // Now ready the image data by mapping it
    {
        auto mappedData = vulkan.GetMemoryManager().Map<const char>(useTemporaryImage ? secondImage : firstImage);
        const char* data = mappedData.data();

        VkImageSubresource imageSubresource{};
        imageSubresource.aspectMask = aspectFlags;

        VkSubresourceLayout subresourceLayout;
        vkGetImageSubresourceLayout(
            vulkan.m_VulkanDevice,
            useTemporaryImage ? secondImage.GetVkBuffer() : firstImage.GetVkBuffer(),
            &imageSubresource,
            &subresourceLayout);

        data += subresourceLayout.offset;
        outputFunction(width, height, format, static_cast<uint32_t>(subresourceLayout.rowPitch), data);

        vulkan.GetMemoryManager().Unmap(useTemporaryImage ? secondImage : firstImage, std::move(mappedData));
    }

    Cleanup();
    return true;

#else
    return false;
#endif // OS_WINDOWS
}

#if 0
//=============================================================================
// Wrap_VkImage
//=============================================================================

//-----------------------------------------------------------------------------
Wrap_VkImage::Wrap_VkImage()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
Wrap_VkImage::~Wrap_VkImage()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
bool Wrap_VkImage::Initialize(Vulkan* pVulkan, const VkImageCreateInfo& ImageInfo, MemoryUsage Usage, const char* pName)
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

    auto& memoryManager = pVulkan->GetMemoryManager();
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
        auto& memoryManager = m_pVulkan->GetMemoryManager();
        memoryManager.Destroy(std::move(m_VmaImage));
    }
    m_pVulkan = nullptr;
    m_ImageInfo = {};
    m_Name.clear();
}
#endif


//=============================================================================
// Wrap_VkSemaphore
//=============================================================================

//-----------------------------------------------------------------------------
Wrap_VkSemaphore::Wrap_VkSemaphore()
//-----------------------------------------------------------------------------
{}
//-----------------------------------------------------------------------------
Wrap_VkSemaphore::~Wrap_VkSemaphore()
//-----------------------------------------------------------------------------
{
    assert(m_Semaphore == VK_NULL_HANDLE || !m_IsOwned);
}

//-----------------------------------------------------------------------------
Wrap_VkSemaphore& Wrap_VkSemaphore::operator=(Wrap_VkSemaphore&& other) noexcept
//-----------------------------------------------------------------------------
{
    if (&other != this)
    {
        m_Name = std::move(other.m_Name);
        assert(m_Semaphore == VK_NULL_HANDLE);
        m_Semaphore = other.m_Semaphore;
        other.m_Semaphore = VK_NULL_HANDLE;
        m_TimelineSemaphore = other.m_TimelineSemaphore;
        other.m_TimelineSemaphore = false;
        m_IsOwned = other.m_IsOwned;
        other.m_IsOwned = false;
    }
    return *this;
}

//-----------------------------------------------------------------------------
Wrap_VkSemaphore::Wrap_VkSemaphore(Wrap_VkSemaphore&& other) noexcept
//-----------------------------------------------------------------------------
{
    *this = std::move(other);
}

//-----------------------------------------------------------------------------
bool Wrap_VkSemaphore::Initialize(Vulkan& rVulkan, const char* pName)
//-----------------------------------------------------------------------------
{
    assert(m_Semaphore == VK_NULL_HANDLE);
    VkSemaphoreTypeCreateInfo TimelineInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                            .initialValue = 0 };
    VkSemaphoreCreateInfo BinarySemaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    auto RetVal = vkCreateSemaphore(rVulkan.m_VulkanDevice, &BinarySemaphoreInfo, NULL, &m_Semaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }
    m_TimelineSemaphore = false;
    m_IsOwned = true;
    if (pName != nullptr)
        m_Name.assign(pName);
    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkSemaphore::InitializeTimeline(Vulkan& rVulkan, uint64_t initialValue, const char* pName)
//-----------------------------------------------------------------------------
{
    assert(m_Semaphore == VK_NULL_HANDLE);
    VkSemaphoreTypeCreateInfo TimelineInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                            .initialValue = initialValue };
    VkSemaphoreCreateInfo TimelineSemaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                 .pNext = &TimelineInfo };

    auto RetVal = vkCreateSemaphore(rVulkan.m_VulkanDevice, &TimelineSemaphoreInfo, NULL, &m_Semaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }
    m_TimelineSemaphore = true;
    m_IsOwned = true;
    if (pName != nullptr)
        m_Name.assign(pName);
    return true;
}

//-----------------------------------------------------------------------------
bool Wrap_VkSemaphore::InitializeExternal(VkSemaphore externalBinarySemaphore, const char* pName)
//-----------------------------------------------------------------------------
{
    assert(m_Semaphore == VK_NULL_HANDLE);
    m_Semaphore = externalBinarySemaphore;
    m_TimelineSemaphore = false;
    m_IsOwned = false;
    if (pName != nullptr)
        m_Name.assign(pName);
    return true;
}

//-----------------------------------------------------------------------------
void Wrap_VkSemaphore::Release(Vulkan& vulkan)
//-----------------------------------------------------------------------------
{
    if (m_IsOwned && m_Semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(vulkan.m_VulkanDevice, m_Semaphore, nullptr);
    m_Semaphore = VK_NULL_HANDLE;
    m_Name.clear();
}
