//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <optional>
#include <string>
#include "shaderManager.hpp"
#include "shader.hpp"
#include "shaderDescription.hpp"
#include "shaderModule.hpp"

/// Tempated (by graphics api) ShaderManager.
/// @ingroup Material
template<typename T_GFXAPI>
class ShaderManager : public ShaderManagerBase
{
    ShaderManager() = delete;
    ShaderManager& operator=(const ShaderManagerBase&) = delete;
    ShaderManager(const ShaderManager<T_GFXAPI>&) = delete;
public:
    ShaderManager(T_GFXAPI& gpxApi);
    ~ShaderManager();

    // Get ShaderBase for the given shader name
    /// @returns nullptr if shaderName unknown
    const Shader<T_GFXAPI>* GetShader(const std::string& shaderName) const;

    using ShaderManagerBase::AddShader; // bring into name resolution scope
protected:
    /// @brief Load all referenced shaders modules from the given description for all passes. Makes shader ready for material to be created from it.
    /// @param shaderName name to be given to this shader for lookup within the application code (user determined name).
    /// @param shaderDescription description of this shader
    /// @return true if everything loaded correctly.
    bool AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription) override;

    /// @brief Load all referenced shaders modules from the given description (called by AddShader)
    std::optional <ShaderModules<T_GFXAPI>> LoadShaderModules(AssetManager& assetManager, const ShaderPassDescription& shaderPassDescription);

    T_GFXAPI& GetGraphicsApi() const { return static_cast<T_GFXAPI&>(m_GraphicsApi); }
};



template<typename T_GFXAPI>
ShaderManager<T_GFXAPI>::ShaderManager(T_GFXAPI& gfxApi) : ShaderManagerBase(gfxApi)
{}

template<typename T_GFXAPI>
ShaderManager<T_GFXAPI>::~ShaderManager()
{
    // Destroy the Shaders (which destroys the owned ShaderPasses etc)
    auto& rVulkan = static_cast<T_GFXAPI&>(m_GraphicsApi);
    for (auto& s : m_shadersByName)
        s.second->Destroy(rVulkan);
    m_shadersByName.clear();

    // ShaderModules (do after Shaders, as the ShaderBase(Pass) points to the ShaderModule)
    // If you get an error here then maybe you haven't included the 'ShaderModule.hpp' for your graphics API !
    for (auto& sm : m_shaderModulesByName)
        apiCast<T_GFXAPI>(sm.second.get())->Destroy(rVulkan);
    m_shaderModulesByName.clear();
}

template<typename T_GFXAPI>
bool ShaderManager<T_GFXAPI>::AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription)
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

    T_GFXAPI& rGfxApi = GetGraphicsApi();

    for (const auto& passIt : orderedPassNames)
    {
        const std::string passName = passIt.first;
        assert(!passName.empty());
        const ShaderPassDescription& shaderPassDescription = passDescriptionsPlaced.first->second.m_descriptionPerPass[passIt.second];    // once created the vector should not (MUST NOT) change size (reference will be stored by other classes)

        // Lookup pass name and get pass index (if name has not been registered skip loading/registering this shader pass)
        auto passIdxIt = m_shaderPassIndexByName.find(passName);
        if (passIdxIt == m_shaderPassIndexByName.end())
        {
            //LOGI("ShaderManagerBase::AddShader - pass name \"%s\" not registered with RegisterRenderPassNames", passName.c_str());
            // This may not be an error (if we are manually grabbing or iterating passes from within a shader/material).
        }

        auto shaderModules = LoadShaderModules(assetManager, shaderPassDescription);
        if (!shaderModules)
        {
            return false;
        }

        //
        // Create the descriptor set layout (for the ShaderPass)
        //
        std::vector<DescriptorSetLayout<T_GFXAPI>> descriptorSetLayouts;

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
        if (!pipelineLayout.Init(rGfxApi, descriptorSetLayouts, shaderPassDescription.m_rootSamplers))
        {
            // Error. Is ok (just means we didnt have a valid descriptor set layout yet)
        }

        PipelineVertexInputState<T_GFXAPI> pipelineVertexInputState{ *pShaderDescription, shaderPassDescription.m_vertexFormatBindings };

        SpecializationConstantsLayout<T_GFXAPI> specializationConstants{ shaderPassDescription.m_constants };

        //
        // Add the ShaderPass to the ShaderBase
        //

        shaderPasses.emplace_back(shaderPassDescription, std::move(shaderModules.value()), std::move(descriptorSetLayouts), std::move(pipelineLayout), std::move(pipelineVertexInputState), std::move(specializationConstants), (uint32_t)shaderPassDescription.m_outputs.size());
        shaderPassNames.push_back(passName);
    }

    if (!success)
    {
        // if we failed to load something then remove all knowledge of this shader
        m_shaderDescriptionsByName.erase(shaderName);
        return false;
    }

    // Create the ShaderBase class
    auto shader = m_shadersByName.try_emplace(shaderName, std::make_unique<Shader<T_GFXAPI>>( pShaderDescription, std::move(shaderPasses), shaderPassNames ));
    if (!shader.second)
    {
        // Duplicate shader name
        return false;
    }

    return true;
}

