//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include "loaderKtx.hpp"
#include <ktxvulkan.h>  // KTX-Software


// Static
// Set to TextureKtxVulkan before calling upload (for the callbacks to use).  Cleaner than hacking on the vulkan allocator, although still icky.
static thread_local TextureKtxT<Vulkan>* sUploadingTextureKtxVulkan = nullptr;


TextureKtxT<Vulkan>::TextureKtxT(Vulkan& vulkan) noexcept : TextureKtx(), m_Vulkan(vulkan)
{
}

TextureKtxT<Vulkan>::~TextureKtxT() noexcept
{}

bool TextureKtxT<Vulkan>::Initialize()
{
    if (!TextureKtx::Initialize())
        return false;

    // Setup the ktx library's device info (ready for subsequent ktx texture data transfers to the GPU)
    m_VulkanDeviceInfo = std::make_unique<ktxVulkanDeviceInfo>();
    const auto transferQueueEnum = /*vulkan.m_VulkanQueues[Vulkan::eTransferQueue].QueueFamilyIndex != -1 ? Vulkan::eTransferQueue : */ Vulkan::eGraphicsQueue;
    const auto queue = m_Vulkan.m_VulkanQueues[transferQueueEnum].Queue;
    const auto pool  = m_Vulkan.m_VulkanQueues[transferQueueEnum].CommandPool;
    if (KTX_SUCCESS != ktxVulkanDeviceInfo_Construct(m_VulkanDeviceInfo.get(), m_Vulkan.m_VulkanGpu, m_Vulkan.m_VulkanDevice, queue, pool, nullptr))
        return false;

    // Callbacks to override the allocation/bind functions used by ktxTexture_VkUploadEx
    // We want to use our memorymanager (and the VMA allocator under it) to do vulkan allocations so the allocations play nicely with the TextureT<Vulkan> container)
    m_VulkanDeviceInfo->vkFuncs.vkAllocateMemory = TextureKtxT<Vulkan>::VkAllocateMemory;
    m_VulkanDeviceInfo->vkFuncs.vkFreeMemory = TextureKtxT<Vulkan>::VkFreeMemory;
    m_VulkanDeviceInfo->vkFuncs.vkDestroyImage = TextureKtxT<Vulkan>::VkDestroyImage;
    m_VulkanDeviceInfo->vkFuncs.vkBindImageMemory = TextureKtxT<Vulkan>::VkBindImageMemory;

    const auto& gpuFeatures = m_Vulkan.GetGpuFeatures().Base.features;
    if (gpuFeatures.textureCompressionBC)
        m_SupportsBcCompression = true;
    if (gpuFeatures.textureCompressionASTC_LDR)
        m_SupportsAstcCompression = true;
    if (gpuFeatures.textureCompressionETC2)
        m_SupportsEtc2Compression = true;

    return true;
}

void TextureKtxT<Vulkan>::Release()
{
    if (m_VulkanDeviceInfo)
        ktxVulkanDeviceInfo_Destruct(m_VulkanDeviceInfo.get());
    m_VulkanDeviceInfo.release();   // we didnt own the pointer!

    TextureKtx::Release();
}

uint32_t TextureKtxT<Vulkan>::DetermineTranscodeOutputFormat() const
{
    if (m_SupportsBcCompression && (m_FavorBcCompression || (!m_SupportsAstcCompression && !m_SupportsEtc2Compression)))
        return KTX_TTF_BC7_RGBA;   // should be better quality than KTX_TTF_BC1_OR_3 but may take longer to transcode (untested)
    else if (m_SupportsAstcCompression)
        return KTX_TTF_ASTC_4x4_RGBA;
    else if (m_SupportsEtc2Compression)
        return KTX_TTF_ETC;
    else
        // Bruteforce fallback (memory hungry)
        return KTX_TTF_RGBA32;
}

TextureKtxFileWrapper TextureKtxT<Vulkan>::Transcode(TextureKtxFileWrapper&& fileData)
{
    auto* const pKtxData = GetKtxTexture(fileData);
    if (pKtxData!=nullptr && ktxTexture_NeedsTranscoding(pKtxData) && pKtxData->classId == class_id::ktxTexture2_c)
    {
        auto pKtx2Data = (ktxTexture2* const)pKtxData;
        if (KTX_SUCCESS != ktxTexture2_TranscodeBasis(pKtx2Data, (ktx_transcode_fmt_e) DetermineTranscodeOutputFormat(), (ktx_transcode_flag_bits_e)0))
        {
            return {};
        }
    }
    return std::move(fileData);
}

