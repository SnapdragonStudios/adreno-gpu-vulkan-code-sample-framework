//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <volk/volk.h>
#include <span>
#include "material/pipelineLayout.hpp"

// Forward declarations
class Vulkan;
template<typename T_GFXAPI> class DescriptorSetLayout;
struct CreateSamplerObjectInfo;


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

	operator bool() const { return m_pipelineLayout != VK_NULL_HANDLE; }

	bool Init(Vulkan& vulkan, const std::span<const DescriptorSetLayout<Vulkan>>, const std::span<const CreateSamplerObjectInfo>);
	bool Init(Vulkan& vulkan, const std::span<const VkDescriptorSetLayout> vkDescriptorSetLayouts);
	void Destroy(Vulkan& vulkan);

	const auto& GetVkPipelineLayout() const { return m_pipelineLayout; }
private:
	VkPipelineLayout	m_pipelineLayout = VK_NULL_HANDLE;
};
