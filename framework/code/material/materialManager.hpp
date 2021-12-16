// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <string>
#include <memory>
#include <functional>
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

// Forward declarations
class Vulkan;
class Material;
class MaterialPass;
class ShaderDescription;
class Shader;
class ShaderPass;
class VulkanTexInfo;


/// Helper class for creating Material 
/// @ingroup Material
class MaterialManager
{
	MaterialManager& operator=(const MaterialManager&) = delete;
	MaterialManager(const MaterialManager&) = delete;
public:
	typedef std::vector<const VulkanTexInfo*> tPerFrameTexInfo;
	typedef std::vector<VkBuffer> tPerFrameVkBuffer;

	MaterialManager();
	~MaterialManager();

	/// Create a material to be used for rendering (potentially contains multiple passes)
	Material CreateMaterial(Vulkan& vulkan, const Shader& shader, uint32_t numFrameBuffers, const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader, const std::function<tPerFrameVkBuffer(const std::string&)>& bufferLoader) const;
protected:
	/// Internal material pass creation (does not UpdateDescriptorSets)
	MaterialPass CreateMaterialPassInternal(Vulkan& vulkan, const ShaderPass& shaderPass, uint32_t numFrameBuffers, const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader, const std::function<tPerFrameVkBuffer(const std::string&)>& bufferLoader) const;
};
