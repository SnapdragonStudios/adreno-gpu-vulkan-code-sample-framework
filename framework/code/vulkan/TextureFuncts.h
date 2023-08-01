//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// TextureFuncts.h
//      Vulkan texture handling support

//#include "vulkan/vulkan.hpp"
#include "texture/texture.hpp"

#if 0
// Forward declarations
class AssetManager;
class Vulkan;
template<typename T_GFXAPI> class TextureT;
using TextureVulkan = TextureT<Vulkan>;

/// @brief A Vulkan texture
/// Owns memory and sampler etc associated with a single texture.
class TextureVulkan
{
public:
	TextureVulkan() : VmaImage() {}
	TextureVulkan(const TextureVulkan&) = delete;
	TextureVulkan& operator=(const TextureVulkan&) = delete;
	TextureVulkan(TextureVulkan&&) noexcept;
	TextureVulkan& operator=(TextureVulkan&&) noexcept;
	~TextureVulkan();

	/// @brief Construct TextureVulkan from a pre-existing vmaImage.
	/// @param vmaImage - ownership passed to this TextureVulkan.
	/// @param sampler - ownership passed to this TextureVulkan.
	/// @param imageView - ownership passed to this TextureVulkan.
	TextureVulkan( uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageLayout imageLayout, MemoryAllocatedBuffer<Vulkan, VkImage> vmaImage, VkSampler sampler, VkImageView imageView )
		: Width(width)
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

	/// @brief Construct TextureVulkan from a pre-existing Vulkan image/memory handles.
	/// @param image - ownership NOT passed in to this TextureVulkan, beware of lifetime issues.
	/// @param sampler - ownership passed to this TextureVulkan.
	/// @param imageView - ownership passed to this TextureVulkan.
	TextureVulkan( uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t firstMip, VkFormat format, VkImageLayout imageLayout, VkImage image, VkDeviceMemory memory, VkSampler sampler, VkImageView imageView )
		: Width(width)
		, Height(height)
		, Depth(1)
		, MipLevels( mipLevels )
		, FirstMip( firstMip )
		, Format(format)
		, ImageLayout(imageLayout)
		, Image(image)
		, Memory(memory)
		, Sampler(sampler)
		, ImageView(imageView)
	{
	}

	void Release(Vulkan* pVulkan);

	VkImage				  GetVkImage() const                 { return VmaImage ? VmaImage.GetVkBuffer() : Image; }
	VkDescriptorImageInfo GetVkDescriptorImageInfo() const   { return { Sampler, ImageView, ImageLayout }; }
	VkImageLayout		  GetVkImageLayout() const			 { return ImageLayout; }
	VkSampler			  GetVkSampler() const				 { return Sampler; }
	VkImageView			  GetVkImageView() const			 { return ImageView;  }
	bool				  IsEmpty() const					 { return !VmaImage; }

	uint32_t  Width = 0;
	uint32_t  Height = 0;
	uint32_t  Depth = 0;
	uint32_t  MipLevels = 0;
	uint32_t  FirstMip = 0;
	VkFormat  Format = VK_FORMAT_UNDEFINED;
    VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

public:

	MemoryAllocatedBuffer<Vulkan, VkImage> VmaImage;

	VkSampler       Sampler = VK_NULL_HANDLE;
	VkImageView     ImageView = VK_NULL_HANDLE;

    // Needed for functions handling own memory (i.e. AndroidHardwareBuffers)
    VkImage         Image = VK_NULL_HANDLE;
    VkDeviceMemory  Memory = VK_NULL_HANDLE;
};
#endif // 0

// Actual support functions

#if 0

/// Load/create texture from .ktx file (Mips to load are lowest resolution, NOT 0,1,2...)
TextureVulkan   LoadKTXTexture(Vulkan *pVulkan, AssetManager&, const char* pFileName, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, int32_t NumMipsToLoad = 0x7fffffff, float mipBias = 0.0f);
/// Parse a KTX texture and dump each mip to individual file (restrictions on faces, formats, output, etc.)
void DumpKTXMipFiles(AssetManager& assetManager, std::string SourceFile, std::string OutBaseFile);
/// Load/create texture from .ppm file
TextureVulkan   LoadPPMTexture(Vulkan* pVulkan, AssetManager&, const char* pFileName, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
/// Create texture from an existing memory buffer
TextureVulkan   CreateTextureFromBuffer(Vulkan* pVulkan, const void *pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SamplerFilter Filter = SamplerFilter::Linear, VkImageUsageFlags FinalUsage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#endif // 0

/// Save texture data to 'filename'
/// @returns true on success
bool SaveTextureData(const char* pFileName, TextureFormat format, int width, int height, const void* data);

#if 0

/// Create texture (generally for render target usage)
TextureVulkan	CreateTextureObject(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, VkFormat Format, TEXTURE_TYPE TexType, const char* pName, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, TEXTURE_FLAGS Flags = TEXTURE_FLAGS::None);
/// Create texture (generally for render target usage).  Uses CreateTexObjectInfo structure to define texture creation parameters.
TextureVulkan	CreateTextureObject(Vulkan* pVulkan, const CreateTexObjectInfo& texInfo);
/// Create texture that is an imageview referencing an existing TextureVulkan.
/// Required that the referenced originalTexInfo does not go out of scope (be destroyed) before the referencing texture. 
TextureVulkan	CreateTextureObjectView( Vulkan* pVulkan, const TextureVulkan& original, VkFormat viewFormat );
///// Release memory associated with the given texture and reset to 'empty' state
//void            ReleaseTexture(Vulkan* pVulkan, TextureVulkan *pTexInfo);
/// Create a vulkan image view object
bool            CreateImageView(Vulkan* pVulkan, VkImage image, VkFormat format, uint32_t baseMipLevel, uint32_t numMipLevels, uint32_t numFaces, VkImageViewType viewType, VkImageView* pRetImageView);
/// Create a vulkan texture sampler
bool            CreateSampler(Vulkan* pVulkan, VkSamplerAddressMode SamplerMode, VkFilter FilterMode, VkBorderColor BorderColor, bool UnnormalizedCoordinates, float mipBias, VkSampler* pRetSampler);

#endif
