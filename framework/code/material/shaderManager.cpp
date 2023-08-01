//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shaderManager.hpp"
#include "shaderDescription.hpp"
#include "shaderModule.hpp"
#include "descriptorSetLayout.hpp"
#include "shader.hpp"
#include "vulkan/specializationConstantsLayout.hpp"
#include "vulkan/shader.hpp"
#include "vulkan/shaderModule.hpp"
#include "vulkan/pipelineLayout.hpp"
#include "vulkan/vulkan.hpp"
#include "system/os_common.h"
#include <memory>


ShaderManager::ShaderManager(GraphicsApiBase& graphicsApi)
    : m_GraphicsApi(graphicsApi)
{}

ShaderManager::~ShaderManager()
{
    assert(m_shadersByName.empty());    // Expected that the derived (playform specific) class destroys the shaders
    assert(m_shaderModulesByName.empty());
}

void ShaderManager::RegisterRenderPassNames(const std::span<const char*const> passNames)
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

bool ShaderManager::AddShader(AssetManager& assetManager, const std::string& shaderName, const std::string& filename)
{
    auto shaderDescription = ShaderDescriptionLoader::Load(assetManager, filename);
    if (!shaderDescription)
    {
        return false;
    }
    return AddShader(assetManager, shaderName, std::move(*shaderDescription));
}

//const ShaderDescription* ShaderManager::GetShaderDescription(const std::string& shaderName) const
//{
//    auto it = m_shaderDescriptionsByName.find(shaderName);
//    if (it != m_shaderDescriptionsByName.end())
//        return &(it->second);
//    return nullptr;
//}

const Shader* ShaderManager::GetShader(const std::string& shaderName) const
{
    auto it = m_shadersByName.find(shaderName);
    if (it != m_shadersByName.end())
        return (it->second).get();
    return nullptr;
}
