//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
template<typename T_GFXAPI> class ShaderT;
template<typename T_GFXAPI> class ShaderModuleT;
class ShaderPassDescription;

/// Reference to a 'normal' graphics pipeline pairing of a vertex and fragment shader
/// @ingroup Material
template<typename T_GFXAPI>
struct GraphicsShaderModules
{
    ShaderModuleT<T_GFXAPI>& vert;
    ShaderModuleT<T_GFXAPI>& frag;
};
/// Reference to a 'vertex only' graphics pipeline pairing of a vertex shader (no fragment, typically used when laying down a depth buffer)
/// @ingroup Material
template<typename T_GFXAPI>
struct GraphicsShaderModuleVertOnly
{
    ShaderModuleT<T_GFXAPI>& vert;
    operator const auto& () const { return vert; }
};
/// Reference to a compute shader
/// @ingroup Material
template<typename T_GFXAPI>
struct ComputeShaderModule
{
    ShaderModuleT<T_GFXAPI>& compute;
    operator const ShaderModuleT<T_GFXAPI>& () const { return compute; }
};
/// Reference to a ray tracing pipeline with a ray-generation shader and some combination of ray-miss, ray-hit, ray-first-hit
/// @ingroup Material
template<typename T_GFXAPI>
struct RayTracingShaderModules
{
    ShaderModuleT<T_GFXAPI>& rayGeneration;
    const ShaderModuleT<T_GFXAPI>* rayClosestHit = nullptr;
    const ShaderModuleT<T_GFXAPI>* rayAnyHit = nullptr;
    const ShaderModuleT<T_GFXAPI>* rayMiss = nullptr;
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
    const std::variant<GraphicsShaderModules<T_GFXAPI>, GraphicsShaderModuleVertOnly<T_GFXAPI>, ComputeShaderModule<T_GFXAPI>, RayTracingShaderModules<T_GFXAPI>> m_modules;
};


/// Container for the gpu objects needed for a shader pass (described by ShaderPassDescription).
/// Typically used to create MaterialPass (s).
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderPass
{
    ShaderPass(const ShaderPass<T_GFXAPI>& src) = delete;
    ShaderPass& operator=(const ShaderPass<T_GFXAPI>& src) = delete;
    ShaderPass() = delete;
public:
    ShaderPass(const ShaderPassDescription& shaderPassDescription, ShaderModules<T_GFXAPI> shaders, std::vector<DescriptorSetLayout> descriptorSetLayouts, PipelineLayout<T_GFXAPI>, PipelineVertexInputState<T_GFXAPI>, SpecializationConstantsLayout<T_GFXAPI>, uint32_t numOutputs) noexcept;
    ShaderPass(ShaderPass<T_GFXAPI>&& other) noexcept;///< @note currently implemented as an assert, so compiler does not complain (eg std::vector::push_back) but implementation expects this is never hit at runtime!

    void Destroy(T_GFXAPI&);

    ShaderModules<T_GFXAPI>                         m_shaders;

    const auto& GetPipelineLayout() const               { return m_pipelineLayout; }
    const auto& GetDescriptorSetLayouts() const         { return m_descriptorSetLayouts; }
    const auto& GetPipelineVertexInputState() const     { return m_pipelineVertexInputState; }
    const auto& GetSpecializationConstantsLayout()const { return m_specializationConstantsLayout; }

    const ShaderPassDescription&                    m_shaderPassDescription;
private:
    std::vector<DescriptorSetLayout>                m_descriptorSetLayouts;
    PipelineLayout<T_GFXAPI>                        m_pipelineLayout;
    PipelineVertexInputState<T_GFXAPI>              m_pipelineVertexInputState;
    SpecializationConstantsLayout<T_GFXAPI>         m_specializationConstantsLayout;
    uint32_t                                        m_numOutputs = 0;

    static_assert(sizeof(T_GFXAPI) > 1);
};


