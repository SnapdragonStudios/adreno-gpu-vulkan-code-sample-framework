//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "memory/vulkan/memoryMapped.hpp"
#include "texture.hpp"
#include "imageWrapper.hpp"
#include "vulkan/vulkan.hpp"

constexpr VkFilter EnumToVk(SamplerFilter s) { assert(s != SamplerFilter::Undefined); return VkFilter(int(s) - 1); }
static_assert(VK_FILTER_NEAREST == int(SamplerFilter::Nearest) - 1);
static_assert(VK_FILTER_LINEAR == int(SamplerFilter::Linear) - 1);

constexpr VkSamplerAddressMode EnumToVk(SamplerAddressMode s) { assert(s != SamplerAddressMode::Undefined); return VkSamplerAddressMode(int(s) - 1); }
static_assert(VK_SAMPLER_ADDRESS_MODE_REPEAT == int(SamplerAddressMode::Repeat) -1);
static_assert(VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT == int(SamplerAddressMode::MirroredRepeat) - 1);
static_assert(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE == int(SamplerAddressMode::ClampEdge) - 1);
static_assert(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER == int(SamplerAddressMode::ClampBorder) - 1);
static_assert(VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE == int(SamplerAddressMode::MirroredClampEdge) - 1);

constexpr VkBorderColor EnumToVk(SamplerBorderColor s) { return VkBorderColor(int(s)); }
static_assert(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK == int(SamplerBorderColor::TransparentBlackFloat));
static_assert(VK_BORDER_COLOR_INT_TRANSPARENT_BLACK == int(SamplerBorderColor::TransparentBlackInt));
static_assert(VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK == int(SamplerBorderColor::OpaqueBlackFloat));
static_assert(VK_BORDER_COLOR_INT_OPAQUE_BLACK == int(SamplerBorderColor::OpaqueBlackInt));
static_assert(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE == int(SamplerBorderColor::OpaqueWhiteFloat));
static_assert(VK_BORDER_COLOR_INT_OPAQUE_WHITE == int(SamplerBorderColor::OpaqueWhiteInt));


TextureT<Vulkan>::TextureT() noexcept : VmaImage()
{}

TextureT<Vulkan>::TextureT(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, TextureFormat format, VkImageLayout imageLayout, MemoryAllocatedBuffer<Vulkan, VkImage> vmaImage, const SamplerT<Vulkan>& sampler, VkImageView imageView) noexcept
    : Texture()
    , Width(width)
    , Height(height)
    , Depth(depth)
    , MipLevels(mipLevels)
    , Format(format)
    , ImageLayout(imageLayout)
    , VmaImage(std::move(vmaImage))
    , Sampler(sampler)
    , ImageView(imageView)
{
}

/// @brief Construct TextureT from a pre-existing Vulkan image/memory handles.
/// @param image - ownership NOT passed in to this TextureT, beware of lifetime issues.
/// @param sampler - ownership passed to this TextureT.
/// @param imageView - ownership passed to this TextureT.
TextureT<Vulkan>::TextureT(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t firstMip, TextureFormat format, VkImageLayout imageLayout, VkImage image, VkDeviceMemory memory, const SamplerT<Vulkan>& sampler, VkImageView imageView) noexcept
    : Texture()
    , Width(width)
    , Height(height)
    , Depth(1)
    , MipLevels(mipLevels)
    , FirstMip(firstMip)
    , Format(format)
    , ImageLayout(imageLayout)
    , Sampler(sampler)
    , ImageView(imageView)
    , Image(image)
    , Memory(memory)
{
}

//
// Constructors/move-operators for TextureT<Vulkan>.
// Ensures we are not leaking owned members.
//
TextureT<Vulkan>::TextureT(TextureT<Vulkan>&& other) noexcept
{
    *this = std::move(other);
}
TextureT<Vulkan>& TextureT<Vulkan>::operator=(TextureT<Vulkan>&& other) noexcept
{
    if (this != &other)
    {
        // Some of the data can be copied and is ok to leave 'other' alone (move operator just has to ensure the 'other' is in a valid state and can be safely deleted)
        Width = other.Width;
        Height = other.Height;
        Depth = other.Depth;
        MipLevels = other.MipLevels;
        FirstMip = other.FirstMip;
        Format = other.Format;
        ImageLayout = other.ImageLayout;
        // Actually transfer ownership from 'other'
        VmaImage = std::move(other.VmaImage);
        Sampler = other.Sampler;
        other.Sampler = VK_NULL_HANDLE;
        ImageView = other.ImageView;
        other.ImageView = VK_NULL_HANDLE;
        Image = other.Image;
        other.Image = VK_NULL_HANDLE;
        Memory = other.Memory;
        other.Memory = VK_NULL_HANDLE;
    }
    return *this;
}

TextureT<Vulkan>::~TextureT() noexcept
{
    // Asserts to ensure we called ReleaseTexture on this already.
    assert(!VmaImage);
    assert(Sampler.IsEmpty());
    assert(ImageView == VK_NULL_HANDLE);
}

