// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "shaderManager.hpp"
#include "shaderDescription.hpp"
#include "shaderModule.hpp"
#include "descriptorSetLayout.hpp"
#include "shader.hpp"
#include "vulkan/vulkan.hpp"
#include "system/os_common.h"
#include <memory>


ShaderManager::ShaderManager(Vulkan& vulkan)
    : m_vulkan(vulkan)
{}

ShaderManager::~ShaderManager()
{
    // Destroy the Shaders (which destroys the owned ShaderPasses etc)
    for (auto& s : m_shadersByName)
        s.second.Destroy(m_vulkan);

    // ShaderModules (do after Shaders, as the Shader(Pass) points to the ShaderModule)
    for (auto& sm : m_shaderModulesByName)
        sm.second.Destroy(m_vulkan);
}

void ShaderManager::RegisterRenderPassNames(const tcb::span<const char*const> passNames)
{
    assert(m_shaderPassIndexByName.size() == 0);    // only expected to be called once
    uint32_t passIndex = 0;
    for (const auto& passName : passNames)
    {
        if (!m_shaderPassIndexByName.emplace(passName, passIndex++).second)
        {
            LOGE("ShaderManager::RegisterRenderPassNames - duplicate pass name \"%s\"", passName);
        }
    }
}

bool ShaderManager::AddShader(Vulkan& vulkan, AssetManager& assetManager, const std::string& shaderName, const std::string& filename)
{
    auto shaderDescription = ShaderDescriptionLoader::Load(assetManager, filename);
    if (!shaderDescription)
    {
        return false;
    }
    return AddShader(vulkan, assetManager, shaderName, std::move(*shaderDescription));
}

