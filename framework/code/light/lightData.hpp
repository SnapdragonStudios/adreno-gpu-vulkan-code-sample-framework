//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "system/glm_common.hpp"

/// @file lightData.hpp
/// @brief Header containing basic light data (shared with shader code)
/// @ingroup Light

struct LightDataBase
{
    glm::vec3   Color;
    float       Intensity;
    int         NodeId;     ///< gltf Node index (id) of this light (used for animations)
};

struct PointLightData : public LightDataBase
{
    glm::vec3   Position;
    float       Radius;     ///< Radius of light (not the range it is active over)
    float       Range;      ///< active range of the light (artificial range so light is not unbounded)
    const char* Name;       ///< name of pointlight (or nullptr if no name).  NOT OWNED by the PointLightData (lightlist or some other container should own the names and set this pointer accordingly).
};

struct SpotLightData : public LightDataBase
{
    glm::vec3   Position;
    float       Radius;             ///< Radius of light (not the range it is active over)
    float       Range;              ///< active range of the light (artificial range so light is not unbounded)
    float       CullSphereRadius;   ///< Radius of the culling sphere.  When culling is multiplied by the Direction in order to get the culling center (and then also used as the culling radius), for pointlights the Direction is a 0,0,0 vector and so the center does not move.
    glm::vec3   Direction;          ///< normalized direction vector.  MUST be 0,0,0 for pointlights
    float       SpotAngle;          ///< outer angle of spotlight cone (in radians, from center)
    const char* Name;               ///< name of spotlight (or nullptr if no name).  NOT OWNED by the SpotLightData (lightlist or some other container should own the names and set this pointer accordingly).
};

struct DirectionalLightData : public LightDataBase
{
    glm::vec3   Direction;  ///< normalized direction vector
    glm::vec3   Position;   ///< world position (may not be set or relevant for a directional but may provide shadow origin position (for example))
};

