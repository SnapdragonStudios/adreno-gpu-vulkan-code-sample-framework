//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "material.hpp"
#include "materialPass.hpp"


// Forward declarations
template<typename T_GFXAPI> class Shader;
template<typename T_GFXAPI> class Texture;


// Material (MaterialBase templated by the graphics api)
template<class T_GFXAPI>
class Material : public MaterialBase
{
    Material (const Material<T_GFXAPI>&) = delete;
    Material<T_GFXAPI>& operator=(const Material<T_GFXAPI>&) = delete;
public:
    Material(const Shader<T_GFXAPI>& shader, uint32_t numFramebuffers);
    Material(Material<T_GFXAPI>&&) noexcept;
    ~Material();

    const Shader<T_GFXAPI>& GetShader() const { return apiCast<T_GFXAPI>(m_shader); }

    void AddMaterialPass(const std::string& passName, MaterialPass<T_GFXAPI>&& pass)
    {
        if (m_materialPassNamesToIndex.try_emplace(passName, (uint32_t)m_materialPasses.size()).second == true)
        {
            m_materialPasses.emplace_back(std::move(pass));
        }
        // else pass name already exists - do nothing!
    }

    const MaterialPass<T_GFXAPI>* GetMaterialPass(const std::string& passName) const;
    const MaterialPass<T_GFXAPI>* GetMaterialPass(uint32_t passIdx) const;
    const auto& GetMaterialPasses() const { return m_materialPasses; }

    bool UpdateDescriptorSets(uint32_t bufferIdx);

    /// @brief Update a single value in a descriptor set
    /// Not optimized for being called multiple times per set (per frame).  Intended to be used sparingly.
    /// @param bufferIdx 
    /// @return true on success
    bool UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const Texture<T_GFXAPI>& newTexture) const;

protected:
    std::vector<MaterialPass<T_GFXAPI>> m_materialPasses;
};


template<class T_GFXAPI>
Material<T_GFXAPI>::Material(Material<T_GFXAPI> && other) noexcept
: MaterialBase( std::move(other) )
, m_materialPasses(std::move(other.m_materialPasses))
{
}

template<class T_GFXAPI>
Material<T_GFXAPI>::~Material()
{
}

template<class T_GFXAPI>
Material<T_GFXAPI>::Material(const Shader<T_GFXAPI>& shader, uint32_t numFramebuffers)
    :MaterialBase(shader, numFramebuffers)
{
}


template<class T_GFXAPI>
const MaterialPass<T_GFXAPI>* Material<T_GFXAPI>::GetMaterialPass(const std::string& passName) const
{
    auto it = m_materialPassNamesToIndex.find(passName);
    if (it != m_materialPassNamesToIndex.end())
    {
        return &m_materialPasses[it->second];
    }
    return nullptr;
}

template<class T_GFXAPI>
const MaterialPass<T_GFXAPI>* Material<T_GFXAPI>::GetMaterialPass(uint32_t passIdx) const
{
    return &m_materialPasses[passIdx];
}

template<class T_GFXAPI>
bool Material<T_GFXAPI>::UpdateDescriptorSets(uint32_t bufferIdx)
{
    // Just go through and update the MaterialPass descriptor sets.
    // Could be much smarter (and could handle failures better than just continuing and reporting something went wrong)
    bool success = true;
    for (auto& materialPass : m_materialPasses)
    {
        success &= materialPass.UpdateDescriptorSets(bufferIdx);
    }
    return success;
}

template<class T_GFXAPI>
bool Material<T_GFXAPI>::UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const Texture<T_GFXAPI>& newTexture) const
{
    bool success = true;
    for (auto& materialPass : m_materialPasses)
    {
        success &= materialPass.UpdateDescriptorSetBinding(bufferIdx, bindingName, newTexture);
    }
    return success;
}
