//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <functional>
#include <vector>
//#include <future>
#include <memory>

// Forward declarations
class AssetManager;
class DescriptorSetLayout;
class ShaderPassDescription;
class VertexDescription;
class VertexFormat;
template<typename T_GFXAPI> class ShaderModuleT;

class ShaderModule
{
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = ShaderModuleT<T_GFXAPI>; // make apiCast work!
    ShaderModule() noexcept {}
    ~ShaderModule() {}

    enum class ShaderType {
        Fragment, Vertex, Compute, RayGeneration, RayClosestHit, RayAnyHit, RayMiss
    };

    void Destroy()
    {
        m_filename.clear();
    }

protected:
    std::string         m_filename;
};

/// Templated Shader module (container for shader code)
/// Expected to be specialized (by the graphics api)
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderModuleT : private ShaderModule
{
    ShaderModuleT(const ShaderModuleT<T_GFXAPI>&) = delete;
    ShaderModuleT& operator=(const ShaderModuleT<T_GFXAPI>&) = delete;
public:
    ShaderModuleT() noexcept = delete;   // this template class must be specialized

    /// Free up the vkShaderModule resource.
    void Destroy(T_GFXAPI&) = delete;

    /// Load the shader binary with the given shader name
    /// @returns true on success
    bool Load(T_GFXAPI& vulkan, AssetManager& assetManager, const std::string& filename);

    /// Load the shader binary for the given shader type (using ShaderPassDescription to get the appropriate shader name).
    /// @returns true on success
    bool Load(T_GFXAPI&, AssetManager&, const ShaderPassDescription&, const ShaderType);

    static_assert(sizeof(ShaderModuleT<T_GFXAPI>) != sizeof(ShaderModule));   // Ensure this class template is specialized (and not used as-is)
};
