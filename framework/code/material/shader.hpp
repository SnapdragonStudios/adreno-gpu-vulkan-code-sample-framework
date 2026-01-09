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
#if !defined(_MSC_VER)
#include "descriptorSetLayout.hpp"
#endif
#include "pipelineLayout.hpp"
#include "pipelineVertexInputState.hpp"
#include "specializationConstantsLayout.hpp"

// forward declarations
class GraphicsApiBase;
class ShaderDescription;
template<typename T_GFXAPI> class Shader;
template<typename T_GFXAPI> class ShaderModule;
template<typename T_GFXAPI> class ShaderPass;
template<typename T_GFXAPI> class DescriptorSetLayout;
class ShaderPassDescription;

/// Reference to a 'normal' graphics pipeline pairing of a vertex and fragment shader
/// @ingroup Material
template<typename T_GFXAPI>
struct GraphicsShaderModules
{
    ShaderModule<T_GFXAPI>& vert;
    ShaderModule<T_GFXAPI>& frag;
};
/// Reference to a 'vertex only' graphics pipeline pairing of a vertex shader (no fragment, typically used when laying down a depth buffer)
/// @ingroup Material
template<typename T_GFXAPI>
struct GraphicsShaderModuleVertOnly
{
    ShaderModule<T_GFXAPI>& vert;
    operator const auto& () const { return vert; }
};
/// Reference to a 'mesh' graphics pipeline pairing of a mesh and fragment shader
/// @ingroup Material
template<typename T_GFXAPI>
struct GraphicsMeshShaderModules
{
    ShaderModule<T_GFXAPI>& mesh;
    ShaderModule<T_GFXAPI>& frag;
};
/// Reference to a 'task mesh' graphics pipeline pairing of a task, mesh and fragment shader
/// @ingroup Material
template<typename T_GFXAPI>
struct GraphicsTaskMeshShaderModules
{
    ShaderModule<T_GFXAPI>& task;
    ShaderModule<T_GFXAPI>& mesh;
    ShaderModule<T_GFXAPI>& frag;
};

/// Reference to a compute shader
/// @ingroup Material
template<typename T_GFXAPI>
struct ComputeShaderModule
{
    ShaderModule<T_GFXAPI>& compute;
    operator const ShaderModule<T_GFXAPI>& () const { return compute; }
};
/// Reference to a ray tracing pipeline with a ray-generation shader and some combination of ray-miss, ray-hit, ray-first-hit
/// @ingroup Material
template<typename T_GFXAPI>
struct RayTracingShaderModules
{
    ShaderModule<T_GFXAPI>& rayGeneration;
    const ShaderModule<T_GFXAPI>* rayClosestHit = nullptr;
    const ShaderModule<T_GFXAPI>* rayAnyHit = nullptr;
    const ShaderModule<T_GFXAPI>* rayMiss = nullptr;
};
/// Container for a reference to one of the allowed shader module types.
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderModules
{
    ShaderModules( const ShaderModules<T_GFXAPI>& ) = delete;
    ShaderModules& operator=( const ShaderModules<T_GFXAPI>& ) = delete;
public:
    ShaderModules( ShaderModules<T_GFXAPI>&& o ) noexcept = default;

    template<typename T>
    explicit ShaderModules( T&& m ) noexcept
        : m_modules(T( std::forward<T>( m ) ))
    {}
    template<typename T> const T& Get() const{ return std::get<T>( m_modules ); }
    const std::variant<GraphicsShaderModules<T_GFXAPI>, GraphicsShaderModuleVertOnly<T_GFXAPI>, GraphicsMeshShaderModules<T_GFXAPI>, GraphicsTaskMeshShaderModules<T_GFXAPI>, ComputeShaderModule<T_GFXAPI>, RayTracingShaderModules<T_GFXAPI>> m_modules;
};