/// @brief Base class for shaders (graphics api agnostic, expected to have a derived API specific implementation)
class Shader
{
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = ShaderT<T_GFXAPI>; // make apiCast work!
    Shader(const ShaderDescription* pShaderDescription) noexcept : m_shaderDescription(pShaderDescription) {}
    virtual ~Shader() {}
    virtual void Destroy(GraphicsApiBase&) = 0;

    const ShaderDescription* m_shaderDescription = nullptr;
};


/// Container for a vector of ShaderPass (with name lookup)
/// Typically used to create Material (s).
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderT : public Shader
{
    ShaderT(const ShaderT<T_GFXAPI>&) = delete;
    ShaderT& operator=(const ShaderT<T_GFXAPI>&) = delete;
public:
    ShaderT( const ShaderDescription* pShaderDescription, std::vector<ShaderPass<T_GFXAPI>> shaderPasses, const std::vector<std::string>& passNames) noexcept;
    ~ShaderT() override { assert(m_shaderPasses.empty()); }
    void Destroy(GraphicsApiBase&) override;

    const ShaderPass<T_GFXAPI>* GetShaderPass(const std::string& passName) const;
    const auto& GetShaderPasses() const { return m_shaderPasses; }
    const auto& GetShaderPassIndicesToNames() const { return m_passIndexToName; }

protected:
    std::vector<ShaderPass<T_GFXAPI>>       m_shaderPasses;     // in order of calls to AddShaderPass, concievably we could have shaders who share passes (eg shadow) with other Shaders, not currently supported though!
    std::map<const std::string, uint32_t>   m_passNameToIndex;  // maps shader pass name to m_shaderPasses[x]
    std::vector<std::reference_wrapper<const std::string>> m_passIndexToName;  // names in index order, eg reverse of m_passNameToIndex.  string referenced is m_passNameToIndex.key
};

//
// Template implementation
//

template<typename T_GFXAPI>
ShaderPass<T_GFXAPI>::ShaderPass(const ShaderPassDescription& shaderPassDescription, ShaderModules<T_GFXAPI> shaders, std::vector<DescriptorSetLayout> descriptorSetLayouts, PipelineLayout<T_GFXAPI> pipelineLayout, PipelineVertexInputState<T_GFXAPI> pipelineVertexInputState, SpecializationConstantsLayout<T_GFXAPI> specializationConstantsLayout, uint32_t numOutputs) noexcept
    : m_shaders(std::move(shaders))
    , m_shaderPassDescription(shaderPassDescription)
    , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
    , m_pipelineLayout(std::move(pipelineLayout))
    , m_pipelineVertexInputState(std::move(pipelineVertexInputState))
    , m_specializationConstantsLayout(std::move(specializationConstantsLayout))
    , m_numOutputs(numOutputs)
{
}

template<typename T_GFXAPI>
ShaderPass<T_GFXAPI>::ShaderPass(ShaderPass<T_GFXAPI>&& other) noexcept
    : m_shaders(std::move(other.m_shaders))
    , m_shaderPassDescription(other.m_shaderPassDescription)
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
ShaderT<T_GFXAPI>::ShaderT(const ShaderDescription* pShaderDescription, std::vector<ShaderPass<T_GFXAPI>> shaderPasses, const std::vector<std::string>& passNames) noexcept
    : Shader(pShaderDescription)
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
void ShaderT<T_GFXAPI>::Destroy(GraphicsApiBase& gfxapi)
{
    for (auto& sp : m_shaderPasses)
        sp.Destroy(static_cast<T_GFXAPI&>(gfxapi));
    m_shaderPasses.clear();
}

template<typename T_GFXAPI>
const ShaderPass<T_GFXAPI>* ShaderT<T_GFXAPI>::GetShaderPass(const std::string& passName) const
{
    auto it = m_passNameToIndex.find(passName);
    if (it != m_passNameToIndex.end())
        return &m_shaderPasses[it->second];
    return nullptr;
}