void TextureT<Vulkan>::Release(GraphicsApiBase* pGraphicsApi)
{
    auto* pVulkan = static_cast<Vulkan*>(pGraphicsApi);

    if (ImageView != VK_NULL_HANDLE)
        vkDestroyImageView(pVulkan->m_VulkanDevice, ImageView, NULL);
    ImageView = VK_NULL_HANDLE;

    //if (Sampler != VK_NULL_HANDLE)
    //    vkDestroySampler(pVulkan->m_VulkanDevice, Sampler, NULL);
    //Sampler = VK_NULL_HANDLE;
    Sampler = {};

    if (VmaImage)
        pVulkan->GetMemoryManager().Destroy(std::move(VmaImage));

    Image = VK_NULL_HANDLE;
    Memory = VK_NULL_HANDLE;
}


static_assert(int(SamplerAddressMode::Repeat) == VK_SAMPLER_ADDRESS_MODE_REPEAT + 1);
static_assert(int(SamplerAddressMode::MirroredRepeat) == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT + 1);
static_assert(int(SamplerAddressMode::ClampEdge) == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE + 1);
static_assert(int(SamplerAddressMode::ClampBorder) == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER + 1);
static_assert(int(SamplerAddressMode::MirroredClampEdge) == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE + 1);

static_assert(int(SamplerFilter::Nearest) == VK_FILTER_NEAREST + 1);
static_assert(int(SamplerFilter::Linear) == VK_FILTER_LINEAR + 1);

