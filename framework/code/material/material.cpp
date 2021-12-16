// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "material.hpp"
#include "vulkan/vulkan.hpp"
#include <array>
#include "system/os_common.h"
#include "vulkan/TextureFuncts.h"


MaterialPass::MaterialPass(Vulkan& vulkan, const ShaderPass& shaderPass, VkDescriptorPool&& descriptorPool, std::vector<VkDescriptorSet>&& descriptorSets, tTextureBindings&& textureBindings, tImageBindings&& imageBindings, tBufferBindings&& bufferBindings)
	: mVulkan(vulkan)
	, mShaderPass(shaderPass)
	, mDescriptorPool(descriptorPool)
	, mDescriptorSets(std::move(descriptorSets))
	, mTextureBindings(std::move(textureBindings))
	, mImageBindings(std::move(imageBindings))
	, mBufferBindings(std::move(bufferBindings))
{
	descriptorPool = VK_NULL_HANDLE;	// we took owenership
}

MaterialPass::MaterialPass(MaterialPass&& other) noexcept
	: mVulkan(other.mVulkan)
	, mShaderPass(other.mShaderPass)
	, mDescriptorPool(other.mDescriptorPool)
	, mDescriptorSets(std::move(other.mDescriptorSets))
	, mTextureBindings(std::move(other.mTextureBindings))
	, mImageBindings(std::move(other.mImageBindings))
	, mBufferBindings(std::move(other.mBufferBindings))
{
	other.mDescriptorPool = VK_NULL_HANDLE;
}

MaterialPass::~MaterialPass()
{
	if (mDescriptorPool != VK_NULL_HANDLE)
	{
		//if (mDescriptorSet != VK_NULL_HANDLE)
		//	vkFreeDescriptorSets(mVulkan.m_VulkanDevice, mDescriptorPool, 1, &mDescriptorSet); only if VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
		vkDestroyDescriptorPool(mVulkan.m_VulkanDevice, mDescriptorPool, nullptr);
	}
}

MaterialPass::ImageInfo::ImageInfo(const VulkanTexInfo& t)
	: image(t.GetVkImage())
	, imageView(t.GetVkImageView())
	, imageLayout(t.GetVkImageLayout())
{
}

bool MaterialPass::UpdateDescriptorSets(uint32_t bufferIdx)
{
	static const int cMAX_WRITES = 32;
	static const int cMAX_IMAGE_INFOS = 32;
	static const int cMAX_BUFFER_INFOS = 32;
	std::array<VkWriteDescriptorSet, cMAX_WRITES> writeInfo{/*zero it*/ };
	std::array<VkDescriptorImageInfo, cMAX_IMAGE_INFOS> imageInfo{/*zero it*/ };
	std::array<VkDescriptorBufferInfo, cMAX_BUFFER_INFOS> bufferInfo{};

	//	std::vector<m_descriptorSetBindings>

	uint32_t writeInfoIdx = 0;
	uint32_t imageInfoCount = 0;
	uint32_t bufferInfoCount = 0;

	// Go through the textures first
	for (const auto& textureBinding : mTextureBindings)
	{
		uint32_t bindingIndex = textureBinding.second.index;
		VkDescriptorType bindingType = textureBinding.second.type;

		const VulkanTexInfo* pTexture = (textureBinding.first.size() == 1) ? textureBinding.first[0] : textureBinding.first[bufferIdx]; // if there is only one binding then all bufferIdx's point to the same one.
		imageInfo[imageInfoCount] = pTexture->GetVkDescriptorImageInfo();
		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[bufferIdx];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = 1;
		writeInfo[writeInfoIdx].pBufferInfo = nullptr;
		writeInfo[writeInfoIdx].pImageInfo = &imageInfo[imageInfoCount];
		++writeInfoIdx;
		++imageInfoCount;
	}

	// Go through the images
	for (const auto& imageBinding : mImageBindings)
	{
		uint32_t bindingIndex = imageBinding.second.index;
		VkDescriptorType bindingType = imageBinding.second.type;
		assert(bindingType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);	// combined image sampler should go through mTextureBindings

		imageInfo[imageInfoCount].sampler = VK_NULL_HANDLE;
		imageInfo[imageInfoCount].imageView = imageBinding.first.imageView;
		imageInfo[imageInfoCount].imageLayout = imageBinding.first.imageLayout;
		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[bufferIdx];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = 1;
		writeInfo[writeInfoIdx].pBufferInfo = nullptr;
		writeInfo[writeInfoIdx].pImageInfo = &imageInfo[imageInfoCount];
		++writeInfoIdx;
		++imageInfoCount;
	}

	// Now do the buffers
	for (const auto& bufferBinding : mBufferBindings)
	{
		uint32_t bindingIndex = bufferBinding.second.index;
		VkDescriptorType bindingType = bufferBinding.second.type;

		VkBuffer buffer = (bufferBinding.first.size() == 1) ? bufferBinding.first[0] : bufferBinding.first[bufferIdx]; // if there is only one binding then all bufferIdx's point to the same one.
		bufferInfo[bufferInfoCount].buffer = buffer;
		bufferInfo[bufferInfoCount].offset = 0;
		bufferInfo[bufferInfoCount].range = VK_WHOLE_SIZE;
		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[bufferIdx];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = 1;
		writeInfo[writeInfoIdx].pBufferInfo = &bufferInfo[bufferInfoCount];
		writeInfo[writeInfoIdx].pImageInfo = nullptr;
		++writeInfoIdx;
		++bufferInfoCount;
	}

	LOGI("Updating Descriptor Set (bufferIdx %d) with %d objects", bufferIdx, writeInfoIdx);
	vkUpdateDescriptorSets(mVulkan.m_VulkanDevice, writeInfoIdx, writeInfo.data(), 0, NULL);
	LOGI("Descriptor Set Updated!");

	return true;
}


//
// Material class implementation
//

Material::Material(const Shader& shader)
	: m_shader(shader)
{}

Material::Material(Material&& other) noexcept
	: m_shader(other.m_shader)
	, m_materialPassNamesToIndex(std::move(other.m_materialPassNamesToIndex))
	, m_materialPasses(std::move(other.m_materialPasses))
{
}

Material::~Material()
{}


const MaterialPass* Material::GetMaterialPass(const std::string& passName) const
{
	auto it = m_materialPassNamesToIndex.find(passName);
	if (it != m_materialPassNamesToIndex.end())
	{
		return &m_materialPasses[it->second];
	}
	return nullptr;
}

bool Material::UpdateDescriptorSets(uint32_t bufferIdx)
{
	// Just go through and update the MaterialPass descriptor sets.
	// Could be much smarter (and could handle failures better than just continuing and reporting something went wrong)
	bool failure = false;
	for (auto& materialPass : m_materialPasses)
	{
		failure |= materialPass.UpdateDescriptorSets(bufferIdx);
	}
	return failure;
}
