//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <span>
#include "material/pipelineLayout.hpp"

// Forward declarations
class Vulkan;
class DescriptorSetLayout;


/// Simple wrapper around VkPipelineLayout.
/// Teamplate specialization for Vulkan.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// @ingroup Material
template<>
class PipelineLayout<Vulkan>
{
	PipelineLayout& operator=(const PipelineLayout<Vulkan>&) = delete;
	PipelineLayout(const PipelineLayout<Vulkan>&) = delete;
public:
	PipelineLayout() noexcept;
	PipelineLayout(PipelineLayout<Vulkan>&&) noexcept;
	~PipelineLayout();

	bool Init(Vulkan& vulkan, const std::span<const DescriptorSetLayout>);
	bool Init(Vulkan& vulkan, const std::span<const VkDescriptorSetLayout> vkDescriptorSetLayouts);
	void Destroy(Vulkan& vulkan);

	const auto& GetVkPipelineLayout() const { return m_pipelineLayout; }
private:
	VkPipelineLayout	m_pipelineLayout = VK_NULL_HANDLE;
};
