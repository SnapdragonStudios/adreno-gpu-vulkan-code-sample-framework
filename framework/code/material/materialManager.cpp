// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "materialManager.hpp"
#include "descriptorSetLayout.hpp"
#include "material.hpp"
#include "shader.hpp"
#include "shaderDescription.hpp"
#include "system/os_common.h"
#include "vulkan/vulkan.hpp"

MaterialManager::MaterialManager()
{}

MaterialManager::~MaterialManager()
{}


MaterialPass MaterialManager::CreateMaterialPassInternal(
	Vulkan& vulkan,
	const ShaderPass& shaderPass,
	uint32_t numFrameBuffers,
	const std::function<const MaterialPass::tPerFrameTexInfo(const std::string&)>& textureLoader,
	const std::function<MaterialPass::tPerFrameVkBuffer(const std::string&)>& bufferLoader) const
{
	//
	// Create the descriptor set pool
	// We should do this 'beter'.  For now just make a set of pools that covers this shader's pass.
	//

	// Collate all the pool sizes for identical descriptor types
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (const auto& layout : shaderPass.GetDescriptorSetLayouts())
	{
		if (poolSizes.size() == 0)
			poolSizes = layout.GetDescriptorPoolSizes();
		else
		{
			///TODO: n^2 search, 'could' be slowish - although not normal to have more than one or two sets
			for (const auto& newPoolSize : layout.GetDescriptorPoolSizes())
			{
				bool found = false;
				for (auto& poolSize : poolSizes)
				{
					if (poolSize.type == newPoolSize.type)
					{
						poolSize.descriptorCount += newPoolSize.descriptorCount;
						found = true;
						break;
					}
				}
				if (!found)
					poolSizes.push_back(newPoolSize);
			}
		}
	}

	for (auto& poolSize : poolSizes)
		poolSize.descriptorCount *= numFrameBuffers;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	//PoolInfo.flags = 0;     // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to allow them to be returned
	poolInfo.maxSets = numFrameBuffers;  // Since descriptor sets come out of this pool we need more than one
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	if (VK_SUCCESS != vkCreateDescriptorPool(vulkan.m_VulkanDevice, &poolInfo, NULL, &descriptorPool))
	{
		assert(0);
	}

	std::vector<VkDescriptorSet> descriptorSets;
	descriptorSets.reserve(numFrameBuffers);	// pre-size so memory does not move in emlace_back

	for (uint32_t whichBuffer = 0; whichBuffer < numFrameBuffers; ++whichBuffer)
	{
		//
		// Create the descriptor set
		//
		VkDescriptorSetAllocateInfo descSetInfo = {};
		descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descSetInfo.pNext = NULL;
		descSetInfo.descriptorPool = descriptorPool;
		descSetInfo.descriptorSetCount = (uint32_t)shaderPass.GetDescriptorSetLayouts().size();
		std::vector<VkDescriptorSetLayout> vkSetLayouts;	// should be stack allocated or stored in the shader pass
		vkSetLayouts.reserve(descSetInfo.descriptorSetCount);
		for (const auto& setLayout : shaderPass.GetDescriptorSetLayouts())
			vkSetLayouts.push_back(setLayout.GetVkDescriptorSetLayout());
		descSetInfo.pSetLayouts = vkSetLayouts.data();
		if (VK_SUCCESS != vkAllocateDescriptorSets(vulkan.m_VulkanDevice, &descSetInfo, &descriptorSets.emplace_back((VkDescriptorSet)VK_NULL_HANDLE)))
		{
			assert(0);
		}
	}

	//
	// Now gather the (named) texture and uniform buffer slots (for this material pass)
	MaterialPass::tTextureBindings textureBindings;
	MaterialPass::tImageBindings imageBindings;
	MaterialPass::tBufferBindings bufferBindings;

	for (const auto& descSetLayout : shaderPass.GetDescriptorSetLayouts())
	{
		const auto& layoutBindingsArray = descSetLayout.GetVkDescriptorSetLayoutBinding();
		for (const auto& bindingNames : descSetLayout.GetNameToBinding())
		{
			const std::string& bindingName = bindingNames.first;
			const DescriptorSetLayout::BindingTypeAndIndex& binding = bindingNames.second;
			switch (binding.type)
			{
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				MaterialPass::tPerFrameTexInfo pTextures = textureLoader(bindingName);	// Get the texture(s) from the callback
				textureBindings.push_back({ std::move(pTextures), binding });
				break;
			}
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			{
				MaterialPass::tPerFrameTexInfo pTextures = textureLoader(bindingName);	// Get the texture(s) from the callback
				MaterialPass::ImageInfo imageInfo( **pTextures.begin() );
				imageBindings.push_back({ std::move(imageInfo), binding });
				break;
			}
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			{
				MaterialPass::tPerFrameTexInfo pTextures = textureLoader(bindingName);	// Get the texture(s) from the callback
				MaterialPass::ImageInfo imageInfo(**pTextures.begin());
				imageBindings.push_back({ std::move(imageInfo), binding });
				break;
			}
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			{
				MaterialPass::tPerFrameVkBuffer vkBuffers = bufferLoader(bindingName);	// Get the buffer(s) from the callback
				bufferBindings.push_back({ std::move(vkBuffers), binding });
				break;
			}
			default:
				assert(0);	// needs implementing
				break;
			}
		}
	}

	return MaterialPass(vulkan, shaderPass, std::move(descriptorPool), std::move(descriptorSets), std::move(textureBindings), std::move(imageBindings), std::move(bufferBindings));
}

Material MaterialManager::CreateMaterial(Vulkan& vulkan, const Shader& shader, uint32_t numFrameBuffers, const std::function<const MaterialPass::tPerFrameTexInfo(const std::string&)>& textureLoader, const std::function<MaterialPass::tPerFrameVkBuffer(const std::string&)>& bufferLoader) const
{
	Material material( shader );

	// We do each pass seperately.
	const ShaderDescription* pShaderDescription = shader.m_shaderDescription;

	// iterate over the passes (in their index order - iterating std::map gaurantees to iterate in key order)
	const auto& shaderPasses = shader.GetShaderPasses();
	for (const auto& shaderPassIdxAndName : shader.GetShaderPassIndicesToNames())
	{
		const std::string& passName = shaderPassIdxAndName.second;
		const ShaderPass& shaderPass = shaderPasses[shaderPassIdxAndName.first];

		material.AddMaterialPass(passName, std::move( CreateMaterialPassInternal(vulkan, shaderPass, numFrameBuffers, textureLoader, bufferLoader) ));
	}

	for (uint32_t whichBuffer = 0; whichBuffer < numFrameBuffers; ++whichBuffer)
	{
		material.UpdateDescriptorSets(whichBuffer);
	}

	return material;
}
