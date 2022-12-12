//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "tcb/span.hpp"

// Forward declarations
class Vulkan;
class DescriptorSetLayout;


/// Simple wrapper around VkPipelineLayout.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// @ingroup Material
class PipelineLayout
{
	PipelineLayout& operator=(const PipelineLayout&) = delete;
	PipelineLayout(const PipelineLayout&) = delete;
public:
	PipelineLayout();
	PipelineLayout(PipelineLayout&&) noexcept;
	~PipelineLayout();

	bool Init(Vulkan& vulkan, const tcb::span<const DescriptorSetLayout>);
	bool Init(Vulkan& vulkan, const tcb::span<const VkDescriptorSetLayout> vkDescriptorSetLayouts);
	void Destroy(Vulkan& vulkan);

	const auto& GetVkPipelineLayout() const { return m_pipelineLayout; }
private:
	VkPipelineLayout	m_pipelineLayout = VK_NULL_HANDLE;
};
