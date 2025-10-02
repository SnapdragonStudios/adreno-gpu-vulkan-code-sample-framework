//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include "shaderManager.hpp"
#include "shader.hpp"
#include "shaderDescription.hpp"
#include "shaderModule.hpp"

/// Tempated (by graphics api) ShaderManager.
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderManagerT : public ShaderManager
{
    ShaderManagerT() = delete;
    ShaderManagerT& operator=(const ShaderManager&) = delete;
    ShaderManagerT(const ShaderManagerT<T_GFXAPI>&) = delete;
public:
    ShaderManagerT(T_GFXAPI& gpxApi);
    ~ShaderManagerT();

protected:
    /// @brief Load all referenced shaders modules from the given description. Makes shader ready for material to be created from it.
    /// @param shaderName name to be given to this shader for lookup within the application code (user determined name).
    /// @param shaderDescription description of this shader
    /// @return true if everything loaded correctly.
    bool AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription) override;

protected:
};



template<typename T_GFXAPI>
ShaderManagerT<T_GFXAPI>::ShaderManagerT(T_GFXAPI& gfxApi) : ShaderManager(gfxApi)
{}

template<typename T_GFXAPI>
ShaderManagerT<T_GFXAPI>::~ShaderManagerT()
{
    // Destroy the Shaders (which destroys the owned ShaderPasses etc)
    auto& rVulkan = static_cast<T_GFXAPI&>(m_GraphicsApi);
    for (auto& s : m_shadersByName)
        s.second->Destroy(rVulkan);
    m_shadersByName.clear();

    // ShaderModules (do after Shaders, as the Shader(Pass) points to the ShaderModule)
    // If you get an error here then maybe you haven't included the 'ShaderModule.hpp' for your graphics API !
    for (auto& sm : m_shaderModulesByName)
        apiCast<T_GFXAPI>(sm.second.get())->Destroy(rVulkan);
    m_shaderModulesByName.clear();
}

