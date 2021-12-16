// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
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
	VulkanTexInfo( uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageLayout imageLayout, MemoryVmaAllocatedBuffer<VkImage> vmaImage, VkSampler sampler, VkImageView imageView )
		: Width(width)
		, Height(height)
		, MipLevels(mipLevels)
		, Format(format)
		, ImageLayout(imageLayout)
		, VmaImage(std::move(vmaImage))
		, Sampler(sampler)
		, ImageView(imageView)
	{
	}

	/// @brief Construct VulkanTexInfo from a pre-existing Vulkan image/memory handles.
	/// @param sampler - ownership passed to this VulkanTexInfo.
	/// @param imageView - ownership passed to this VulkanTexInfo.
	VulkanTexInfo( uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageLayout imageLayout, VkImage image, VkDeviceMemory memory, VkSampler sampler, VkImageView imageView )
		: Width(width)
		, Height(height)
		, MipLevels(mipLevels)
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
	VkImageView			  GetVkImageView() const			 { return ImageView;  }
	bool				  IsEmpty() const					 { return !VmaImage; }

	uint32_t  Width = 0;
	uint32_t  Height = 0;
	uint32_t  MipLevels = 0;
	VkFormat  Format = VK_FORMAT_UNDEFINED;

protected:

	MemoryVmaAllocatedBuffer<VkImage> VmaImage;

	VkSampler       Sampler = VK_NULL_HANDLE;
	VkImageView     ImageView = VK_NULL_HANDLE;
	VkImageLayout   ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Needed for functions handling own memory (i.e. AndroidHardwareBuffers)
    VkImage         Image = VK_NULL_HANDLE;
    VkDeviceMemory  Memory = VK_NULL_HANDLE;
};

enum TEXTURE_TYPE
{
	TT_NORMAL = 0,
	TT_RENDER_TARGET,
	TT_RENDER_TARGET_SUBPASS,
	TT_DEPTH_TARGET,
	TT_COMPUTE_TARGET,
	NUM_TEXTURE_TYPES
};

// Actual support functions
VulkanTexInfo   LoadKTXTexture(Vulkan *pVulkan, AssetManager&, const char* pFileName, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
VulkanTexInfo   LoadTextureFromBuffer(Vulkan* pVulkan, void *pData, uint32_t DataSize, uint32_t Width, uint32_t Height, VkFormat NewFormat, VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkFilter Filter = VK_FILTER_LINEAR, VkImageUsageFlags FinalUsage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
VulkanTexInfo	CreateTextureObject(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, VkFormat NewFormat, TEXTURE_TYPE TexType, const char* pName, VkSampleCountFlagBits Msaa = VK_SAMPLE_COUNT_1_BIT);
void            ReleaseTexture(Vulkan* pVulkan, VulkanTexInfo *pTexInfo);

