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
#include "material/shaderModule.hpp"

// Forward declarations
class AssetManager;
class DescriptorSetLayoutBase;
class ShaderPassDescription;
class VertexDescription;
class VertexFormat;
class Dx12;

/// Wrapper around a Dx12 VkShaderModule.
/// @ingroup Material
template<>
class ShaderModule<Dx12> : public ShaderModuleBase
{
    ShaderModule(const ShaderModule<Dx12>&) = delete;
    ShaderModule& operator=(const ShaderModule<Dx12>&) = delete;
public:
    typedef std::vector<std::byte> tData;
    ShaderModule() noexcept;
    ~ShaderModule();

    /// Free up the shader memory.
    void Destroy(Dx12&);

    /// Load the shader binary with the given shader name
    /// @returns true on success
    bool Load(Dx12& vulkan, AssetManager& assetManager, const std::string& filename);

    /// Load the shader binary for the given shader type (using ShaderPassDescription to get the appropriate shader name).
    /// @returns true on success
    bool Load(Dx12&, AssetManager&, const ShaderPassDescription&, const ShaderType);

    const tData& GetShaderData() const { return m_Shader; }
private:
    tData  m_Shader;
};
