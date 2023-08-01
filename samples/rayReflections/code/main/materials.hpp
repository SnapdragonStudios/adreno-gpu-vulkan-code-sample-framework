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
};

struct ObjectFragUB
{
    glm::vec4   Color;

    // X: Max Ray Bounces?
    // Y: Not Used
    // Z: Not Used
    // W: Not Used
    glm::vec4   MaterialProps;
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
// Texture Display
// **********************
struct TexDispVertUB
{
    glm::mat4   VPMatrix;
    glm::mat4   ModelMatrix;
    glm::vec4   Color;
};

// **********************
// Deferred Lighting Fullscreen pass
// **********************
struct LightFragCtrl 
{
    // Needed to convert from Depth to World Position
    glm::mat4 ProjectionInv;
    glm::mat4 ViewInv;

    // To apply a tint to the final results
    glm::vec4 ShaderTint;

    // Standard lighting
    glm::vec4 EyePos;
    glm::vec4 LightDir;
    glm::vec4 LightColor;

    // X: Mip Level
    // Y: Reflect Amount
    // Z: Reflect Fresnel Min
    // W: Reflect Fresnel Max
    glm::vec4 ReflectParams;

    // X: Mip Level
    // Y: IBL Amount
    // Z: Not Used
    // W: Not Used
    glm::vec4 IrradianceParams;

    // X: Ray Min Distance
    // Y: Ray Max Distance
    // Z: Max Ray Bounces
    // W: Ray Blend Amount (Mirror Amount)
    glm::vec4 RayParam01;

    // X: Shadow Amount
    // Y: Minumum Normal Dot Light
    // Z: Not Used
    // W: Not Used
    glm::vec4 LightParams;
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
// The Probe Ray Output
// **********************
struct ProbeRayOutUB
{
    // Rays are deterministic based on number of rays.
    // Each frame they are rotated by a random transform.
    glm::mat4   RayTransform;

    // Spacing of the probes in world coordinates
    glm::vec4   ProbeOrigin;
    glm::vec4   ProbeCounts;
    glm::vec4   ProbeSpacing;

    // X: Rays Per Probe
    // Y: Total Number of Probes
    // Z: Ray Min Distance
    // W: Ray Max Distance
    glm::vec4   RayParam01;

    // X: Irradiance Texels (Not Counting Border)
    // Y: Distance Texels (Not Counting Border)
    // Z: Not Used
    // W: Not Used
    glm::vec4   RayParam02;

    // X: Gamma Encode Power
    // Y: Gamma Inverse Power
    // Z: Shadow Amount
    // W: Not Used
    glm::vec4   RayParam03;

    // X: Blend Lerp
    // Y: Max Probe Change
    // Z: Max Brightness
    // W: Distance Exponent
    glm::vec4   RayParam04;

    // Environment Settings
    glm::vec4   LightDir;
    glm::vec4   LightColor;
};

// **********************
// The Scene Data
// **********************
struct SceneData
{
    // WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! 
    // mat4 MUST be on 16 byte boundary or the validation layer complains
    // Therefore, can't start with an "int" value in this structure without padding

    // The transform for this instance
    glm::mat4       Transform;

    // The transform used for normals (Inverse Transpose)
    // See http://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Applying_Transformations.html#Normals
    glm::mat4       TransformIT;

    // The index into the list of meshes.
    // This could really be put in VkAccelerationStructureInstanceKHR.instanceCustomIndex.
    // For now, the custom index will point to the array of ScenData structures and
    // then this will point to meshes.
    // Rays are deterministic based on number of rays.
    // Each frame they are rotated by a random transform.
    uint32_t        MeshIndex;


    // Material information
    int32_t         DiffuseTex;     // -1 means no entry
    int32_t         NormalTex;      // -1 means no entry

    int32_t         Padding;        // So lines up on 16

};

struct SceneDataInternal
{
    // The index into the list of meshes.
    // This could really be put in VkAccelerationStructureInstanceKHR.instanceCustomIndex.
    // For now, the custom index will point to the array of ScenData structures and
    // then this will point to meshes.
    // Rays are deterministic based on number of rays.
    // Each frame they are rotated by a random transform.
    uint32_t        MeshIndex;

    // The transform for this instance
    glm::mat4       Transform;

    // The transform used for normals (Inverse Transpose)
    // See http://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Applying_Transformations.html#Normals
    glm::mat4       TransformIT;

    // Material information
    std::string     DiffuseTex;
    std::string     NormalTex;
};


