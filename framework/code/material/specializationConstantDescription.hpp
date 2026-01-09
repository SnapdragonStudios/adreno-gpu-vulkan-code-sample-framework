//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "vertexFormat.hpp"
#include <string>

/// Describes a single specialization constant.
/// Somewhat api agnostic.
/// @ingroup Material
class SpecializationConstantDescription
{
public:
    std::string                         name;
    uint32_t                            constantIndex;
    VertexFormat::Element::ElementType  type;
};
