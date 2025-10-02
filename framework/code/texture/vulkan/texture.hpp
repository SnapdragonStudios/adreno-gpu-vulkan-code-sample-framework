//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "../texture.hpp"
#include "memory/vulkan/memoryMapped.hpp"
#include <vulkan/vulkan.h>
#include <vector>

// Forward declarations
class Vulkan;
template<typename T_GFXAPI, typename T_BUFFER> class MemoryAllocatedBuffer;
using TextureVulkan = TextureT<Vulkan>;
using ImageVulkan = ImageT<Vulkan>;
using ImageViewVulkan = ImageViewT<Vulkan>;
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
    SamplerT() noexcept;
    ~SamplerT() noexcept;
    SamplerT(VkSampler sampler) noexcept;
    SamplerT(SamplerT<Vulkan>&& src) noexcept;
    SamplerT& operator=(SamplerT<Vulkan>&& src) noexcept;
    SamplerT<Vulkan> Copy() const noexcept
    {
        return {m_Sampler};
    }

    auto GetVkSampler() const { return m_Sampler; }
    bool IsEmpty() const { return m_Sampler == VK_NULL_HANDLE; }

private:
    friend void ReleaseSampler<Vulkan>( Vulkan& vulkan, SamplerT<Vulkan>* pSampler );
    VkSampler   m_Sampler;
};


/// @brief Template specialization of sampler container for Vulkan graphics api.
template<>
class ImageViewT<Vulkan> final : public ImageView
{
public:
    ImageViewT() noexcept : m_ImageView( VK_NULL_HANDLE ), m_ImageViewType( ImageViewType::View1D ) {};
    ImageViewT( VkImageView imageView, VkImageViewType viewType ) noexcept;
    ImageViewT( ImageViewT<Vulkan>&& src ) noexcept {
        *this = std::move( src );
    }
    ImageViewT& operator=( ImageViewT<Vulkan>&& src ) noexcept {
        m_ImageView = src.m_ImageView;
        src.m_ImageView = VK_NULL_HANDLE;
        m_ImageViewType = src.m_ImageViewType;
        src.m_ImageViewType = ImageViewType::View1D;
        return *this;
    }
    ~ImageViewT();
    auto GetImageViewType() const { return m_ImageViewType; }
    auto GetVkImageView() const { return m_ImageView; }
    bool IsEmpty() const { return m_ImageView == VK_NULL_HANDLE; }

private:
    friend void ReleaseImageView<Vulkan>( Vulkan& vulkan, ImageViewT<Vulkan>* pImageView );
    VkImageView     m_ImageView;
    ImageViewType   m_ImageViewType;
};


/// @brief Template specialization of image container for Vulkan graphics api.
template<>
class ImageT<Vulkan> final : public Image
{
public:
    ImageT() noexcept {};

    /// @brief Construct ImageT from a pre-existing vmaImage.
    /// @param vmaImage - ownership passed to this ImageT.
    ImageT( MemoryAllocatedBuffer<Vulkan, VkImage> vmaImage ) noexcept;

    /// @brief Construct ImageT from a pre-existing Vulkan image/memory handles.
    /// @param image - ownership NOT passed in to this ImageT, beware of lifetime issues.
    /// @param memory - ownership NOT passed to this ImageT, beware of lifetime issues.
    ImageT( VkImage image, VkDeviceMemory memory ) noexcept;

    ImageT( ImageT<Vulkan>&& src ) noexcept {
        *this = std::move( src );
    }
    ImageT& operator=( ImageT<Vulkan>&& src ) noexcept
    {
        VmaImage = std::move(src.VmaImage);
        Image = src.Image;
        src.Image = VK_NULL_HANDLE;
        Memory = src.Memory;
        src.Memory = VK_NULL_HANDLE;
        return *this;
    }
    ~ImageT();
    VkImage	GetVkImage() const { return VmaImage ? VmaImage.GetVkBuffer() : Image; }
    bool IsEmpty() const { return !VmaImage && Image == VK_NULL_HANDLE; }

private:
    friend void ReleaseImage<Vulkan>( Vulkan& vulkan, ImageT<Vulkan>* pImage );

    // Managed memory buffer allocation and VkImage
    MemoryAllocatedBuffer<Vulkan, VkImage>  VmaImage;

