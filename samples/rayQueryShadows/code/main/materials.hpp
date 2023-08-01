//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "vulkan/vulkan.hpp"

#include "system/os_common.h"
#include "system/glm_common.hpp"

//=============================================================================
// Uniform Buffers
//=============================================================================


// **********************
// The Objects
// **********************
struct ObjectVertUB
{
    glm::mat4   VPMatrix;
    glm::vec4   AnimationRotation;      // Used if running the 'animated' version of the shaders
};

struct ObjectFragUB
{
    glm::vec4   Color;

    // X: Normal Height
    // Y: Normal Mirror Reflect Amount
    // Z: Reflect Fresnel Min
    // W: Reflect Fresnel Max
    glm::vec4   NormalHeight;
};


// **********************
// The Skybox
// **********************
struct SkyboxVertUB
{
    glm::mat4   MVPMatrix;
    glm::mat4   ModelMatrix;
    glm::vec4   Color;
};


// **********************
// Deferred Lighting Fullscreen pass
// **********************
struct LightFragCtrl {
    glm::mat4 ProjectionInv;
    glm::mat4 ViewInv;
    glm::mat4 WorldToShadow;// for rasterized depth shadow
    glm::vec4 CameraPos;

    glm::vec4 PointLightPosition;  // w is intensity
    glm::vec4 PointLightColor;

    // Directional Light
    glm::vec4 DirectionalLightDirection;
    glm::vec4 DirectionalLightColor;

    glm::vec4 AmbientColor;

    float PointLightRadius; // of the light itself (not the area of influence)
    float PointLightCutoff;

    float SpecScale;
    float SpecPower;
    float irradianceAmount;
    float irradianceMipLevel;
    float AmbientOcclusionScale;
    int Width;
    int Height;
};


// **********************
// Additional deferred pass light objects (additional lights in scene)
// **********************
struct PointLightUB{
    // Vertex shader
    glm::mat4   MVPMatrix;

    // Fragment shader (could split in to 2 uniform buffers, one per shader stage)
    glm::mat4   ProjectionInv;
    glm::mat4   ViewInv;
    glm::vec4   CameraPos;
    glm::vec3   LightPosition;
    float       LightIntensity;
    glm::vec4   LightColor;
    float       LightRadius;    // Radius of the physical light (not its range of influence)
    float       LightCutoff;    // Cutoff brightness for the light
    float       SpecScale;
    float       SpecPower;
    glm::vec2   WindowSize;
};


// **********************
// Post/Blit
// **********************
struct BlitFragCtrl {
    float Bloom = 0.0f;
    float Diffuse = 1.0f;   // 0 to 2 range (dark to white)
    int sRGB = 0;           // 1 - apply srgb conversion in output blit shader, 0 passthrough color
};


// **********************
// Compute (ray traced shadow)
// **********************
struct ShadowRQCtrl {
    // X: Screen Width
    // Y: Screen Height
    // Z: One Width Pixel
    // W: One Height Pixel
    glm::vec4   ScreenSize;
    // Camera inverse projection
    glm::mat4   ProjectionInv;
    // Camera inverse view
    glm::mat4   ViewInv;
    // Light(shadow) world position (only used for spotlight or pointlight)
    glm::vec4   LightWorldPos;
    // Light(shadow) world direction (only used for spotlight or directional light)
    glm::vec4   LightWorldDirection;
} m_ShadowRQCtrl;

// **********************
// Compute (mesh animation)
// **********************
struct MeshAnimatorUB
{
    glm::vec4 AnimationRotation;
};
