//==============================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//==============================================================================================

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

// Uniform buffer locations
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Texture Locations
#define SHADER_DIFFUSE_TEXTURE_LOC          2
#define SHADER_NORMAL_TEXTURE_LOC           3
#define SHADER_ENVIRONMENT_TEXTURE_LOC      4
#define SHADER_IRRADIANCE_TEXTURE_LOC       5
#define SHADER_REFLECT_TEXTURE_LOC          6

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff 
{
float tmp;
} FragCB;

// Textures
layout(set = 0, binding = SHADER_DIFFUSE_TEXTURE_LOC) uniform sampler2D             u_DiffuseTex;

// Varying's
layout (location = 0) in vec2   v_TexCoord;
layout (location = 1) in vec3   v_WorldPos;
layout (location = 2) in vec3   v_WorldNorm;
layout (location = 3) in vec3   v_WorldTan;
layout (location = 4) in vec3   v_WorldBitan;
layout (location = 5) in vec4   v_VertColor;

// Finally, the output color
layout (location = 0) out vec4  FragColor;


//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    // ********************************
    // Base Lighting
    // ********************************
    // Get base color from the color texture
    vec4 DiffuseColor = texture( u_DiffuseTex, v_TexCoord.xy );

    // Adjust by vertex color.
    DiffuseColor.xyzw *= v_VertColor.xyzw;

    // Output
    FragColor = vec4(DiffuseColor.xyz, 1.0);
}