class ShaderPassBase
{
    ShaderPassBase( const ShaderPassBase& ) = delete;
    ShaderPassBase& operator=( const ShaderPassBase& ) = delete;
    ShaderPassBase() = delete;
protected:
    ShaderPassBase( const ShaderPassDescription& shaderPassDescription ) noexcept : m_shaderPassDescription( shaderPassDescription )
    {}
    ShaderPassBase( ShaderPassBase&& other ) noexcept;///< @note currently implemented as an assert, so compiler does not complain (eg std::vector::push_back) but implementation expects this is never hit at runtime!
public:
    template<typename T_GFXAPI> using tApiDerived = ShaderPass<T_GFXAPI>; // make apiCast work!

    const ShaderPassDescription& m_shaderPassDescription;
};


/// Container for the gpu objects needed for a shader pass (described by ShaderPassDescription).
/// Typically used to create MaterialPass (s).
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderPass final : public ShaderPassBase
{
    using DescriptorSetLayout = DescriptorSetLayout<T_GFXAPI>;
    using PipelineLayout = PipelineLayout<T_GFXAPI>;
    using PipelineVertexInputState = PipelineVertexInputState<T_GFXAPI>;
    using ShaderModules = ShaderModules<T_GFXAPI>;
    using SpecializationConstantsLayout = SpecializationConstantsLayout<T_GFXAPI>;

    ShaderPass(const ShaderPass<T_GFXAPI>& src) = delete;
    ShaderPass& operator=(const ShaderPass<T_GFXAPI>& src) = delete;
    ShaderPass() = delete;
public:

    ShaderPass(const ShaderPassDescription& shaderPassDescription, ShaderModules shaders, std::vector<DescriptorSetLayout> descriptorSetLayouts, PipelineLayout, PipelineVertexInputState, SpecializationConstantsLayout, uint32_t numOutputs) noexcept;
    ShaderPass(ShaderPass<T_GFXAPI>&& other) noexcept;///< @note currently implemented as an assert, so compiler does not complain (eg std::vector::push_back) but implementation expects this is never hit at runtime!

    void Destroy(T_GFXAPI&);

    ShaderModules                                       m_shaders;

    const auto& GetPipelineLayout() const               { return m_pipelineLayout; }
    const auto& GetDescriptorSetLayouts() const         { return m_descriptorSetLayouts; }
    const auto& GetPipelineVertexInputState() const     { return m_pipelineVertexInputState; }
    const auto& GetSpecializationConstantsLayout()const { return m_specializationConstantsLayout; }
private:
    std::vector<DescriptorSetLayout>                m_descriptorSetLayouts;
    PipelineLayout                                  m_pipelineLayout;
    PipelineVertexInputState                        m_pipelineVertexInputState;
    SpecializationConstantsLayout                   m_specializationConstantsLayout;
    uint32_t                                        m_numOutputs = 0;

    static_assert(sizeof(T_GFXAPI) > 1);
};


/// @brief Base class for shaders (graphics api agnostic, expected to have a derived API specific implementation)
class ShaderBase
{
    ShaderBase(const ShaderBase&) = delete;
    ShaderBase& operator=(const ShaderBase&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = Shader<T_GFXAPI>; // make apiCast work!
    ShaderBase(const ShaderDescription* pShaderDescription) noexcept : m_shaderDescription(pShaderDescription) {}
    virtual ~ShaderBase() {}
    virtual void Destroy(GraphicsApiBase&) = 0;

    const auto& GetShaderPassIndicesToNames() const { return m_passIndexToName; }

    const ShaderDescription* m_shaderDescription = nullptr;

protected:
    std::map<const std::string, uint32_t>   m_passNameToIndex;  // maps shader pass name to m_shaderPasses[x]
    std::vector<std::reference_wrapper<const std::string>> m_passIndexToName;  // names in index order, eg reverse of m_passNameToIndex.  string referenced is m_passNameToIndex.key
};


/// Container for a vector of ShaderPass (with name lookup)
/// Typically used to create MaterialBase (s).
/// @ingroup Material
template<typename T_GFXAPI>
class Shader final : public ShaderBase
{
    Shader(const Shader<T_GFXAPI>&) = delete;
    Shader& operator=(const Shader<T_GFXAPI>&) = delete;
    using ShaderPass = ShaderPass<T_GFXAPI>;
public:
    Shader( const ShaderDescription* pShaderDescription, std::vector<ShaderPass> shaderPasses, const std::vector<std::string>& passNames) noexcept;
    ~Shader() override { assert(m_shaderPasses.empty()); }
    void Destroy(GraphicsApiBase&) override;