template<typename T_GFXAPI>
bool ShaderManagerT<T_GFXAPI>::AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription)
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
    std::vector<ShaderPass<T_GFXAPI>> shaderPasses;
    shaderPasses.reserve(passDescriptionsPlaced.first->second.m_passNameToIndex.size());
    std::vector<std::string> shaderPassNames;
    shaderPassNames.reserve(passDescriptionsPlaced.first->second.m_passNameToIndex.size());

    // Get the pass names in pass index order.
    std::vector<std::pair<std::string, uint32_t>> orderedPassNames;
    orderedPassNames.resize(passDescriptionsPlaced.first->second.m_passNameToIndex.size(), { std::string(), 0 });
    for (const auto& passNameIndex : passDescriptionsPlaced.first->second.m_passNameToIndex)
    {
        orderedPassNames[passNameIndex.second] = passNameIndex;
    }

    T_GFXAPI& rGfxApi = static_cast<T_GFXAPI&>(m_GraphicsApi);

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

        std::optional<ShaderModules<T_GFXAPI>> shaderModules;

        if (shaderPassDescription.m_vertexName.empty() && !shaderPassDescription.m_taskName.empty() && !shaderPassDescription.m_meshName.empty() && !shaderPassDescription.m_fragmentName.empty())
        {
            // Create the task ShaderModule
            auto taskShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_taskName);
            if (taskShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Task))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_taskName);
                    success = false;
                    break;
                }
                taskShader.first->second = std::move(pShaderModule);
            }
            auto* pTaskShaderModule = apiCast<T_GFXAPI>(taskShader.first->second.get());

            // Create the mesh ShaderModule
            auto meshShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_meshName);
            if (meshShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Mesh))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_meshName);
                    success = false;
                    break;
                }
                meshShader.first->second = std::move(pShaderModule);
            }
            auto* pMeshShaderModule = apiCast<T_GFXAPI>(meshShader.first->second.get());

            // Create the fragment ShaderModule
            auto fragShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_fragmentName);
            if (fragShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Fragment))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_fragmentName);
                    success = false;
                    break;
                }
                fragShader.first->second = std::move(pShaderModule);
            }
            auto* pFragShaderModule = apiCast<T_GFXAPI>(fragShader.first->second.get());

            shaderModules.emplace(ShaderModules<T_GFXAPI>(GraphicsTaskMeshShaderModules<T_GFXAPI>{ *pTaskShaderModule, *pMeshShaderModule, *pFragShaderModule }));
        }
        else if (shaderPassDescription.m_vertexName.empty() && shaderPassDescription.m_taskName.empty() && !shaderPassDescription.m_meshName.empty() && !shaderPassDescription.m_fragmentName.empty())
        {
            // Create the mesh ShaderModule
            auto meshShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_meshName);
            if (meshShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Mesh))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_meshName);
                    success = false;
                    break;
                }
                meshShader.first->second = std::move(pShaderModule);
            }
            auto* pMeshShaderModule = apiCast<T_GFXAPI>(meshShader.first->second.get());

            // Create the fragment ShaderModule
            auto fragShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_fragmentName);
            if (fragShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Fragment))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_fragmentName);
                    success = false;
                    break;
                }
                fragShader.first->second = std::move(pShaderModule);
            }
            auto* pFragShaderModule = apiCast<T_GFXAPI>(fragShader.first->second.get());

            shaderModules.emplace(ShaderModules<T_GFXAPI>(GraphicsMeshShaderModules<T_GFXAPI>{ *pMeshShaderModule, *pFragShaderModule }));
        }
        else if (!shaderPassDescription.m_vertexName.empty())
        {
            // Create the vertex ShaderModule
            auto vertShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_vertexName);
            if (vertShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Vertex))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_vertexName);
                    success = false;
                    break;
                }
                vertShader.first->second = std::move(pShaderModule);
            }
            auto* pVertShaderModule = apiCast<T_GFXAPI>(vertShader.first->second.get());
            if (shaderPassDescription.m_fragmentName.empty())    // Dont have to have a fragment shader!
            {
                // Vertex only Module
                shaderModules.emplace(ShaderModules<T_GFXAPI>(GraphicsShaderModuleVertOnly<T_GFXAPI>{ *pVertShaderModule }));
            }
            else
            {
                // Create the fragment ShaderModule
                auto fragShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_fragmentName);
                if (fragShader.second == true)
                {
                    // Create the unique_ptr object (is not already loaded)
                    auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                    // Load the physical shader file
                    if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Fragment))
                    {
                        // Failed to load, remove the unloaded Shader class!
                        m_shaderModulesByName.erase(shaderPassDescription.m_fragmentName);
                        m_shaderModulesByName.erase(shaderPassDescription.m_vertexName);
                        success = false;
                        break;
                    }
                    fragShader.first->second = std::move(pShaderModule);
                }
                auto* pFragShaderModule = apiCast<T_GFXAPI>(fragShader.first->second.get());
                shaderModules.emplace(ShaderModules<T_GFXAPI>(GraphicsShaderModules<T_GFXAPI>{ *pVertShaderModule, *pFragShaderModule }));
            }
        }
        else if (!shaderPassDescription.m_computeName.empty())
        {
            // Create the compute ShaderModule
            auto computeShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_computeName);
            if (computeShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::Compute))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_computeName);
                    success = false;
                    break;
                }
                computeShader.first->second = std::move(pShaderModule);
            }
            auto* pComputeShaderModule = apiCast<T_GFXAPI>(computeShader.first->second.get());
            shaderModules.emplace(ShaderModules<T_GFXAPI>(ComputeShaderModule<T_GFXAPI>{*pComputeShaderModule}));
        }
        else if (!shaderPassDescription.m_rayGenerationName.empty())
        {
            // Create the ray generation ShaderModule
            auto rayGenerationShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_rayGenerationName);
            if (rayGenerationShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::RayGeneration))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_rayGenerationName);
                    success = false;
                    break;
                }
                rayGenerationShader.first->second = std::move(pShaderModule);
            }
            auto* pRayGenerationShaderModule = apiCast<T_GFXAPI>(rayGenerationShader.first->second.get());

            const ShaderModuleT<T_GFXAPI>* pRayClosestHitShaderModule = nullptr;
            const ShaderModuleT<T_GFXAPI>* pRayAnyHitShaderModule = nullptr;
            const ShaderModuleT<T_GFXAPI>* pRayMissShaderModule = nullptr;

            if (!shaderPassDescription.m_rayClosestHitName.empty())
            {
                auto rayClosestHitShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_rayClosestHitName);
                if (rayClosestHitShader.second == true)
                {
                    // Create the unique_ptr object (is not already loaded)
                    auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                    // Load the physical shader file
                    if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::RayClosestHit))
                    {
                        // Failed to load, remove the unloaded Shader class!
                        m_shaderModulesByName.erase(shaderPassDescription.m_rayClosestHitName);
                        success = false;
                        break;
                    }
                    rayClosestHitShader.first->second = std::move(pShaderModule);
                }
                pRayClosestHitShaderModule = apiCast <T_GFXAPI>(rayClosestHitShader.first->second.get());
            }
            if (!shaderPassDescription.m_rayAnyHitName.empty())
            {
                auto rayAnyHitShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_rayAnyHitName);
                if (rayAnyHitShader.second == true)
                {
                    // Create the unique_ptr object (is not already loaded)
                    auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                    // Load the physical shader file
                    if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::RayAnyHit))
                    {
                        // Failed to load, remove the unloaded Shader class!
                        m_shaderModulesByName.erase(shaderPassDescription.m_rayAnyHitName);
                        success = false;
                        break;
                    }
                    rayAnyHitShader.first->second = std::move(pShaderModule);
                }
                pRayAnyHitShaderModule = apiCast<T_GFXAPI>(rayAnyHitShader.first->second.get());
            }
            if (!shaderPassDescription.m_rayMissName.empty())
            {
                auto rayMissShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_rayMissName);
                if (rayMissShader.second == true)
                {
                    // Create the unique_ptr object (is not already loaded)
                    auto pShaderModule = std::make_unique<ShaderModuleT<T_GFXAPI>>();
                    // Load the physical shader file
                    if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModule::ShaderType::RayMiss))
                    {
                        // Failed to load, remove the unloaded Shader class!
                        m_shaderModulesByName.erase(shaderPassDescription.m_rayMissName);
                        success = false;
                        break;
                    }
                    rayMissShader.first->second = std::move(pShaderModule);
                }
                pRayMissShaderModule = apiCast<T_GFXAPI>(rayMissShader.first->second.get());
            }

            shaderModules.emplace(ShaderModules<T_GFXAPI>(RayTracingShaderModules<T_GFXAPI>{ *pRayGenerationShaderModule, pRayClosestHitShaderModule, pRayAnyHitShaderModule, pRayMissShaderModule }));
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
        for (const auto& set : shaderPassDescription.m_sets)
        {
            if (!descriptorSetLayouts.emplace_back().Init(rGfxApi, set))
            {
                // Error
                return false;
            }
        }

        //
        // Create the pipeline layout (for the ShaderPass)
        //
        PipelineLayout<T_GFXAPI> pipelineLayout;
        if (!pipelineLayout.Init(rGfxApi, descriptorSetLayouts))
        {
            // Error. Is ok (just means we didnt have a valid descriptor set layout yet)
        }

        PipelineVertexInputState<T_GFXAPI> pipelineVertexInputState{ *pShaderDescription, shaderPassDescription.m_vertexFormatBindings };

        SpecializationConstantsLayout<T_GFXAPI> specializationConstants{ shaderPassDescription.m_constants };

        shaderPasses.emplace_back(shaderPassDescription, std::move(shaderModules.value()), std::move(descriptorSetLayouts), std::move(pipelineLayout), std::move(pipelineVertexInputState), std::move(specializationConstants), (uint32_t)shaderPassDescription.m_outputs.size());
        shaderPassNames.push_back(passName);
    }

    if (!success)
    {
        // if we failed to load something then remove all knowledge of this shader
        m_shaderDescriptionsByName.erase(shaderName);
        return false;
    }

    // Create the Shader class
    auto shader = m_shadersByName.try_emplace(shaderName, std::make_unique<ShaderT<T_GFXAPI>>( pShaderDescription, std::move(shaderPasses), shaderPassNames ));
    if (!shader.second)
    {
        // Duplicate shader name
        return false;
    }

    return true;
}