static_assert(int(SamplerBorderColor::TransparentBlackFloat) == VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
static_assert(int(SamplerBorderColor::TransparentBlackInt) == VK_BORDER_COLOR_INT_TRANSPARENT_BLACK);
static_assert(int(SamplerBorderColor::OpaqueBlackFloat) == VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
static_assert(int(SamplerBorderColor::OpaqueBlackInt) == VK_BORDER_COLOR_INT_OPAQUE_BLACK);
static_assert(int(SamplerBorderColor::OpaqueWhiteFloat) == VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
static_assert(int(SamplerBorderColor::OpaqueWhiteInt) == VK_BORDER_COLOR_INT_OPAQUE_WHITE);

static constexpr std::array<VkFormat, 247> sTextureFormatToVk {
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R4G4_UNORM_PACK8,
    VK_FORMAT_R4G4B4A4_UNORM_PACK16,
    VK_FORMAT_B4G4R4A4_UNORM_PACK16,
    VK_FORMAT_R5G6B5_UNORM_PACK16,
    VK_FORMAT_B5G6R5_UNORM_PACK16,
    VK_FORMAT_R5G5B5A1_UNORM_PACK16,
    VK_FORMAT_B5G5R5A1_UNORM_PACK16,
    VK_FORMAT_A1R5G5B5_UNORM_PACK16,
    VK_FORMAT_R8_UNORM,
    VK_FORMAT_R8_SNORM,
    VK_FORMAT_R8_USCALED,
    VK_FORMAT_R8_SSCALED,
    VK_FORMAT_R8_UINT,
    VK_FORMAT_R8_SINT,
    VK_FORMAT_R8_SRGB,
    VK_FORMAT_R8G8_UNORM,
    VK_FORMAT_R8G8_SNORM,
    VK_FORMAT_R8G8_USCALED,
    VK_FORMAT_R8G8_SSCALED,
    VK_FORMAT_R8G8_UINT,
    VK_FORMAT_R8G8_SINT,
    VK_FORMAT_R8G8_SRGB,
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_R8G8B8_SNORM,
    VK_FORMAT_R8G8B8_USCALED,
    VK_FORMAT_R8G8B8_SSCALED,
    VK_FORMAT_R8G8B8_UINT,
    VK_FORMAT_R8G8B8_SINT,
    VK_FORMAT_R8G8B8_SRGB,
    VK_FORMAT_B8G8R8_UNORM,
    VK_FORMAT_B8G8R8_SNORM,
    VK_FORMAT_B8G8R8_USCALED,
    VK_FORMAT_B8G8R8_SSCALED,
    VK_FORMAT_B8G8R8_UINT,
    VK_FORMAT_B8G8R8_SINT,
    VK_FORMAT_B8G8R8_SRGB,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R8G8B8A8_SNORM,
    VK_FORMAT_R8G8B8A8_USCALED,
    VK_FORMAT_R8G8B8A8_SSCALED,
    VK_FORMAT_R8G8B8A8_UINT,
    VK_FORMAT_R8G8B8A8_SINT,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_B8G8R8A8_SNORM,
    VK_FORMAT_B8G8R8A8_USCALED,
    VK_FORMAT_B8G8R8A8_SSCALED,
    VK_FORMAT_B8G8R8A8_UINT,
    VK_FORMAT_B8G8R8A8_SINT,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_A8B8G8R8_UNORM_PACK32,
    VK_FORMAT_A8B8G8R8_SNORM_PACK32,
    VK_FORMAT_A8B8G8R8_USCALED_PACK32,
    VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
    VK_FORMAT_A8B8G8R8_UINT_PACK32,
    VK_FORMAT_A8B8G8R8_SINT_PACK32,
    VK_FORMAT_A8B8G8R8_SRGB_PACK32,
    VK_FORMAT_A2R10G10B10_UNORM_PACK32,
    VK_FORMAT_A2R10G10B10_SNORM_PACK32,
    VK_FORMAT_A2R10G10B10_USCALED_PACK32,
    VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
    VK_FORMAT_A2R10G10B10_UINT_PACK32,
    VK_FORMAT_A2R10G10B10_SINT_PACK32,
    VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    VK_FORMAT_A2B10G10R10_SNORM_PACK32,
    VK_FORMAT_A2B10G10R10_USCALED_PACK32,
    VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
    VK_FORMAT_A2B10G10R10_UINT_PACK32,
    VK_FORMAT_A2B10G10R10_SINT_PACK32,
    VK_FORMAT_R16_UNORM,
    VK_FORMAT_R16_SNORM,
    VK_FORMAT_R16_USCALED,
    VK_FORMAT_R16_SSCALED,
    VK_FORMAT_R16_UINT,
    VK_FORMAT_R16_SINT,
    VK_FORMAT_R16_SFLOAT,
    VK_FORMAT_R16G16_UNORM,
    VK_FORMAT_R16G16_SNORM,
    VK_FORMAT_R16G16_USCALED,
    VK_FORMAT_R16G16_SSCALED,
    VK_FORMAT_R16G16_UINT,
    VK_FORMAT_R16G16_SINT,
    VK_FORMAT_R16G16_SFLOAT,
    VK_FORMAT_R16G16B16_UNORM,
    VK_FORMAT_R16G16B16_SNORM,
    VK_FORMAT_R16G16B16_USCALED,
    VK_FORMAT_R16G16B16_SSCALED,
    VK_FORMAT_R16G16B16_UINT,
    VK_FORMAT_R16G16B16_SINT,
    VK_FORMAT_R16G16B16_SFLOAT,
    VK_FORMAT_R16G16B16A16_UNORM,
    VK_FORMAT_R16G16B16A16_SNORM,
    VK_FORMAT_R16G16B16A16_USCALED,
    VK_FORMAT_R16G16B16A16_SSCALED,
    VK_FORMAT_R16G16B16A16_UINT,
    VK_FORMAT_R16G16B16A16_SINT,
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_FORMAT_R32_UINT,
    VK_FORMAT_R32_SINT,
    VK_FORMAT_R32_SFLOAT,
    VK_FORMAT_R32G32_UINT,
    VK_FORMAT_R32G32_SINT,
    VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_R32G32B32_UINT,
    VK_FORMAT_R32G32B32_SINT,
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_FORMAT_R32G32B32A32_UINT,
    VK_FORMAT_R32G32B32A32_SINT,
    VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R64_UINT,
    VK_FORMAT_R64_SINT,
    VK_FORMAT_R64_SFLOAT,
    VK_FORMAT_R64G64_UINT,
    VK_FORMAT_R64G64_SINT,
    VK_FORMAT_R64G64_SFLOAT,
    VK_FORMAT_R64G64B64_UINT,
    VK_FORMAT_R64G64B64_SINT,
    VK_FORMAT_R64G64B64_SFLOAT,
    VK_FORMAT_R64G64B64A64_UINT,
    VK_FORMAT_R64G64B64A64_SINT,
    VK_FORMAT_R64G64B64A64_SFLOAT,
    VK_FORMAT_B10G11R11_UFLOAT_PACK32,
    VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
    VK_FORMAT_D16_UNORM,
    VK_FORMAT_X8_D24_UNORM_PACK32,
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_BC1_RGB_UNORM_BLOCK,
    VK_FORMAT_BC1_RGB_SRGB_BLOCK,
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
    VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
    VK_FORMAT_BC2_UNORM_BLOCK,
    VK_FORMAT_BC2_SRGB_BLOCK,
    VK_FORMAT_BC3_UNORM_BLOCK,
    VK_FORMAT_BC3_SRGB_BLOCK,
    VK_FORMAT_BC4_UNORM_BLOCK,
    VK_FORMAT_BC4_SNORM_BLOCK,
    VK_FORMAT_BC5_UNORM_BLOCK,
    VK_FORMAT_BC5_SNORM_BLOCK,
    VK_FORMAT_BC6H_UFLOAT_BLOCK,
    VK_FORMAT_BC6H_SFLOAT_BLOCK,
    VK_FORMAT_BC7_UNORM_BLOCK,
    VK_FORMAT_BC7_SRGB_BLOCK,
    VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
    VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
    VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
    VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
    VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
    VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
    VK_FORMAT_EAC_R11_UNORM_BLOCK,
    VK_FORMAT_EAC_R11_SNORM_BLOCK,
    VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
    VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
    VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
    VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
    VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
    VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
    VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
    VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
    VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
    VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
    VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
    VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
    VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
    VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
    VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
    VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
    VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
    VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
    VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
    VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
    VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
    VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
    VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
    VK_FORMAT_ASTC_12x12_SRGB_BLOCK,    //184

    VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, //1000054000
    VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
    VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
    VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
    VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
    VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
    VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
    VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,

    VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK, //1000066000
    VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK,
    VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK,

    VK_FORMAT_G8B8G8R8_422_UNORM,   //1000156000
    VK_FORMAT_B8G8R8G8_422_UNORM,
    VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
    VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
    VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
    VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
    VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
    VK_FORMAT_R10X6_UNORM_PACK16,
    VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
    VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
    VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
    VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
    VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
    VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
    VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
    VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
    VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
    VK_FORMAT_R12X4_UNORM_PACK16,
    VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
    VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
    VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
    VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
    VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
    VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
    VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
    VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
    VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
    VK_FORMAT_G16B16G16R16_422_UNORM,
    VK_FORMAT_B16G16R16G16_422_UNORM,
    VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
    VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
    VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
    VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
    VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
    VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
    VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16,
    VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16,
    VK_FORMAT_G16_B16R16_2PLANE_444_UNORM,
    VK_FORMAT_A4R4G4B4_UNORM_PACK16,
    VK_FORMAT_A4B4G4R4_UNORM_PACK16,
};
#include <algorithm>
// Ensure the sTextureFormatToVk array is in strict numerical order (so we can quickly std::find
consteval bool CheckFormatOrdering() {
    return std::is_sorted(std::begin(sTextureFormatToVk), std::end(sTextureFormatToVk));
}
static_assert(CheckFormatOrdering());


// do some rough checks to ensure our formats are in the order we expect (or else the above lookup doesnt work!)
static_assert(int(TextureFormat::B8G8R8A8_SRGB) == 50);
static_assert(sTextureFormatToVk[int(TextureFormat::B8G8R8A8_SRGB)] == int(VK_FORMAT_B8G8R8A8_SRGB));
static_assert(int(TextureFormat::R32_SFLOAT) == 100);
static_assert(int(TextureFormat::ETC2_R8G8B8A1_SRGB_BLOCK) == 150);
static_assert(sTextureFormatToVk[int(TextureFormat::ETC2_R8G8B8A1_SRGB_BLOCK)] == int(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK));
static_assert(int(TextureFormat::ASTC_12x12_SRGB_BLOCK) == 184);
static_assert(int(TextureFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG) == 185);
static_assert(sTextureFormatToVk[int(TextureFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG)] == int(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG));
static_assert(int(TextureFormat::ASTC_4x4_SFLOAT_BLOCK) == 193);
static_assert(sTextureFormatToVk[int(TextureFormat::ASTC_4x4_SFLOAT_BLOCK)] == int(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK));
static_assert(int(TextureFormat::G8B8G8R8_422_UNORM) == 207);
static_assert(sTextureFormatToVk[int(TextureFormat::G8B8G8R8_422_UNORM)] == int(VK_FORMAT_G8B8G8R8_422_UNORM));
static_assert(int(TextureFormat::EndCount) == 247);

//-----------------------------------------------------------------------------
VkFormat TextureFormatToVk(TextureFormat f)
//-----------------------------------------------------------------------------
{
    return sTextureFormatToVk[uint32_t(f)];
}

//-----------------------------------------------------------------------------
TextureFormat VkToTextureFormat(VkFormat f)
//-----------------------------------------------------------------------------
{
    // Use the fact we have ensured the TextureFormats are sorted with respect to the values of VkFormat, so we can binary search.
    auto foundIt = std::lower_bound(std::begin(sTextureFormatToVk), std::end(sTextureFormatToVk), f);
    if (foundIt != std::end(sTextureFormatToVk) && *foundIt == f)
        return TextureFormat( std::distance( std::begin(sTextureFormatToVk), foundIt ) );
    return TextureFormat::UNDEFINED;
}


//-----------------------------------------------------------------------------
template<>
TextureT<Vulkan> CreateTextureObject<Vulkan>(Vulkan& vulkan, const CreateTexObjectInfo& texInfo)
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    if (texInfo.pName == nullptr)
        LOGI("CreateTextureObject (%dx%d): <No Name>", texInfo.uiWidth, texInfo.uiHeight);
    else
        LOGI("CreateTextureObject (%dx%d): %s", texInfo.uiWidth, texInfo.uiHeight, texInfo.pName);

    VkImageView RetImageView = VK_NULL_HANDLE;
    VkImageLayout RetImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // How this texture object will be used.
    MemoryUsage MemoryUsage = MemoryUsage::GpuExclusive;

    VkFormat vkFormat = TextureFormatToVk(texInfo.Format);

    // Create the image...
    VkImageCreateInfo ImageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ImageInfo.flags = 0;
    assert(texInfo.uiDepth != 0);
    ImageInfo.imageType = (texInfo.uiDepth == 1) ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
    ImageInfo.format = vkFormat;
    ImageInfo.extent.width = texInfo.uiWidth;
    ImageInfo.extent.height = texInfo.uiHeight;
    ImageInfo.extent.depth = texInfo.uiDepth;
    ImageInfo.mipLevels = texInfo.uiMips;
    ImageInfo.arrayLayers = texInfo.uiFaces;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.queueFamilyIndexCount = 0;
    ImageInfo.pQueueFamilyIndices = NULL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    switch (texInfo.TexType)
    {
    case TT_SHADING_RATE_IMAGE:
        ImageInfo.usage = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case TT_CPU_UPDATE:
        ImageInfo.tiling = VK_IMAGE_TILING_LINEAR;
        ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case TT_NORMAL:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;// VK_IMAGE_TILING_LINEAR;      // VK_IMAGE_TILING_OPTIMAL
        ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        break;
    case TT_RENDER_TARGET:
        // If using VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT then tiling MUST be VK_IMAGE_TILING_OPTIMAL
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // DO NOT enable Storage_Bit (potential performance impact)
        ImageInfo.samples = (VkSampleCountFlagBits)texInfo.Msaa;
        break;
    case TT_RENDER_TARGET_WITH_STORAGE:
        // If using VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT then tiling MUST be VK_IMAGE_TILING_OPTIMAL
        // Also enabling Storage may disable some hardware render buffer optimizations
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        assert(texInfo.Msaa == 1 && texInfo.Format != TextureFormat::R8G8B8A8_SRGB);  //TODO: use Vulkan to determine if this texture buffer can have storeage set, for now assert on formats known to be problematic (msaa or srgb)
        ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        ImageInfo.samples = (VkSampleCountFlagBits)texInfo.Msaa;
        break;
    case TT_RENDER_TARGET_TRANSFERSRC:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ImageInfo.samples = (VkSampleCountFlagBits)texInfo.Msaa;
        break;
    case TT_RENDER_TARGET_SUBPASS:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        ImageInfo.samples = (VkSampleCountFlagBits)texInfo.Msaa;
        // Subpass render targets dont need to be backed by memory!
        MemoryUsage = MemoryUsage::GpuLazilyAllocated;
        break;
    case TT_COMPUTE_TARGET:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;// | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case TT_COMPUTE_STORAGE:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
        break;
    case TT_DEPTH_TARGET:
        // If using VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT then tiling MUST be VK_IMAGE_TILING_OPTIMAL
        ImageInfo.mipLevels = 1;
        ImageInfo.arrayLayers = 1;
        ImageInfo.samples = (VkSampleCountFlagBits)texInfo.Msaa;
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT /*| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT*/;
        if (texInfo.Msaa != VK_SAMPLE_COUNT_1_BIT)
            ImageInfo.flags |= VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;
        break;

    default:
        assert(0);
        break;
    }

    if ((texInfo.Flags & TEXTURE_FLAGS::MutableFormat) != 0)
        ImageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    if ((texInfo.Flags & TEXTURE_FLAGS::ForceLinearTiling) != 0)
        ImageInfo.tiling = VK_IMAGE_TILING_LINEAR;

    // Need the return image
    Wrap_VkImage RetImage;
    bool ImageInitialized = RetImage.Initialize(&vulkan, ImageInfo, MemoryUsage, texInfo.pName);
    if (!ImageInitialized && MemoryUsage == MemoryUsage::GpuLazilyAllocated)
    {
        LOGI("Unable to initialize GpuLazilyAllocated image (probably not supported by GPU hardware).  Falling back to GpuExclusive");
        MemoryUsage = MemoryUsage::GpuExclusive;
        ImageInfo.usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        ImageInitialized = RetImage.Initialize(&vulkan, ImageInfo, MemoryUsage, texInfo.pName);
    }
    if (!ImageInitialized)
    {
        LOGE("Unable to initialize image (Not from file)");
        return {};
    }

    VkCommandBuffer SetupCmdBuffer = vulkan.StartSetupCommandBuffer();
    switch (texInfo.TexType)
    {
    case TT_SHADING_RATE_IMAGE:
        vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, ImageInfo.mipLevels, 0, ImageInfo.arrayLayers);
        RetImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        break;
    case TT_CPU_UPDATE:
        vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, ImageInfo.mipLevels, 0, ImageInfo.arrayLayers);
        RetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case TT_NORMAL:
        vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, ImageInfo.mipLevels, 0, ImageInfo.arrayLayers);
        RetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case TT_RENDER_TARGET:
    case TT_RENDER_TARGET_WITH_STORAGE:
        vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
        RetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case TT_RENDER_TARGET_TRANSFERSRC:
        vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
        RetImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        break;
    case TT_RENDER_TARGET_SUBPASS:
        RetImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        break;
    case TT_COMPUTE_TARGET:
    case TT_COMPUTE_STORAGE:
        vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_GENERAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, ImageInfo.mipLevels, 0, ImageInfo.arrayLayers);
        RetImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        break;
    case TT_DEPTH_TARGET:
        if (FormatHasStencil( texInfo.Format))
        {
            // Can have depth and stencil flag
            vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
        }
        else if (FormatHasDepth(texInfo.Format))
        {
            // Only has the depth flag set
            vulkan.SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
        }
        else
        {
            LOGE("Unhandled depth format!!!");
        }
        RetImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        break;
    default:
        assert(0);
        break;
    }

    vulkan.FinishSetupCommandBuffer(SetupCmdBuffer);

    // ... and an ImageView
    VkImageViewCreateInfo ImageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ImageViewInfo.flags = 0;
    ImageViewInfo.image = RetImage.m_VmaImage.GetVkBuffer();
    ImageViewInfo.viewType = (texInfo.uiDepth == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D; // <== No support for VK_IMAGE_VIEW_TYPE_CUBE
    ImageViewInfo.format = ImageInfo.format;
    ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_R;
    ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_G;
    ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_B;
    ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_A;
    ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageViewInfo.subresourceRange.baseMipLevel = 0;
    ImageViewInfo.subresourceRange.levelCount = ImageInfo.mipLevels;
    ImageViewInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewInfo.subresourceRange.layerCount = 1;

    SamplerAddressMode SamplerMode = texInfo.SamplerMode;

    SamplerBorderColor BorderColor = SamplerBorderColor::TransparentBlackFloat;

    switch (texInfo.TexType)
    {
    case TT_SHADING_RATE_IMAGE:
    case TT_CPU_UPDATE:
    case TT_NORMAL:
    case TT_RENDER_TARGET:
    case TT_RENDER_TARGET_WITH_STORAGE:
    case TT_RENDER_TARGET_TRANSFERSRC:
    case TT_RENDER_TARGET_SUBPASS:
    case TT_COMPUTE_TARGET:
    case TT_COMPUTE_STORAGE:
        ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
    case TT_DEPTH_TARGET:
        ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        SamplerMode = (SamplerMode == SamplerAddressMode::Undefined) ? SamplerAddressMode::ClampBorder : SamplerMode;    // default to ClampBorder
        BorderColor = SamplerBorderColor::OpaqueWhiteFloat;
        break;
    default:
        assert(0);
        break;
    }
    SamplerMode = (SamplerMode == SamplerAddressMode::Undefined) ? SamplerAddressMode::ClampEdge : SamplerMode;         // default to ClampEdge

    RetVal = vkCreateImageView(vulkan.m_VulkanDevice, &ImageViewInfo, NULL, &RetImageView);
    if (!CheckVkError("vkCreateImageView()", RetVal))
    {
        return {};
    }

    // LOGI("vkCreateImageView: %s -> %p", pName, RetImageView);

    // Need a sampler...
    SamplerFilter FilterMode = texInfo.FilterMode;
    if (FilterMode == SamplerFilter::Undefined)
    {
        const auto& FormatProperties = vulkan.GetFormatProperties(vkFormat);
        auto TilingFeatures = (ImageInfo.tiling == VK_IMAGE_TILING_LINEAR) ? FormatProperties.linearTilingFeatures : FormatProperties.optimalTilingFeatures;
        if ((TilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0)
            FilterMode = SamplerFilter::Linear;
        else
            FilterMode = SamplerFilter::Nearest;
    }
    SamplerT<Vulkan> Sampler = CreateSampler(vulkan, SamplerMode, FilterMode, BorderColor, 0.0f);
    if (Sampler.IsEmpty())
    {
        return {};
    }

    return{ texInfo.uiWidth, texInfo.uiHeight, texInfo.uiDepth, texInfo.uiMips, texInfo.Format, RetImageLayout, std::move(RetImage.m_VmaImage), Sampler, RetImageView };
}


//-----------------------------------------------------------------------------
TextureT<Vulkan> CreateTextureObjectView( Vulkan& vulkan, const TextureT<Vulkan>& original, TextureFormat viewFormat )
//-----------------------------------------------------------------------------
{
    VkImageView imageView = VK_NULL_HANDLE;

    VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    imageViewCreateInfo.image = original.GetVkImage();
    imageViewCreateInfo.viewType = ( original.Depth == 1 ) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D; // <== No support for VK_IMAGE_VIEW_TYPE_CUBE
    imageViewCreateInfo.format = TextureFormatToVk(viewFormat);
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = original.MipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    auto RetVal = vkCreateImageView( vulkan.m_VulkanDevice, &imageViewCreateInfo, NULL, &imageView );
    if (!CheckVkError( "vkCreateImageView()", RetVal ))
    {
        return {};
    }

    SamplerVulkan sampler = CreateSampler( vulkan, SamplerAddressMode::ClampEdge, SamplerFilter::Linear, SamplerBorderColor::TransparentBlackFloat, 0.0f );
    if (sampler.IsEmpty())
    {
        return {};
    }

    return { original.Width, original.Height, original.Depth, original.MipLevels, original.Format, original.GetVkImageLayout(), original.GetVkImage(), VK_NULL_HANDLE, sampler, imageView };
}

//-----------------------------------------------------------------------------
template<>
TextureT<Vulkan> CreateTextureFromBuffer<Vulkan>( Vulkan& vulkan, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName )
//-----------------------------------------------------------------------------
{
    if (pName == nullptr)
        LOGI( "CreateTextureFromBuffer (%dx%d): <No Name>", Width, Height );
    else
        LOGI( "CreateTextureFromBuffer (%dx%d): %s", Width, Height, pName );

    uint32_t Faces = 1;
    uint32_t MipLevels = 1;
    VkFormat vkFormat = TextureFormatToVk( Format );
    VkImageUsageFlags FinalUsage = ( VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
    VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Image creation info.  Will change below based on need
    VkImageCreateInfo ImageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ImageInfo.flags = 0;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = vkFormat;
    ImageInfo.extent.width = Width;
    ImageInfo.extent.height = Height;
    ImageInfo.extent.depth = Depth;
    ImageInfo.mipLevels = 1;	// Set to 1 since making each level.  Later will be set to MipLevels;
    ImageInfo.arrayLayers = 1;  // Set to 6 for cube maps
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.queueFamilyIndexCount = 0;
    ImageInfo.pQueueFamilyIndices = NULL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;   // VK_IMAGE_LAYOUT_UNDEFINED says data not guaranteed to be preserved when changing state

    struct CubeFace
    {
        CubeFace( uint32_t depthSlices, uint32_t mips ) : mipsPerDepthSlice( mips ), images( depthSlices* mips )
        {}
        const auto& GetImage( uint32_t depth, uint32_t mip ) const { return images[depth * mipsPerDepthSlice]; }
        auto& GetImage( uint32_t depth, uint32_t mip ) { return images[depth * mipsPerDepthSlice + mip]; }
        const uint32_t mipsPerDepthSlice;
        std::vector<Wrap_VkImage> images;
    };

    // Allocate the depth slices (and contained mips) for each face
    std::vector<CubeFace> cubeFaces;
    cubeFaces.reserve( Faces );
    for (uint32_t WhichFace = 0; WhichFace < Faces; ++WhichFace)
    {
        cubeFaces.emplace_back( Depth, MipLevels );
    }

    // Create and copy mip levels

    // Need the setup command buffer for loading images
    VkCommandBuffer SetupCmdBuffer = vulkan.StartSetupCommandBuffer();

    uint32_t formatBytesPerPixel = 4;
    if (vkFormat == VK_FORMAT_R32G32B32A32_SFLOAT)
        formatBytesPerPixel = 16;
    else if (vkFormat >= VK_FORMAT_R8_UNORM && vkFormat <= VK_FORMAT_R8_SRGB)
        formatBytesPerPixel = 1;
    else if (vkFormat >= VK_FORMAT_R8G8_UNORM && vkFormat <= VK_FORMAT_R8G8_SRGB)
        formatBytesPerPixel = 2;
    else if (vkFormat >= VK_FORMAT_R16_UNORM && vkFormat <= VK_FORMAT_R16_SFLOAT)
        formatBytesPerPixel = 2;

    const uint8_t* pData8 = static_cast<const uint8_t*>( pData );

    // Load separate cube map faces into linear tiled textures (for copying only).
    // Split into faces and 2d depth slices because linear textures have extremely limited format/size restrictions.
    for (uint32_t WhichFace = 0; WhichFace < Faces; WhichFace++)
    {
        for (uint32_t WhichDepth = 0; WhichDepth < Depth; WhichDepth++)
        {
            for (uint32_t WhichMip = 0; WhichMip < MipLevels; WhichMip++)
            {
                ImageInfo.extent.width = Width;
                ImageInfo.extent.height = Height;
                ImageInfo.extent.depth = 1;

                Wrap_VkImage& faceImage = cubeFaces[WhichFace].GetImage( WhichDepth, WhichMip );
                if (!faceImage.Initialize( &vulkan, ImageInfo, MemoryUsage::CpuToGpu ))
                {
                    LOGE( "CreateTextureFromBuffer: Unable to initialize mip %d of face %d", WhichMip + 1, WhichFace + 1 );
                    return {};
                }

                // ... copy texture data into the image
                VkImageSubresource SubresInfo = {};
                SubresInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                SubresInfo.mipLevel = 0;
                SubresInfo.arrayLayer = 0;

                VkSubresourceLayout SubResLayout = {};
                auto& faceImageMem = faceImage.m_VmaImage;
                vkGetImageSubresourceLayout( vulkan.m_VulkanDevice, faceImageMem.GetVkBuffer(), &SubresInfo, &SubResLayout );

                {
                    auto& memorymanager = vulkan.GetMemoryManager();
                    auto mappedMemory = memorymanager.Map<uint8_t>( faceImageMem );
                    if (SubResLayout.rowPitch == Width * formatBytesPerPixel)
                    {
                        memcpy( mappedMemory.data(), pData8, SubResLayout.size );
                        pData8 += Width * Height * formatBytesPerPixel;
                    }
                    else
                    {
                        uint8_t* pDest = mappedMemory.data();
                        for (uint32_t h = 0; h < Height; ++h)
                        {
                            memcpy( pDest, pData8, Width * formatBytesPerPixel );
                            pDest += SubResLayout.rowPitch;
                            pData8 += Width * formatBytesPerPixel;
                        }
                    }
                    memorymanager.Unmap( faceImageMem, std::move( mappedMemory ) );
                }

                // Image barrier for linear image (base)
                // Linear image will be used as a source for the copy
                VkPipelineStageFlags srcMask = 0;
                VkPipelineStageFlags dstMask = 1;
                uint32_t baseMipLevel = 0;
                uint32_t mipLevelCount = 1;

                vulkan.SetImageLayout( faceImageMem.GetVkBuffer(),
                    SetupCmdBuffer,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_PREINITIALIZED,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    srcMask,
                    dstMask,
                    baseMipLevel,
                    mipLevelCount );
            }   // Each mipmap in each depth slice
        }   // Each depth slice in each face
    }   // Each Face

    // Transfer cube map faces to optimal tiling

    // Now that creating the whole thing we need the correct size and type
    ImageInfo.extent.width = Width;
    ImageInfo.extent.height = Height;
    ImageInfo.extent.depth = Depth;
    ImageInfo.imageType = Depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;

    // Now that we are done creating single images, need to create all mip levels
    ImageInfo.mipLevels = MipLevels;

    // Setup texture as blit target with optimal tiling
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = FinalUsage;

    if (Faces == 6)
        ImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    ImageInfo.arrayLayers = Faces;

    // Need the return image
    Wrap_VkImage RetImage;
    if (!RetImage.Initialize( &vulkan, ImageInfo, MemoryUsage::GpuExclusive ))
    {
        LOGE( "CreateTextureFromBuffer: Unable to initialize texture image" );
        return {};
    }

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    VkPipelineStageFlags srcMask = 0;
    VkPipelineStageFlags dstMask = 1;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = MipLevels;
    uint32_t baseLayer = 0;
    uint32_t layerCount = Faces;

    vulkan.SetImageLayout( RetImage.m_VmaImage.GetVkBuffer(),
        SetupCmdBuffer,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_PREINITIALIZED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount );

    // Copy cube map faces one by one
    // Vulkan spec says the order is +X, -X, +Y, -Y, +Z, -Z
    for (uint32_t WhichFace = 0; WhichFace < Faces; WhichFace++)
    {
        // Copy depth slices one at a time
        for (uint32_t WhichDepth = 0; WhichDepth < Depth; WhichDepth++)
        {
            for (uint32_t WhichMip = 0; WhichMip < MipLevels; WhichMip++)
            {
                // Copy region for image blit
                VkImageCopy copyRegion = {};

                // Source is always base level because we have an image for each face and each mip
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset.x = 0;
                copyRegion.srcOffset.y = 0;
                copyRegion.srcOffset.z = 0;

                // Source is the section of the main image
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = WhichFace;
                copyRegion.dstSubresource.mipLevel = WhichMip;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset.x = 0;
                copyRegion.dstOffset.y = 0;
                copyRegion.dstOffset.z = WhichDepth;

                // Size is the size of this mip
                copyRegion.extent.width = Width;
                copyRegion.extent.height = Height;
                copyRegion.extent.depth = 1;

                // Put image copy into command buffer
                vkCmdCopyImage(
                    SetupCmdBuffer,
                    cubeFaces[WhichFace].GetImage( WhichDepth, WhichMip ).m_VmaImage.GetVkBuffer(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    RetImage.m_VmaImage.GetVkBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copyRegion );
            }   // Each mipmap in each depth slice
        }   // Each depth slice in each face
    }   // Each Face

    // Change texture image layout to the 'final' settings now we are done transferring
    srcMask = 0;
    dstMask = 1;
    baseMipLevel = 0;
    mipLevelCount = MipLevels;
    baseLayer = 0;
    layerCount = Faces;

    vulkan.SetImageLayout( RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FinalLayout,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount );

    // Submit the command buffer we have been working on
    vulkan.FinishSetupCommandBuffer( SetupCmdBuffer );

    // Need a sampler...
    SamplerVulkan Sampler = CreateSampler( vulkan, SamplerMode, Filter, SamplerBorderColor::TransparentBlackFloat, 0.0f );
    if (Sampler.IsEmpty())
    {
        return {};
    }

    VkImageViewType viewType;
    if (Depth > 1)
        viewType = VK_IMAGE_VIEW_TYPE_3D;
    else if (Faces == 6)
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
        viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageView RetImageView;
    if (!CreateImageView(vulkan, RetImage.m_VmaImage.GetVkBuffer(), vkFormat, baseMipLevel, MipLevels, Faces, viewType, &RetImageView))
    {
        ReleaseSampler(vulkan, &Sampler);
        return {};
    }

    // Cleanup
    cubeFaces.clear();

    // Set the return values
    TextureVulkan RetTex{ Width, Height, Depth, MipLevels, Format, FinalLayout, std::move( RetImage.m_VmaImage ), Sampler, RetImageView };
    return RetTex;
}


//-----------------------------------------------------------------------------
/// Create a vulkan image view object
bool CreateImageView(Vulkan& vulkan, VkImage image, VkFormat format, uint32_t baseMipLevel, uint32_t numMipLevels, uint32_t numFaces, VkImageViewType viewType, VkImageView* pRetImageView)
//-----------------------------------------------------------------------------
{
    assert(pRetImageView);
    VkImageViewCreateInfo ImageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ImageViewInfo.flags = 0;
    ImageViewInfo.image = image;
    ImageViewInfo.viewType = viewType;

    ImageViewInfo.format = format;
    ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageViewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    ImageViewInfo.subresourceRange.levelCount = numMipLevels;
    ImageViewInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewInfo.subresourceRange.layerCount = 1;
    if (numFaces == 6)
        ImageViewInfo.subresourceRange.layerCount = 6;
    else
        ImageViewInfo.subresourceRange.layerCount = 1;

    auto RetVal = vkCreateImageView(vulkan.m_VulkanDevice, &ImageViewInfo, NULL, pRetImageView);
    if (!CheckVkError("vkCreateImageView()", RetVal))
    {
        return false;
    }
    return true;
}

// Implementation of template function specialization
template<>
void ReleaseTexture<Vulkan>( Vulkan& vulkan, TextureT<Vulkan>* pTexture )
{
    if (!pTexture)
        return;
    pTexture->Release( &vulkan );
    *pTexture = TextureT<Vulkan>{};    // destroy and clear
}


// Implementation of template function specialization
template<>
SamplerT<Vulkan> CreateSampler<Vulkan>(Vulkan& vulkan, const CreateSamplerObjectInfo& createInfo)
{
    const bool anisotropyEnable = createInfo.Filter == SamplerFilter::Linear;
    const VkSamplerCreateInfo SamplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .flags = 0,
        .magFilter = EnumToVk(createInfo.Filter),
        .minFilter = EnumToVk(createInfo.Filter),
        .mipmapMode = createInfo.Filter == SamplerFilter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = EnumToVk(createInfo.Mode),
        .addressModeV = EnumToVk(createInfo.Mode),
        .addressModeW = EnumToVk(createInfo.Mode),
        .mipLodBias = createInfo.MipBias,
        .anisotropyEnable = anisotropyEnable ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = anisotropyEnable == VK_TRUE ? 4.0f : 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = FLT_MAX,
        .borderColor = EnumToVk(createInfo.BorderColor),
        .unnormalizedCoordinates = createInfo.UnnormalizedCoordinates ? VK_TRUE : VK_FALSE };

    VkSampler vkSampler;
    VkResult RetVal = vkCreateSampler(vulkan.m_VulkanDevice, &SamplerInfo, NULL, &vkSampler);
    if (!CheckVkError("vkCreateSampler()", RetVal))
        return {};
    return { vkSampler };
}

// Implementation of template function specialization
template<>
void ReleaseSampler<Vulkan>( Vulkan& vulkan, SamplerT<Vulkan>* pSampler)
{
    if (!pSampler)
        return;
    vkDestroySampler(vulkan.m_VulkanDevice, pSampler->m_Sampler, nullptr);
    *pSampler = SamplerT<Vulkan>{};    // destroy and clear
}
