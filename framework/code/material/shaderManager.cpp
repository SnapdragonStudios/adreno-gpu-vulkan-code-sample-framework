//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shaderManager.hpp"
#include "shaderDescription.hpp"
#include "shaderModule.hpp"
#include "shader.hpp"
#include "system/os_common.h"


ShaderManagerBase::ShaderManagerBase(GraphicsApiBase& graphicsApi)
    : m_GraphicsApi(graphicsApi)
{}

ShaderManagerBase::~ShaderManagerBase()
{
    assert(m_shadersByName.empty());    // Expected that the derived (playform specific) class destroys the shaders
    assert(m_shaderModulesByName.empty());
}

void ShaderManagerBase::RegisterRenderPassNames(const std::span<const char*const> passNames)
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

bool ShaderManagerBase::AddShader(
    AssetManager& assetManager, 
    const std::string& shaderName, 
    const std::string& filename,
    const std::filesystem::path& source_dir)
{
    auto shaderDescription = ShaderDescriptionLoader::Load(assetManager, source_dir.empty() ? filename : (source_dir / filename).string());
    if (!shaderDescription)
    {
        return false;
    }
    return AddShader(assetManager, shaderName, std::move(*shaderDescription));
}

bool ShaderManagerBase::AddShader(
    AssetManager& assetManager, 
    const std::string& shaderName, 
    const std::filesystem::path& filename,
    const std::filesystem::path& source_dir)
{
    return AddShader(assetManager, shaderName, filename.string(), source_dir);
}

//const ShaderDescription* ShaderManagerBase::GetShaderDescription(const std::string& shaderName) const
//{
//    auto it = m_shaderDescriptionsByName.find(shaderName);
//    if (it != m_shaderDescriptionsByName.end())
//        return &(it->second);
//    return nullptr;
//}

const ShaderBase* ShaderManagerBase::GetShader(const std::string& shaderName) const
{
    auto it = m_shadersByName.find(shaderName);
    if (it != m_shadersByName.end())
        return (it->second).get();
    return nullptr;
}