template<typename T_GFXAPI>
std::optional<ShaderModules<T_GFXAPI>> ShaderManager<T_GFXAPI>::LoadShaderModules(AssetManager& assetManager, const ShaderPassDescription& shaderPassDescription)
{
    // TEMPORARY FIX FOR RECENT MEDIA FOLDER CHANGES //

    if (shaderPassDescription.m_fragmentName.starts_with("Media/Shaders/"))
    {
        LOGW("Project uses old 'Media/Shaders/' path its shaders (fragment), the new one is 'build/Media/Shaders', please update your shader json accordingly, temporary fix will be applied");
        const_cast<ShaderPassDescription&>(shaderPassDescription).m_fragmentName = std::string("build/Media/Shaders/") + shaderPassDescription.m_fragmentName.substr(std::string("Media/Shaders/").size());
    }

    if (shaderPassDescription.m_vertexName.starts_with("Media/Shaders/"))
    {
        LOGW("Project uses old 'Media/Shaders/' path its shaders (vertex), the new one is 'build/Media/Shaders', please update your shader json accordingly, temporary fix will be applied");
        const_cast<ShaderPassDescription&>(shaderPassDescription).m_vertexName = std::string("build/Media/Shaders/") + shaderPassDescription.m_vertexName.substr(std::string("Media/Shaders/").size());
    }

    if (shaderPassDescription.m_computeName.starts_with("Media/Shaders/"))
    {
        LOGW("Project uses old 'Media/Shaders/' path its shaders (compute), the new one is 'build/Media/Shaders', please update your shader json accordingly, temporary fix will be applied");
        const_cast<ShaderPassDescription&>(shaderPassDescription).m_computeName = std::string("build/Media/Shaders/") + shaderPassDescription.m_computeName.substr(std::string("Media/Shaders/").size());
    }

    ///////////////////////////////////////////////////

    std::optional<ShaderModules<T_GFXAPI>> shaderModules;
    auto& rGfxApi = GetGraphicsApi();

    if (!shaderPassDescription.m_meshName.empty())
    {
        // Create the mesh ShaderModule
        auto meshShader = m_shaderModulesByName.try_emplace( shaderPassDescription.m_meshName );
        if (meshShader.second == true)
        {
            // Create the unique_ptr object (is not already loaded)
            auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
            // Load the physical shader file
            if (!pShaderModule->Load( rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::Mesh ))
            {
                // Failed to load, remove the unloaded Shader class!
                m_shaderModulesByName.erase( shaderPassDescription.m_meshName );
                return {};
            }
            meshShader.first->second = std::move( pShaderModule );
        }
        auto* pMeshShaderModule = apiCast<T_GFXAPI>( meshShader.first->second.get() );

        // Create the fragment ShaderModule
        if (shaderPassDescription.m_fragmentName.empty())
        {
            assert( shaderPassDescription.m_fragmentName.empty() );
            return {};
        }
        auto fragShader = m_shaderModulesByName.try_emplace( shaderPassDescription.m_fragmentName );
        if (fragShader.second == true)
        {
            // Create the unique_ptr object (is not already loaded)
            auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
            // Load the physical shader file
            if (!pShaderModule->Load( rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::Fragment ))
            {
                // Failed to load, remove the unloaded Shader class!
                m_shaderModulesByName.erase( shaderPassDescription.m_fragmentName );
                return {};
            }
            fragShader.first->second = std::move( pShaderModule );
        }
        auto* pFragShaderModule = apiCast<T_GFXAPI>( fragShader.first->second.get() );

        // Create the (optional) task shader module
        if (!shaderPassDescription.m_taskName.empty())
        {
            auto taskShader = m_shaderModulesByName.try_emplace( shaderPassDescription.m_taskName );
            if (taskShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load( rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::Task ))
                {
                    // Failed to load, remove the unloaded Shader class!
                    m_shaderModulesByName.erase( shaderPassDescription.m_taskName );
                    return {};
                }
                taskShader.first->second = std::move( pShaderModule );
            }
            auto* pTaskShaderModule = apiCast<T_GFXAPI>( taskShader.first->second.get() );
            shaderModules.emplace( ShaderModules<T_GFXAPI>( GraphicsTaskMeshShaderModules<T_GFXAPI>{ *pTaskShaderModule, * pMeshShaderModule, * pFragShaderModule } ) );
        }
        else
        {
            shaderModules.emplace( ShaderModules<T_GFXAPI>( GraphicsMeshShaderModules<T_GFXAPI>{ * pMeshShaderModule, * pFragShaderModule } ) );
        }
    }
    else if (!shaderPassDescription.m_taskName.empty())
    {
        assert( 0 && "task shader without a mesh shader");
        return {};
    }
    else if (!shaderPassDescription.m_vertexName.empty())
    {
        // Create the vertex ShaderModule
        auto vertShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_vertexName);
        if (vertShader.second == true)
        {
            // Create the unique_ptr object (is not already loaded)
            auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
            // Load the physical shader file
            if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::Vertex))
            {
                // Failed to load, remove the unloaded ShaderBase class!
                m_shaderModulesByName.erase(shaderPassDescription.m_vertexName);
                return {};
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
                auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::Fragment))
                {
                    // Failed to load, remove the unloaded ShaderBase class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_fragmentName);
                    m_shaderModulesByName.erase(shaderPassDescription.m_vertexName);
                    return {};
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
            auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
            // Load the physical shader file
            if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::Compute))
            {
                // Failed to load, remove the unloaded ShaderBase class!
                m_shaderModulesByName.erase(shaderPassDescription.m_computeName);
                return {};
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
            auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
            // Load the physical shader file
            if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::RayGeneration))
            {
                // Failed to load, remove the unloaded ShaderBase class!
                m_shaderModulesByName.erase(shaderPassDescription.m_rayGenerationName);
                return {};
            }
            rayGenerationShader.first->second = std::move(pShaderModule);
        }
        auto* pRayGenerationShaderModule = apiCast<T_GFXAPI>(rayGenerationShader.first->second.get());

        const ShaderModule<T_GFXAPI>* pRayClosestHitShaderModule = nullptr;
        const ShaderModule<T_GFXAPI>* pRayAnyHitShaderModule = nullptr;
        const ShaderModule<T_GFXAPI>* pRayMissShaderModule = nullptr;

        if (!shaderPassDescription.m_rayClosestHitName.empty())
        {
            auto rayClosestHitShader = m_shaderModulesByName.try_emplace(shaderPassDescription.m_rayClosestHitName);
            if (rayClosestHitShader.second == true)
            {
                // Create the unique_ptr object (is not already loaded)
                auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::RayClosestHit))
                {
                    // Failed to load, remove the unloaded ShaderBase class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_rayClosestHitName);
                    return {};
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
                auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::RayAnyHit))
                {
                    // Failed to load, remove the unloaded ShaderBase class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_rayAnyHitName);
                    return {};
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
                auto pShaderModule = std::make_unique<ShaderModule<T_GFXAPI>>();
                // Load the physical shader file
                if (!pShaderModule->Load(rGfxApi, assetManager, shaderPassDescription, ShaderModuleBase::ShaderType::RayMiss))
                {
                    // Failed to load, remove the unloaded ShaderBase class!
                    m_shaderModulesByName.erase(shaderPassDescription.m_rayMissName);
                    return {};
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
    }
    return shaderModules;
}


template<typename T_GFXAPI>
const Shader<T_GFXAPI>* ShaderManager<T_GFXAPI>::GetShader(const std::string& shaderName) const
{
    return apiCast<T_GFXAPI>(ShaderManagerBase::GetShader(shaderName));
}
