//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

//
/// @file math_common.hpp
/// @ingroup System
// Common math defines and includes
// Vector math is included with glm_common.hpp
//

#include <cmath>

// Constants
constexpr float PI = 3.1415926535f;
constexpr float PI_MUL_2 = 6.2831853072f;
constexpr float PI_DIV_2 = 1.5707963268f;
constexpr float PI_DIV_3 = 1.0471975512f;
constexpr float PI_DIV_4 = 0.7853981634f;
constexpr float PI_DIV_6 = 0.5235987756f;
constexpr float TO_RADIANS = 0.0174532925f;
constexpr float TO_DEGREES = 57.2957795131f;

// Helper Macro
#define SET_VEC4(p, x, y, z, w)     {p[0] = x; p[1] = y; p[2] = z; p[3] = w; }
