//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

///
/// KTX image file loading for vulkan
/// 

#include "../loaderKtx.hpp"
#include "memory/vulkan/memoryMapped.hpp"
#include "texture.hpp"
#include <vulkan/vulkan.h>
#include <set>
#include <memory>

// Forward declarations
struct ktxVulkanDeviceInfo;
struct ktxTexture;
class Vulkan;
template<typename T_GFXAPI> class TextureT;
template<typename T_GFXAPI> class SamplerT;
using TextureVulkan = TextureT<Vulkan>;


/// @brief Class to handle loading KTX textures in to Vulkan memory
template<>
class TextureKtxT<Vulkan> : public TextureKtx
{
    TextureKtxT(const TextureKtxT<Vulkan>&) = delete;
    TextureKtxT& operator=(const TextureKtxT<Vulkan>&) = delete;
public:
    TextureKtxT(Vulkan& vulkan) noexcept;
    ~TextureKtxT() noexcept;

    /// @brief Initialize this loader class
    /// @return true on success
    bool Initialize() override;

    /// @brief Release (de-initialize) back to a clean (initializable) state
    void Release() override;

    /// @brief Do the necessary upload etc to go from a cpu texture representation to Vulkan format
    /// @param textureFile ktx file data we want to load as a vulkan texture
    /// @param sampler sampler that loaded texture will take OWNERSHIP of, may be VK_NULL_HANDLE (in which case LoadKtx creates an appropriate sampler)
    /// @returns a &TextureVulkan, will be empty on failure
    TextureVulkan LoadKtx(Vulkan& vulkan, const TextureKtxFileWrapper& textureFile, const SamplerT<Vulkan>& sampler);

    /// @brief Load a ktx file and do the necessary upload etc to go from a cpu texture representation to Vulkan format
    /// @param filename of ktx (or ktx2) format file we want to load as a vulkan texture
    /// @param sampler sampler that loaded texture will take OWNERSHIP of, may be VK_NULL_HANDLE (in which case LoadKtx creates an appropriate sampler)
    /// @returns a &TextureVulkan, will be empty on failure
    TextureVulkan LoadKtx(Vulkan& vulkan, AssetManager& assetManager, const char* const pFileName, const SamplerT<Vulkan>& sampler);

    /// @brief Run the Ktx2 transcoding step (if needed) 
    /// Will do nothing for textures that do not need transcoding.
    /// Performance will depend on ktx2 texture size and intermediate encoding format.
    /// @param fileData ktx file data loaded by TextureKtx::LoadData or similar.
    /// @return transcoded Ktx texture.
    TextureKtxFileWrapper Transcode(TextureKtxFileWrapper&& fileData);

protected:
    // Callbacks to override the allocation/bind functions used by ktxTexture_VkUploadEx
    static VkResult VkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory);
    static void VkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator);
    static VkResult VkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset);
    static void VkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator);

    uint32_t DetermineTranscodeOutputFormat() const;

private:
    Vulkan& m_Vulkan;
    std::unique_ptr<ktxVulkanDeviceInfo> m_VulkanDeviceInfo;
    std::set<MemoryAllocatedBuffer<Vulkan, VkBuffer>, std::less<>> m_AllocatedBuffers;
    std::set<MemoryAllocatedBuffer<Vulkan, VkImage>, std::less<>> m_AllocatedImages;
    std::set<MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>, std::less<>> m_AllocatedMemory;

    // Format support flags
    bool m_SupportsBcCompression = false;
    bool m_SupportsAstcCompression = false;
    bool m_SupportsEtc2Compression = false;
    /// @brief determine if want to use BC compression formats for transcoded textures (if false we will prefer ASTC and then ETC2 before trying BC)
    const bool m_FavorBcCompression = true;
};