TextureVulkan TextureKtxT<Vulkan>::LoadKtx(Vulkan& vulkan, const TextureKtxFileWrapper& fileData, const SamplerT<Vulkan>& sampler)
{
    auto* const pKtxData = GetKtxTexture(fileData);
    if (!pKtxData)
        return {};
    if (ktxTexture_NeedsTranscoding(pKtxData) && pKtxData->classId == class_id::ktxTexture2_c)
    {
        auto pKtx2Data = (ktxTexture2* const)pKtxData;
        if (KTX_SUCCESS != ktxTexture2_TranscodeBasis(pKtx2Data, (ktx_transcode_fmt_e) DetermineTranscodeOutputFormat(), (ktx_transcode_flag_bits_e)0))
        {
            return {};
        }
    }

    ktxVulkanTexture uploadedTexture{};

    sUploadingTextureKtxVulkan = this;
    if (KTX_SUCCESS != ktxTexture_VkUploadEx(GetKtxTexture(fileData), m_VulkanDeviceInfo.get(), &uploadedTexture,
                                             VK_IMAGE_TILING_OPTIMAL,
                                             VK_IMAGE_USAGE_SAMPLED_BIT,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
        return {};
    }
    sUploadingTextureKtxVulkan = nullptr;

    VkImageView imageView;
    if (!CreateImageView(vulkan, uploadedTexture.image, uploadedTexture.imageFormat, 0, uploadedTexture.levelCount, uploadedTexture.layerCount, uploadedTexture.viewType, &imageView))
    {
        ktxVulkanTexture_Destruct(&uploadedTexture, vulkan.m_VulkanDevice, nullptr);
        return {};
    }
    //SamplerT<Vulkan>& samplerVulkan = static_cast<const SamplerT<Vulkan>&>(sampler);
    //if (samplerVulkan.IsEmpty())
    //{
    //    if (!CreateSampler(&vulkan, VK_SAMPLER_ADDRESS_MODE_REPEAT, SamplerFilter::Linear, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, false, 0.0f, &sampler))
    //    {
    //        vkDestroyImageView(vulkan.m_VulkanDevice, imageView, nullptr);
    //        ktxVulkanTexture_Destruct(&uploadedTexture, vulkan.m_VulkanDevice, nullptr);
    //        return {};
    //    }
    //}

    TextureFormat textureFormat = VkToTextureFormat(uploadedTexture.imageFormat);

    // Take ownership of the image from the container holding images created during ktxTexture_VkUploadEx
    auto allocatedImage = std::move(m_AllocatedImages.extract(m_AllocatedImages.find(uploadedTexture.image)).value());
    // Return the fully formed texture object
    TextureVulkan texture{ uploadedTexture.width, uploadedTexture.height, uploadedTexture.depth, uploadedTexture.levelCount, textureFormat, uploadedTexture.imageLayout, std::move(allocatedImage), sampler, imageView };
    return texture;
}

TextureVulkan TextureKtxT<Vulkan>::LoadKtx( Vulkan& vulkan, AssetManager& assetManager, const char* const pFileName, const SamplerT<Vulkan>& sampler )
{
    auto ktxData = LoadFile( assetManager, pFileName );
    if (!ktxData)
        return {};
    return LoadKtx( vulkan, ktxData, sampler );
}

// Comparison functions so we can look for VkBuffer in a set of MemoryAllocatedBuffer<Vulkan, VkBuffer>
static bool operator<(const MemoryAllocatedBuffer<Vulkan, VkBuffer>& a, const MemoryAllocatedBuffer<Vulkan, VkBuffer>& b) { return a.GetVkBuffer() < b.GetVkBuffer(); }
static bool operator<(const VkBuffer& a, const MemoryAllocatedBuffer<Vulkan, VkBuffer>& b) { return a < b.GetVkBuffer(); }
static bool operator<(const MemoryAllocatedBuffer<Vulkan, VkBuffer>& a, const VkBuffer& b) { return a.GetVkBuffer() < b; }
// Comparison functions so we can look for VkImage in a set of MemoryAllocatedBuffer<Vulkan, VkImage>
static bool operator<(const MemoryAllocatedBuffer<Vulkan, VkImage>& a, const MemoryAllocatedBuffer<Vulkan, VkImage>& b) { return a.GetVkBuffer() < b.GetVkBuffer(); }
static bool operator<(const VkImage& a, const MemoryAllocatedBuffer<Vulkan, VkImage>& b) { return a < b.GetVkBuffer(); }
static bool operator<(const MemoryAllocatedBuffer<Vulkan, VkImage>& a, const VkImage& b) { return a.GetVkBuffer() < b; }
// Comparison functions so we can look for VkDeviceMemory in a set of MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>
static bool operator<(const MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>& a, const MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>& b) { return a.GetVkBuffer() < b.GetVkBuffer(); }
static bool operator<(const VkDeviceMemory& a, const MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>& b) { return a < b.GetVkBuffer(); }
static bool operator<(const MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>& a, const VkDeviceMemory& b) { return a.GetVkBuffer() < b; }

/*static*/ VkResult TextureKtxT<Vulkan>::VkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
    if (sUploadingTextureKtxVulkan)
    {
        uint32_t memoryTypeBits = 1u << pAllocateInfo->memoryTypeIndex;
        auto allocatedMemory = sUploadingTextureKtxVulkan->m_Vulkan.GetMemoryManager().AllocateMemory(pAllocateInfo->allocationSize, memoryTypeBits);
        if (allocatedMemory)
        {
            *pMemory = allocatedMemory.GetVkBuffer();
            sUploadingTextureKtxVulkan->m_AllocatedMemory.emplace(std::move(allocatedMemory));
            return VK_SUCCESS;
        }
    }
    return vkAllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

/*static*/ void TextureKtxT<Vulkan>::VkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
    if (sUploadingTextureKtxVulkan)
    {
        auto& memoryAllocations = sUploadingTextureKtxVulkan->m_AllocatedMemory;
        auto it = memoryAllocations.find(memory);
        if (it != memoryAllocations.end())
        {
            auto allocatedMemory = std::move(memoryAllocations.extract(it).value());
            assert(allocatedMemory);
            sUploadingTextureKtxVulkan->m_Vulkan.GetMemoryManager().Destroy(std::move(allocatedMemory));
        }
        else
        {
            // We expect the allocation to be in the m_AllocatedMemory container... maybe vkDestroyImage was not called?
            assert(0);
        }
    }
    else
        return VkFreeMemory(device, memory, pAllocator);
}

