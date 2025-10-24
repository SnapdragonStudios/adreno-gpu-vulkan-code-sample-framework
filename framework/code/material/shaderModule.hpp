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
//#include <future>
#include <memory>

// Forward declarations
class AssetManager;
class DescriptorSetLayoutBase;
class ShaderPassDescription;
class VertexDescription;
class VertexFormat;
template<typename T_GFXAPI> class ShaderModule;

class ShaderModuleBase
{
    ShaderModuleBase(const ShaderModuleBase&) = delete;
    ShaderModuleBase& operator=(const ShaderModuleBase&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = ShaderModule<T_GFXAPI>; // make apiCast work!
    ShaderModuleBase() noexcept {}
    ~ShaderModuleBase() {}

    enum class ShaderType {
        Fragment, Vertex, Compute, RayGeneration, RayClosestHit, RayAnyHit, RayMiss, Task, Mesh
    };

    void Destroy()
    {
        m_filename.clear();
    }

protected:
    std::string         m_filename;
};

/// Templated ShaderBase module (container for shader code)
/// Expected to be specialized (by the graphics api)
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderModule : private ShaderModuleBase
{
    ShaderModule(const ShaderModule<T_GFXAPI>&) = delete;
    ShaderModule& operator=(const ShaderModule<T_GFXAPI>&) = delete;
public:
    ShaderModule() noexcept = delete;   // this template class must be specialized

    /// Free up the vkShaderModule resource.
    void Destroy(T_GFXAPI&) = delete;

    /// Load the shader binary with the given shader name
    /// @returns true on success
    bool Load(T_GFXAPI& vulkan, AssetManager& assetManager, const std::string& filename);

    /// Load the shader binary for the given shader type (using ShaderPassDescription to get the appropriate shader name).
    /// @returns true on success
    bool Load(T_GFXAPI&, AssetManager&, const ShaderPassDescription&, const ShaderType);

    static_assert(sizeof(ShaderModule<T_GFXAPI>) != sizeof(ShaderModuleBase));   // Ensure this class template is specialized (and not used as-is)
};
