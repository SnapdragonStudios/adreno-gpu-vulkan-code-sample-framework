//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <string>


// Forward declarations
template<typename T_GFXAPI> class Material;
class ShaderBase;


/// An instance of a Shader material.
/// Container for MaterialPasses and reference to this material's Shader
/// @ingroup Material
class MaterialBase
{
    MaterialBase(const MaterialBase&) = delete;
    MaterialBase& operator=(const MaterialBase&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = Material<T_GFXAPI>; // make apiCast work!
    MaterialBase(const ShaderBase& shader, uint32_t numFramebuffers);
    MaterialBase(MaterialBase&&) noexcept;
    ~MaterialBase();

    uint32_t GetNumFrameBuffers() const { return m_numFramebuffers; }

    const ShaderBase& m_shader;
    const ShaderBase& GetShader() const { return m_shader; }

protected:
    std::map<std::string, uint32_t> m_materialPassNamesToIndex; /// pass name to index in m_materialPasses
    const uint32_t m_numFramebuffers;                           /// more accurately 'number of frames of buffers'.    
};