/*static*/ VkResult TextureKtxT<Vulkan>::VkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    if (sUploadingTextureKtxVulkan)
    {
        auto& memoryAllocations = sUploadingTextureKtxVulkan->m_AllocatedMemory;
        auto allocatedMemory = std::move(memoryAllocations.extract(memoryAllocations.find(memory)).value());
        if (allocatedMemory)
        {
            auto imageBuffer = sUploadingTextureKtxVulkan->m_Vulkan.GetMemoryManager().BindImageToMemory(image, std::move(allocatedMemory));
            sUploadingTextureKtxVulkan->m_AllocatedImages.emplace(std::move(imageBuffer));
            return VK_SUCCESS;
        }
        return VK_ERROR_UNKNOWN;
    }
    else
        return vkBindImageMemory(device, image, memory, memoryOffset);
}

/*static*/ void TextureKtxT<Vulkan>::VkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
    if (sUploadingTextureKtxVulkan && image != VK_NULL_HANDLE)
    {
        auto& images = sUploadingTextureKtxVulkan->m_AllocatedImages;
        auto allocatedImage = std::move(images.extract(images.find(image)).value());
        if (allocatedImage)
        {
            auto allocatedMemory = sUploadingTextureKtxVulkan->m_Vulkan.GetMemoryManager().DestroyBufferOnly(std::move(allocatedImage));
            sUploadingTextureKtxVulkan->m_AllocatedMemory.emplace(std::move(allocatedMemory));
        }
    }
    else
        vkDestroyImage(device, image, pAllocator);
}

/// @brief Function specialization
template<>
TextureT<Vulkan> TextureKtx::LoadKtx( Vulkan& vulkan, const TextureKtxFileWrapper& textureFile, const SamplerT<Vulkan>& sampler )
{
    return apiCast<Vulkan>( this )->LoadKtx( vulkan, textureFile, sampler );
}

/// @brief Function specialization
template<>
TextureT<Vulkan> TextureKtx::LoadKtx( Vulkan& vulkan, AssetManager& assetManager, const char* const pFileName, const SamplerT<Vulkan>& sampler )
{
    return apiCast<Vulkan>( this )->LoadKtx( vulkan, assetManager, pFileName, sampler );
}
