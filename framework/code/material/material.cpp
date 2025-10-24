//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "material.hpp"
#include <cassert>

//
// MaterialBase base class implementation
//

MaterialBase::MaterialBase(const ShaderBase& shader, uint32_t numFramebuffers)
: m_shader(shader)
, m_numFramebuffers(numFramebuffers)
{
    assert(m_numFramebuffers > 0);
}

MaterialBase::MaterialBase(MaterialBase&& other) noexcept
    : m_shader(other.m_shader)
    , m_materialPassNamesToIndex(std::move(other.m_materialPassNamesToIndex))
    , m_numFramebuffers(other.m_numFramebuffers)
{
}

MaterialBase::~MaterialBase()
{}
