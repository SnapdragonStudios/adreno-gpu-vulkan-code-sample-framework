//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Uniform buffer locations
#define SHADER_VERT_UBO_LOCATION            0

// Texture Locations
#define SHADER_ENVIRONMENT_TEXTURE_LOC      1

// Textures
layout(set = 0, binding = SHADER_ENVIRONMENT_TEXTURE_LOC) uniform samplerCube       u_EnvironmentTex;

// Varying's
layout (location = 0) in vec2   v_TexCoord;
layout (location = 1) in vec3   v_WorldPos;
layout (location = 2) in vec3   v_WorldNorm;
layout (location = 6) in vec4   v_VertColor;

// Finally, the output color
layout (location = 0) out vec4 FragColor;

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    // ********************************
    // Diffuse Color
    // ********************************
    // Get base color from the color texture
    vec4 DiffuseColor = v_VertColor.xyzw;

    // ********************************
    // Skybox Color
    // ********************************
    vec3 CubeUV = normalize(v_WorldNorm);
    vec3 ReflectMapColor = textureLod(u_EnvironmentTex, CubeUV, 0).rgb;

    // ********************************
    // Final Color
    // ********************************
    FragColor = vec4(DiffuseColor.xyz * ReflectMapColor.rgb, DiffuseColor.w);
}