    const ShaderPass*const      GetShaderPass(const std::string& passName) const;
    const auto&                 GetShaderPasses() const { return m_shaderPasses; }

protected:
    std::vector<ShaderPass>     m_shaderPasses;     // in order of calls to AddShaderPass, concievably we could have shaders who share passes (eg shadow) with other Shaders, not currently supported though!
};

//
// Template implementation
//

template<typename T_GFXAPI>
ShaderPass<T_GFXAPI>::ShaderPass(const ShaderPassDescription& shaderPassDescription, ShaderModules shaders, std::vector<DescriptorSetLayout> descriptorSetLayouts, PipelineLayout pipelineLayout, PipelineVertexInputState pipelineVertexInputState, SpecializationConstantsLayout specializationConstantsLayout, uint32_t numOutputs) noexcept
    : ShaderPassBase(shaderPassDescription)
    , m_shaders(std::move(shaders))
    , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
    , m_pipelineLayout(std::move(pipelineLayout))
    , m_pipelineVertexInputState(std::move(pipelineVertexInputState))
    , m_specializationConstantsLayout(std::move(specializationConstantsLayout))
    , m_numOutputs(numOutputs)
{
}

template<typename T_GFXAPI>
ShaderPass<T_GFXAPI>::ShaderPass(ShaderPass<T_GFXAPI>&& other) noexcept
    : ShaderPassBase(other.m_shaderPassDescription)
    , m_shaders(std::move(other.m_shaders))
    , m_pipelineVertexInputState(std::move(other.m_pipelineVertexInputState))
    , m_specializationConstantsLayout(std::move(other.m_specializationConstantsLayout))
{
    // only here so we have std::vector<ShaderPass> and call push_back without compiler complaining.  Ensure vector is 'reserved' so it never has to hit this constructor
    assert(0);
}

template<typename T_GFXAPI>
void ShaderPass<T_GFXAPI>::Destroy(T_GFXAPI& gfxapi)
{
    m_pipelineLayout.Destroy(gfxapi);
    for (auto& dsl : m_descriptorSetLayouts)
        dsl.Destroy(gfxapi);
    m_descriptorSetLayouts.clear();
}

template<typename T_GFXAPI>
Shader<T_GFXAPI>::Shader(const ShaderDescription* pShaderDescription, std::vector<ShaderPass> shaderPasses, const std::vector<std::string>& passNames) noexcept
    : ShaderBase(pShaderDescription)
    , m_shaderPasses(std::move(shaderPasses))
{
    uint32_t idx = 0;
    m_passIndexToName.reserve(passNames.size());
    for (const auto& passName : passNames)
    {
        auto emplaced = m_passNameToIndex.try_emplace(passName, idx);
        m_passIndexToName.push_back(emplaced.first->first);
        ++idx;
    }
}

template<typename T_GFXAPI>
void Shader<T_GFXAPI>::Destroy(GraphicsApiBase& gfxapi)
{
    for (auto& sp : m_shaderPasses)
        sp.Destroy(static_cast<T_GFXAPI&>(gfxapi));
    m_shaderPasses.clear();
}

template<typename T_GFXAPI>
const ShaderPass<T_GFXAPI>* const Shader<T_GFXAPI>::GetShaderPass(const std::string& passName) const
{
    auto it = m_passNameToIndex.find(passName);
    if (it != m_passNameToIndex.end())
        return &m_shaderPasses[it->second];
    return nullptr;
}
