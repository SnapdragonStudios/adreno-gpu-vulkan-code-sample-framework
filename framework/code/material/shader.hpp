//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <vulkan/vulkan.h>
#include "pipelineLayout.hpp"
#include "pipelineVertexInputState.hpp"

// forward declarations
class DescriptorSetLayout;
class ShaderDescription;
class ShaderModule;
class ShaderPassDescription;

/// Reference to a 'normal' graphics pipeline pairing of a vertex and fragment shader
/// @ingroup Material
struct GraphicsShaderModules
{
    ShaderModule& vert;
    ShaderModule& frag;
};
/// Reference to a 'vertex only' graphics pipeline pairing of a vertex shader (no fragment, typically used when laying down a depth buffer)
/// @ingroup Material
struct GraphicsShaderModuleVertOnly
{
    ShaderModule& vert;
    operator const ShaderModule& () const { return vert; }
};
/// Reference to a compute shader
/// @ingroup Material
struct ComputeShaderModule
{
    ShaderModule& compute;
    operator const ShaderModule& () const { return compute; }
};
/// Container for a reference to one of the allowed shader module types.
/// @ingroup Material
class ShaderModules
{
    ShaderModules( const ShaderModules& ) = delete;
    ShaderModules& operator=( const ShaderModules& ) = delete;
public:
    template<typename T>
    ShaderModules( T&& m ) 
        : m_modules(T( std::forward<T>( m ) ))
    {}
    template<> ShaderModules( ShaderModules&& o ) : m_modules( std::move( o.m_modules ) ) {}
    template<typename T> const T& Get() const{ return std::get<T>( m_modules ); }
    const std::variant<GraphicsShaderModules, GraphicsShaderModuleVertOnly, ComputeShaderModule> m_modules;
};

/// Container for the Vulkan objects needed for a shader pass (described by ShaderPassDescription).
/// Typically used to create MaterialPass (s).
/// @ingroup Material
class ShaderPass
{
    ShaderPass(const ShaderPass& src) = delete;
    ShaderPass& operator=(const ShaderPass& src) = delete;
    ShaderPass() = delete;
public:
    ShaderPass(const ShaderPassDescription& shaderPassDescription, ShaderModules shaders, std::vector<DescriptorSetLayout> descriptorSetLayouts, PipelineLayout, PipelineVertexInputState, uint32_t numOutputs);// , Hash hash);
    ShaderPass(ShaderPass&& other) noexcept;///< @note currently implemented as an assert, so compiler does not complain (eg std::vector::push_back) but implementation expects this is never hit at runtime!

    void Destroy(Vulkan& vulkan);

    ShaderModules                                   m_shaders;

    const auto& GetPipelineLayout() const           { return m_pipelineLayout; }
    const auto& GetDescriptorSetLayouts() const     { return m_descriptorSetLayouts; }
    const auto& GetPipelineVertexInputState() const { return m_pipelineVertexInputState; }

    const ShaderPassDescription&                    m_shaderPassDescription;
private:
    std::vector<DescriptorSetLayout>                m_descriptorSetLayouts;
    PipelineLayout                                  m_pipelineLayout;
    PipelineVertexInputState                        m_pipelineVertexInputState;
    uint32_t                                        m_numOutputs = 0;
};

/// Container for a vector of ShaderPass (with name lookup)
/// Typically used to create Material (s).
/// @ingroup Material
class Shader
{
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
public:
    Shader( const ShaderDescription* pShaderDescription, std::vector<ShaderPass>&& shaderPasses, const std::vector<std::string>& passNames);
    void Destroy(Vulkan& vulkan);

    const ShaderPass* GetShaderPass(const std::string& passName) const;
    const auto& GetShaderPasses() const { return m_shaderPasses; }
    const auto& GetShaderPassIndicesToNames() const { return m_passIndexToName; }

    const ShaderDescription* m_shaderDescription = nullptr;
protected:
    std::vector<ShaderPass> m_shaderPasses;    // in order of calls to AddShaderPass, concievably we could have shaders who share passes (eg shadow) with other Shaders, not currently supported though!
    std::map<const std::string, uint32_t> m_passNameToIndex;    // maps shader pass name to m_shaderPasses[x]
    std::map<uint32_t, const std::string&> m_passIndexToName;    // maps index of shader (in m_shaderPasses) to name, eg reverse of m_passNameToIndex
};