    // Needed for functions handling own memory (i.e. AndroidHardwareBuffers)
    VkImage                                 Image = VK_NULL_HANDLE;
    VkDeviceMemory                          Memory = VK_NULL_HANDLE;
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
    TextureT(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t firstMip, uint32_t faces, uint32_t firstFace, TextureFormat, VkImageLayout imageLayout, ImageT<Vulkan> image, SamplerT<Vulkan> sampler, ImageViewT<Vulkan> imageView) noexcept;

    /// @brief Construct TextureT from a pre-existing Vulkan image/memory handles.
    /// @param image - ownership NOT passed in to this TextureT, beware of lifetime issues.
    /// @param sampler - ownership passed to this TextureT.
    /// @param imageView - ownership passed to this TextureT.
    TextureT(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t firstMip, uint32_t faces, uint32_t firstFace, TextureFormat format, VkImageLayout imageLayout, VkImage image, VkDeviceMemory memory, SamplerT<Vulkan> sampler, ImageViewT<Vulkan> imageView) noexcept;

    void Release(GraphicsApiBase* pGraphicsApi) override;

    VkImage				  GetVkImage() const { return Image.GetVkImage(); }
    VkDescriptorImageInfo GetVkDescriptorImageInfo() const { return { Sampler.GetVkSampler(), ImageView.GetVkImageView(), ImageLayout }; }
    VkImageLayout		  GetVkImageLayout() const { return ImageLayout; }
    VkSampler			  GetVkSampler() const { return Sampler.GetVkSampler(); }
    VkImageView			  GetVkImageView() const { return ImageView.GetVkImageView(); }
    bool				  IsEmpty() const { return Image.IsEmpty() || Sampler.IsEmpty(); }

    uint32_t  Width = 0;
    uint32_t  Height = 0;
    uint32_t  Depth = 0;
    uint32_t  MipLevels = 0;
    uint32_t  FirstMip = 0;
    uint32_t  Faces = 0;
    uint32_t  FirstFace = 0;
    TextureFormat  Format = TextureFormat::UNDEFINED;
    VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

public:
    ImageT<Vulkan>                          Image;
    SamplerT<Vulkan>                        Sampler;
    ImageViewT<Vulkan>                      ImageView;
};


/// Template specialization for Vulkan CreateTextureObject
template<>
TextureT<Vulkan> CreateTextureObject<Vulkan>( Vulkan&, const CreateTexObjectInfo& texInfo );

/// Template specialization for Vulkan CreateTextureFromBuffer
template<>
TextureT<Vulkan> CreateTextureFromBuffer<Vulkan>( Vulkan&, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName );

/// Create a texture that views (aliases) another texture but using a different texture format (must be 'related' formats, which formats are related is dependant on graphics api)
TextureT<Vulkan> CreateTextureObjectView( Vulkan&, const TextureT<Vulkan>& original, TextureFormat viewFormat );

/// Template specialization for Vulkan ReleaseTexture
template<>
void ReleaseTexture<Vulkan>( Vulkan& vulkan, TextureT<Vulkan>* );

/// Template specialization for Vulkan ReleaseImage
template<>
void ReleaseImage<Vulkan>( Vulkan& vulkan, ImageT<Vulkan>* );

/// Template specialization for Vulkan CreateImageView
template<>
ImageViewT<Vulkan> CreateImageView( Vulkan&, const ImageT<Vulkan>& image, TextureFormat format, uint32_t numMipLevels, uint32_t baseMipLevel, uint32_t numFaces, uint32_t firstFace, ImageViewType viewType );

/// Template specialization for Vulkan ReleaseImageView
template<>
void ReleaseImageView<Vulkan>( Vulkan& vulkan, ImageViewT<Vulkan>* );


/// Template specialization for Vulkan CreateSampler
template<>
SamplerT<Vulkan> CreateSampler( Vulkan&, const CreateSamplerObjectInfo& );

/// Template specialization for Vulkan ReleaseSampler
template<>
void ReleaseSampler<Vulkan>( Vulkan& vulkan, SamplerT<Vulkan>* );

/// Helper to take a sorce texture and make an array of textures where each one points to a single mip in the source
std::vector<TextureT<Vulkan>> MakeTextureMipViews( Vulkan& vulkan, const TextureT<Vulkan>& source, uint32_t maxMips );
