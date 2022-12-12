//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// TextureFuncts.h
//      Vulkan texture handling support

#include "vulkan/vulkan.hpp"

class AssetManager;

/// @brief A Vulkan texture
/// Owns memory and sampler etc associated with a single texture.
class VulkanTexInfo
{
public:
	VulkanTexInfo() : VmaImage() {}
	VulkanTexInfo(const VulkanTexInfo&) = delete;
	VulkanTexInfo& operator=(const VulkanTexInfo&) = delete;
	VulkanTexInfo(VulkanTexInfo&&) noexcept;
	VulkanTexInfo& operator=(VulkanTexInfo&&) noexcept;
	~VulkanTexInfo();

	/// @brief Construct VulkanTexInfo from a pre-existing vmaImage.
	/// @param vmaImage - ownership passed to this VulkanTexInfo.
	/// @param sampler - ownership passed to this VulkanTexInfo.
	/// @param imageView - ownership passed to this VulkanTexInfo.
	VulkanTexInfo( uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageLayout imageLayout, MemoryVmaAllocatedBuffer<VkImage> vmaImage, VkSampler sampler, VkImageView imageView )
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

	/// @brief Construct VulkanTexInfo from a pre-existing Vulkan image/memory handles.
	/// @param image - ownership NOT passed in to this VulkanTexInfo, beware of lifetime issues.
	/// @param sampler - ownership passed to this VulkanTexInfo.
	/// @param imageView - ownership passed to this VulkanTexInfo.
	VulkanTexInfo( uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t firstMip, VkFormat format, VkImageLayout imageLayout, VkImage image, VkDeviceMemory memory, VkSampler sampler, VkImageView imageView )
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

protected:

	MemoryVmaAllocatedBuffer<VkImage> VmaImage;

	VkSampler       Sampler = VK_NULL_HANDLE;
	VkImageView     ImageView = VK_NULL_HANDLE;

    // Needed for functions handling own memory (i.e. AndroidHardwareBuffers)
    VkImage         Image = VK_NULL_HANDLE;
    VkDeviceMemory  Memory = VK_NULL_HANDLE;
};

/// Texture types (intended use) for CreateTextureObject
enum TEXTURE_TYPE
{
	TT_NORMAL = 0,
	TT_RENDER_TARGET,
	TT_RENDER_TARGET_WITH_STORAGE,
	TT_RENDER_TARGET_TRANSFERSRC,
	TT_RENDER_TARGET_SUBPASS,
	TT_DEPTH_TARGET,
	TT_COMPUTE_TARGET,
    TT_CPU_UPDATE,
	TT_SHADING_RATE_IMAGE,
	NUM_TEXTURE_TYPES
};

/// Texture flags (feature enable/disable) for CreateTextureObject
enum TEXTURE_FLAGS
{
    None = 0,
    MutableFormat = 1,
    ForceLinearTiling = 2
};

/// Parameters for CreateTextureObject
struct CreateTexObjectInfo
{
	uint32_t uiWidth = 0;
	uint32_t uiHeight = 0;
	uint32_t uiDepth = 1;	// default to 2d texture.
	uint32_t uiMips = 1;    // default to no mips
	uint32_t uiFaces = 1;	// default to 1 face (ie not a texture array or cubemap)
	VkFormat Format = VK_FORMAT_UNDEFINED;
    TEXTURE_TYPE TexType = TEXTURE_TYPE::TT_NORMAL;
    TEXTURE_FLAGS Flags = TEXTURE_FLAGS::None;
    const char* pName = nullptr;
	VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT;
	VkFilter FilterMode = VK_FILTER_LINEAR;
	VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;	//default to picking from a default dependant on the texture type
	bool UnNormalizedCoordinates = false;
};

// Actual support functions

/// Load/create texture from .ktx file (Mips to load are lowest resolution, NOT 0,1,2...)
VulkanTexInfo   LoadKTXTexture(Vulkan *pVulkan, AssetManager&, const char* pFileName, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, int32_t NumMipsToLoad = 0x7fffffff, float mipBias = 0.0f);
/// Parse a KTX texture and dump each mip to individual file (restrictions on faces, formats, output, etc.)
void DumpKTXMipFiles(AssetManager& assetManager, std::string SourceFile, std::string OutBaseFile);
/// Load/create texture from .ppm file
VulkanTexInfo   LoadPPMTexture(Vulkan* pVulkan, AssetManager&, const char* pFileName, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
/// Create texture from an existing memory buffer
VulkanTexInfo   LoadTextureFromBuffer(Vulkan* pVulkan, const void *pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkFilter Filter = VK_FILTER_LINEAR, VkImageUsageFlags FinalUsage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
/// Save texture data to 'filename'
/// @returns true on success
bool SaveTextureData(const char* pFileName, VkFormat format, int width, int height, const void* data);
/// Create texture (generally for render target usage)
VulkanTexInfo	CreateTextureObject(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, VkFormat Format, TEXTURE_TYPE TexType, const char* pName, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT, TEXTURE_FLAGS Flags = TEXTURE_FLAGS::None);
/// Create texture (generally for render target usage).  Uses CreateTexObjectInfo structure to define texture creation parameters.
VulkanTexInfo	CreateTextureObject(Vulkan* pVulkan, const CreateTexObjectInfo& texInfo);
/// Create texture that is an imageview referencing an existing VulkanTexInfo.
/// Required that the referenced originalTexInfo does not go out of scope (be destroyed) before the referencing texture. 
VulkanTexInfo	CreateTextureObjectView( Vulkan* pVulkan, const VulkanTexInfo& original, VkFormat viewFormat );
/// Release memory associated with the given texture and reset to 'empty' state
void            ReleaseTexture(Vulkan* pVulkan, VulkanTexInfo *pTexInfo);

