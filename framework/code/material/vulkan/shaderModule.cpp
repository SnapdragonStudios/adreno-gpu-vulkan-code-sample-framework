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
#include "vertexDescription.hpp"
#include "vulkan/vulkan.hpp"
#include "system/assetManager.hpp"


ShaderModule<Vulkan>::ShaderModule() noexcept : ShaderModuleBase()
    , m_shader(VK_NULL_HANDLE)
{
}

/// Destructor.  Will assert if a shader was loaded and the matching Destroy was not called.
ShaderModule<Vulkan>::~ShaderModule()
{
    assert(m_shader == VK_NULL_HANDLE);
}

void ShaderModule<Vulkan>::Destroy(Vulkan& vulkan)
{
    if (m_shader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(vulkan.m_VulkanDevice, m_shader, nullptr);
        m_shader = VK_NULL_HANDLE;
    }
    ShaderModuleBase::Destroy();
}

bool ShaderModule<Vulkan>::Load(Vulkan& vulkan, AssetManager& assetManager, const std::string& filename)
{
    bool success = true;
    Destroy(vulkan);

    m_filename = filename;

    if (!m_filename.empty())
    {
        std::vector<char> data;
        if ( assetManager.LoadFileIntoMemory(m_filename.c_str(), data) )
        {
            VkShaderModuleCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = data.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(data.data());
            VkShaderModule shaderModule;
            if (vkCreateShaderModule(vulkan.m_VulkanDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create shader module!");
                success = false;
            }
            else
            {
                m_shader = shaderModule;
            }
            vulkan.SetDebugObjectName(shaderModule, m_filename.c_str());

        }
        else
        {
            success = false;
        }
    }
    return success;
}

bool ShaderModule<Vulkan>::Load(Vulkan& vulkan, AssetManager& assetManager, const ShaderPassDescription& shaderDescription, const ShaderType shaderType)
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
    case ShaderType::Mesh:
        shaderFileName = &shaderDescription.m_meshName;
        break;
    case ShaderType::Task:
        shaderFileName = &shaderDescription.m_taskName;
        break;
    }

    assert(shaderFileName!=nullptr);
    return Load(vulkan, assetManager, *shaderFileName);    ///TODO: better return code / error handling
}
