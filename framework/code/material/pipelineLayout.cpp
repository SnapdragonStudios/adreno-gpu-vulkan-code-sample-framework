// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "pipelineLayout.hpp"
#include "descriptorSetLayout.hpp"
#include "vulkan/vulkan.hpp"

PipelineLayout::PipelineLayout()
{}

PipelineLayout::PipelineLayout(PipelineLayout&& other) noexcept
{
	m_pipelineLayout = other.m_pipelineLayout;
	other.m_pipelineLayout = VK_NULL_HANDLE;
}

PipelineLayout::~PipelineLayout()
{
	assert(m_pipelineLayout == VK_NULL_HANDLE);	// call Destroy prior to object being deleted
}

void PipelineLayout::Destroy(Vulkan& vulkan)
{
	if (m_pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(vulkan.m_VulkanDevice, m_pipelineLayout, nullptr);
		m_pipelineLayout = VK_NULL_HANDLE;
	}
}

bool PipelineLayout::Init(Vulkan& vulkan, const std::vector<DescriptorSetLayout>& descriptorSetLayouts)
{
	std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
	vkDescriptorSetLayouts.reserve(descriptorSetLayouts.size());
	for (const auto& descriptorSetLayout : descriptorSetLayouts)
	{
		vkDescriptorSetLayouts.push_back(descriptorSetLayout.GetVkDescriptorSetLayout());
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = (uint32_t)vkDescriptorSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();

	VkResult retVal = vkCreatePipelineLayout(vulkan.m_VulkanDevice, &pipelineLayoutInfo, NULL, &m_pipelineLayout);
	if (!CheckVkError("vkCreatePipelineLayout()", retVal))
	{
		return false;
	}
	return true;
}
