//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"

#ifdef GLM_FORCE_QUAT_DATA_WXYZ
#error "code currently expects glm::quat to be xyzw"
#endif

/// Camera data structure.
/// Contains basic camera data that can be loaded from json/model and used to position a @Camera
/// @ingroup Camera
struct CameraData
{
    glm::vec3 Position;
    glm::quat Orientation;
    int NodeId;                 //from gltf, used to lookup animations
};