bool ShaderManager::AddShader(Vulkan & vulkan, AssetManager & assetManager, const std::string & shaderName, ShaderDescription shaderDescription)
{
    bool success = true;
    // Create the ShaderDescription
    auto passDescriptionsPlaced = m_shaderDescriptionsByName.try_emplace(shaderName, std::move(shaderDescription));
    if (!passDescriptionsPlaced.second)
    {
        // Duplicate shader name
        return false;
    }

    // Get the location of ShaderDescription (never moves inside the map)
    const ShaderDescription* pShaderDescription = &(passDescriptionsPlaced.first->second);

    // Generate the shaderpasses
    std::vector<ShaderPass> shaderPasses;
    shaderPasses.reserve(passDescriptionsPlaced.first->second.m_passNameToIndex.size());
    std::vector<std::string> shaderPassNames;
    shaderPassNames.reserve(passDescriptionsPlaced.first->second.m_passNameToIndex.size());

    // Get the pass names in pass index order.
    std::vector<std::pair<std::string, uint32_t>> orderedPassNames;
    orderedPassNames.resize(passDescriptionsPlaced.first->second.m_passNameToIndex.size(), { std::string(), 0 });
    for( const auto& passNameIndex : passDescriptionsPlaced.first->second.m_passNameToIndex )
    {
        orderedPassNames[passNameIndex.second] = passNameIndex;
    }

    for (const auto& passIt : orderedPassNames)
    {
        const std::string passName = passIt.first;
        assert(!passName.empty());
        const ShaderPassDescription& shaderPassDescription = passDescriptionsPlaced.first->second.m_descriptionPerPass[passIt.second];    // once created the vector should not (MUST NOT) change size (reference will be stored by other classes)

        // Lookup pass name and get pass index (if name has not been registered skip loading/registering this shader pass)
        auto passIdxIt = m_shaderPassIndexByName.find(passName);
        if (passIdxIt == m_shaderPassIndexByName.end())
        {
            //LOGI("ShaderManager::AddShader - pass name \"%s\" not registered with RegisterRenderPassNames", passName.c_str());
            // This may not be an error (if we are manually grabbing or iterating passes from within a shader/material).
        }

        std::optional<ShaderModules> shaderModules;

        if (!shaderPassDescription.m_vertexName.empty())
        {
            // Create the vertex ShaderModule
            auto vertShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_vertexName);
            if (vertShader.second == false)
            {
                // already in the list!  This is ok (good)
            }
            else
            {
                // Load the physical shader file
                if (!vertShader.first->second.Load(vulkan, assetManager, shaderPassDescription, ShaderModule::ShaderType::Vertex))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_vertexName);
                    success = false;
                    break;
                }
            }
            if( shaderPassDescription.m_fragmentName.empty() )    // Dont have to have a fragment shader!
            {
                // Vertex only Module
                shaderModules.emplace( ShaderModules( GraphicsShaderModuleVertOnly{ vertShader.first->second } ) );
            }
            else
            {
                // Create the fragment ShaderModule
                auto fragShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_fragmentName);
                if (fragShader.second == false)
                {
                    // already in the list!  This is ok (good)
                }
                else
                {
                    // Load the physical shader file
                    if (!fragShader.first->second.Load(vulkan, assetManager, shaderPassDescription, ShaderModule::ShaderType::Fragment))
                    {
                        // Failed to load, remove the unloaded Shader class!
                        m_shaderModulesByName.erase(shaderPassDescription.m_fragmentName);
                        success = false;
                        break;
                    }
                }
                shaderModules.emplace( ShaderModules( GraphicsShaderModules{ vertShader.first->second, fragShader.first->second } ) );
            }
        }
        else if (!shaderPassDescription.m_computeName.empty())
        {
            // Create the compute ShaderModule
            auto computeShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_computeName);
            if (computeShader.second == false)
            {
                // already in the list!  This is ok (good)
            }
            else
            {
                // Load the physical shader file
                if (!computeShader.first->second.Load(vulkan, assetManager, shaderPassDescription, ShaderModule::ShaderType::Compute))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_computeName);
                    success = false;
                    break;
                }
            }
            shaderModules.emplace( ShaderModules( ComputeShaderModule{computeShader.first->second} ) );
        }
        else
        {
            // Error - must have a vertex shader or a compute shader
            success = false;
        }


        //
        // Add the ShaderPass to the Shader
        //

        //
        // Create the descriptor set layout (for the ShaderPass)
        //
        std::vector<DescriptorSetLayout> descriptorSetLayouts;

        descriptorSetLayouts.reserve(shaderPassDescription.m_sets.size());
        //std::vector<DescriptorSetDescription> dsd;
        for (const auto& set : shaderPassDescription.m_sets)
        {
            if (!descriptorSetLayouts.emplace_back().Init(vulkan, set))
            {
                // Error
                return false;
            }
        }

        //
        // Create the pipeline layout (for the ShaderPass)
        //
        PipelineLayout pipelineLayout;
        if (!pipelineLayout.Init(vulkan, descriptorSetLayouts))
        {
            // Error
            return false;
        }

        PipelineVertexInputState pipelineVertexInputState{ *pShaderDescription, shaderPassDescription.m_vertexFormatBindings };

        shaderPasses.emplace_back( shaderPassDescription, std::move(*shaderModules), std::move(descriptorSetLayouts), std::move(pipelineLayout), std::move(pipelineVertexInputState),(uint32_t)shaderPassDescription.m_outputs.size());
        shaderPassNames.push_back(passName);
    }


    if (!success)
    {
        // if we failed to load something then remove all knowledge of this shader
        m_shaderDescriptionsByName.erase(shaderName);
        return false;
    }

    // Create the Shader class
    auto shader = m_shadersByName.try_emplace(shaderName, pShaderDescription, std::move(shaderPasses), shaderPassNames);
    if (!shader.second)
    {
        // Duplicate shader name
        return false;
    }

    return true;
}

const ShaderDescription* ShaderManager::GetShaderDescription(const std::string& shaderName) const
{
    auto it = m_shaderDescriptionsByName.find(shaderName);
    if (it != m_shaderDescriptionsByName.end())
        return &(it->second);
    return nullptr;
}

const Shader* ShaderManager::GetShader(const std::string& shaderName) const
{
    auto it = m_shadersByName.find(shaderName);
    if (it != m_shadersByName.end())
        return &(it->second);
    return nullptr;
}

