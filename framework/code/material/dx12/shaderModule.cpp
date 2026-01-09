//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shaderModule.hpp"
#include <vector>
#include "material/shaderDescription.hpp"
//#include "material/vertexDescription.hpp"
#include "system/assetManager.hpp"

class Dx12;

ShaderModule<Dx12>::ShaderModule() noexcept : ShaderModuleBase()
    , m_Shader{}
{
}

ShaderModule<Dx12>::~ShaderModule()
{
    assert(m_Shader.empty());        // expects that Destroy() called before destruction
}

void ShaderModule<Dx12>::Destroy(Dx12& dx12)
{
    m_Shader = {};
    ShaderModuleBase::Destroy();
}

bool ShaderModule<Dx12>::Load(Dx12& dx12, AssetManager& assetManager, const std::string& filename)
{
    bool success = true;
    Destroy(dx12);

    m_filename = filename;

    if (!m_filename.empty())
    {
        if ( assetManager.LoadFileIntoMemory(m_filename.c_str(), m_Shader) )
        {
        }
        else
        {
            success = false;
        }
    }
    return success;
}

bool ShaderModule<Dx12>::Load(Dx12& dx12, AssetManager& assetManager, const ShaderPassDescription& shaderDescription, const ShaderType shaderType)
{
    const std::string* shaderFileName = nullptr;
    switch (shaderType)
    {
    case ShaderType::Fragment:
        shaderFileName = &shaderDescription.m_fragmentName;
        break;
    case ShaderType::Vertex:
        shaderFileName = &shaderDescription.m_vertexName;
        break;
    case ShaderType::Compute:
        shaderFileName = &shaderDescription.m_computeName;
        break;
    case ShaderType::RayGeneration:
        shaderFileName = &shaderDescription.m_rayGenerationName;
        break;
    case ShaderType::RayClosestHit:
        shaderFileName = &shaderDescription.m_rayClosestHitName;
        break;
    case ShaderType::RayAnyHit:
        shaderFileName = &shaderDescription.m_rayAnyHitName;
        break;
    case ShaderType::RayMiss:
        shaderFileName = &shaderDescription.m_rayMissName;
        break;
    }

    assert(shaderFileName!=nullptr);
    return Load(dx12, assetManager, *shaderFileName);    ///TODO: better return code / error handling
}
