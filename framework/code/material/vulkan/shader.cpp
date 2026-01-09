//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shader.hpp"
#include "shaderModule.hpp"
#include "material/descriptorSetLayout.hpp"
#include "pipelineLayout.hpp"
#include <cassert>

ShaderPassBase::ShaderPassBase( ShaderPassBase&& other ) noexcept
    : m_shaderPassDescription( other.m_shaderPassDescription )
{
    assert( 0 );    // should not use this function
}
