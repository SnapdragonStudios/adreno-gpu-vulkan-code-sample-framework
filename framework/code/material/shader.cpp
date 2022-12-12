//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shader.hpp"
#include "shaderModule.hpp"
#include "descriptorSetLayout.hpp"
#include "pipelineLayout.hpp"
#include <cassert>

Shader::Shader(const ShaderDescription* pShaderDescription, std::vector<ShaderPass>&& shaderPasses, const std::vector<std::string>& passNames)
    : m_shaderDescription(pShaderDescription)
    , m_shaderPasses(std::move(shaderPasses))
{
    uint32_t idx = 0;
    for (const auto& passName : passNames)
    {
        auto emplaced = m_passNameToIndex.try_emplace(passName, idx);
        m_passIndexToName.try_emplace(idx, emplaced.first->first);
        ++idx;
    }
}

void Shader::Destroy(Vulkan& vulkan)
{
    for (auto& sp : m_shaderPasses)
        sp.Destroy(vulkan);
    m_shaderPasses.clear();
}

const ShaderPass* Shader::GetShaderPass(const std::string& passName) const
{
    auto it = m_passNameToIndex.find(passName);
    if (it != m_passNameToIndex.end())
        return &m_shaderPasses[it->second];
    return nullptr;
}


ShaderPass::ShaderPass(const ShaderPassDescription& shaderPassDescription, ShaderModules shaders, std::vector<DescriptorSetLayout> descriptorSetLayouts, PipelineLayout pipelineLayout, PipelineVertexInputState pipelineVertexInputState, uint32_t numOutputs)
    : m_shaderPassDescription(shaderPassDescription)
    , m_pipelineLayout(std::move(pipelineLayout))
    , m_shaders(std::move(shaders))
    , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
    , m_pipelineVertexInputState(std::move(pipelineVertexInputState))
    , m_numOutputs(numOutputs)
{
}

ShaderPass::ShaderPass(ShaderPass&& other) noexcept
    : m_shaderPassDescription(other.m_shaderPassDescription)
    , m_pipelineVertexInputState(std::move(other.m_pipelineVertexInputState))
    , m_shaders(std::move(other.m_shaders))
{
    // only here so we have std::vector<ShaderPass> and call push_back without compiler complaining.  Ensure vector is 'reserved' so it never has to hit this constructor
    assert(0);
}

void ShaderPass::Destroy(Vulkan& vulkan)
{
    m_pipelineLayout.Destroy(vulkan);
    for (auto& dsl : m_descriptorSetLayouts)
        dsl.Destroy(vulkan);
    m_descriptorSetLayouts.clear();
}

