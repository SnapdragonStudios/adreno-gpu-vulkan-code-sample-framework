//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "../texture.hpp"
#include "memory/vulkan/memoryMapped.hpp"
#include <vulkan/vulkan.h>

// Forward declarations
class Vulkan;
template<typename T_GFXAPI, typename T_BUFFER> class MemoryAllocatedBuffer;
using TextureVulkan = TextureT<Vulkan>;
using SamplerVulkan = SamplerT<Vulkan>;

/// @brief Convert from our TextureFormat to Vulkan's VkFormat
extern VkFormat TextureFormatToVk(TextureFormat);
/// @brief Convert from our Vulkan's VkFormat to our TextureFormat.
extern TextureFormat VkToTextureFormat(VkFormat);


/// @brief Template specialization of sampler container for Vulkan graphics api.
template<>
class SamplerT<Vulkan> final : public Sampler
{
public:
    SamplerT() noexcept : m_Sampler(VK_NULL_HANDLE) {};
    SamplerT(VkSampler sampler) noexcept : m_Sampler(sampler) {};
    SamplerT(const SamplerT<Vulkan>& src) noexcept : m_Sampler(src.m_Sampler) {};
    SamplerT<Vulkan>& operator=(const SamplerT<Vulkan>& src) noexcept {
        if (this != &src)
            m_Sampler = src.m_Sampler;
        return *this;
    }
    auto GetVkSampler() const { return m_Sampler; }
    bool IsEmpty() const { return m_Sampler == VK_NULL_HANDLE; }

private:

    friend void ReleaseSampler<Vulkan>( Vulkan& vulkan, SamplerT<Vulkan>* pSampler );
    VkSampler   m_Sampler;
};


/// @brief Template specialization of texture container for Vulkan graphics api.
template<>
class TextureT<Vulkan> final : public Texture
{
public:
    TextureT() noexcept;
    TextureT(const TextureT<Vulkan>&) = delete;
    TextureT& operator=(const TextureT<Vulkan>&) = delete;
    TextureT(TextureT<Vulkan>&&) noexcept;
    TextureT& operator=(TextureT<Vulkan>&&) noexcept;
    ~TextureT() noexcept;

    /// @brief Construct TextureT from a pre-existing vmaImage.
    /// @param vmaImage - ownership passed to this TextureT.
    /// @param sampler - ownership passed to this TextureT.
    /// @param imageView - ownership passed to this TextureT.
    TextureT(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, TextureFormat, VkImageLayout imageLayout, MemoryAllocatedBuffer<Vulkan, VkImage> vmaImage, const SamplerT<Vulkan>& sampler, VkImageView imageView) noexcept;

    /// @brief Construct TextureT from a pre-existing Vulkan image/memory handles.
    /// @param image - ownership NOT passed in to this TextureT, beware of lifetime issues.
    /// @param sampler - ownership passed to this TextureT.
    /// @param imageView - ownership passed to this TextureT.
    TextureT(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t firstMip, TextureFormat format, VkImageLayout imageLayout, VkImage image, VkDeviceMemory memory, const SamplerT<Vulkan>& sampler, VkImageView imageView) noexcept;

    void Release(GraphicsApiBase* pGraphicsApi) override;

    VkImage				  GetVkImage() const { return VmaImage ? VmaImage.GetVkBuffer() : Image; }
    VkDescriptorImageInfo GetVkDescriptorImageInfo() const { return { Sampler.GetVkSampler(), ImageView, ImageLayout }; }
    VkImageLayout		  GetVkImageLayout() const { return ImageLayout; }
    VkSampler			  GetVkSampler() const { return Sampler.GetVkSampler(); }
    VkImageView			  GetVkImageView() const { return ImageView; }
    bool				  IsEmpty() const { return !VmaImage; }

    uint32_t  Width = 0;
    uint32_t  Height = 0;
    uint32_t  Depth = 0;
    uint32_t  MipLevels = 0;
    uint32_t  FirstMip = 0;
    TextureFormat  Format = TextureFormat::UNDEFINED;
    VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

public:

    MemoryAllocatedBuffer<Vulkan, VkImage>  VmaImage;

    SamplerT<Vulkan>                        Sampler;
    VkImageView                             ImageView = VK_NULL_HANDLE;

    // Needed for functions handling own memory (i.e. AndroidHardwareBuffers)
    VkImage                                 Image = VK_NULL_HANDLE;
    VkDeviceMemory                          Memory = VK_NULL_HANDLE;
};


/// Helper to create a Vulkan VkImageView object (user is expected to release the object)
extern bool CreateImageView(Vulkan&, VkImage image, VkFormat format, uint32_t baseMipLevel, uint32_t numMipLevels, uint32_t numFaces, VkImageViewType viewType, VkImageView* pRetImageView);


/// Template specialization for Vulkan CreateTextureObject
template<>
TextureT<Vulkan> CreateTextureObject<Vulkan>( Vulkan&, const CreateTexObjectInfo& texInfo );

/// Template specialization for Vulkan CreateTextureFromBuffer
template<>
TextureT<Vulkan> CreateTextureFromBuffer<Vulkan>( Vulkan&, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName, uint32_t extraFlags );

/// Create a texture that views (aliases) another texture but using a different texture format (must be 'related' formats, which formats are related is dependant on graphics api)
TextureT<Vulkan> CreateTextureObjectView( Vulkan&, const TextureT<Vulkan>& original, TextureFormat viewFormat );

template<>
void ReleaseTexture<Vulkan>( Vulkan& vulkan, TextureT<Vulkan>* );

/// Template specialization for Vulkan CreateSampler
template<>
SamplerT<Vulkan> CreateSampler( Vulkan&, const CreateSamplerObjectInfo& );

/// Template specialization for Vulkan ReleaseSampler
template<>
void ReleaseSampler<Vulkan>( Vulkan& vulkan, SamplerT<Vulkan>* );
