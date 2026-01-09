//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <functional>
#include <vector>
#include <future>
#include <memory>
#include <volk/volk.h>
#include "material/shaderModule.hpp"

// Forward declarations
class AssetManager;
class DescriptorSetLayoutBase;
class ShaderPassDescription;
class VertexDescription;
class VertexFormat;
class Vulkan;

/// Wrapper around a Vulkan VkShaderModule.
/// @ingroup Material
template<>
class ShaderModule<Vulkan> : public ShaderModuleBase
{
    ShaderModule(const ShaderModule<Vulkan>&) = delete;
    ShaderModule& operator=(const ShaderModule<Vulkan>&) = delete;
public:
    ShaderModule() noexcept;
    ~ShaderModule();

    /// Free up the vkShaderModule resource.
    void Destroy(Vulkan&);

    /// Load the shader binary with the given shader name
    /// @returns true on success
    bool Load(Vulkan& vulkan, AssetManager& assetManager, const std::string& filename);

    /// Load the shader binary for the given shader type (using ShaderPassDescription to get the appropriate shader name).
    /// @returns true on success
    bool Load(Vulkan&, AssetManager&, const ShaderPassDescription&, const ShaderType);

    const VkShaderModule& GetVkShaderModule() const { return m_shader; }
private:
    VkShaderModule      m_shader;
};
