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
#extension GL_GOOGLE_include_directive : enable

// Define available features

// Uniform buffer locations
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Texture Locations
#define SHADER_DIFFUSE_TEXTURE_LOC          2
#define SHADER_NORMAL_TEXTURE_LOC           3

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff 
{
    vec4    Color;

    // X: Max Ray Bounces?
    // Y: Not Used
    // Z: Not Used
    // W: Not Used
    vec4    MaterialProps;

} FragCB;

#define NORMAL_HEIGHT           FragCB.NormalHeight.x
#define NORMAL_MIRROR_AMOUNT    FragCB.NormalHeight.y

// Textures
layout(set = 0, binding = SHADER_DIFFUSE_TEXTURE_LOC) uniform sampler2D             u_DiffuseTex;
layout(set = 0, binding = SHADER_NORMAL_TEXTURE_LOC) uniform sampler2D              u_NormalTex;

// Varying's
layout (location = 0) in vec2   v_TexCoord;
layout (location = 1) in vec3   v_WorldPos;
layout (location = 2) in vec3   v_WorldNorm;
layout (location = 3) in vec3   v_WorldTan;
layout (location = 4) in vec3   v_WorldBitan;
layout (location = 5) in vec4   v_VertColor;

// Output color
layout (location = 0) out vec4 FragColor;
// Output Normal
layout (location = 1) out vec4 FragNormal;
// Output Material Properties
layout (location = 2) out vec4 FragMatProps;



//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    // ********************************
    // Base (albedo) color
    // ********************************
    // Get color from the color texture
    vec4 AlbedoColor = texture( u_DiffuseTex, v_TexCoord.xy );
    AlbedoColor.xyzw *= FragCB.Color.xyzw;

    // Adjust by vertex color.
    AlbedoColor.xyzw *= v_VertColor.xyzw;

    // Get base normal from the bump texture
    vec4 NormTexValue = texture( u_NormalTex, v_TexCoord.xy );
    vec3 N = NormTexValue.xyz * 2.0 - 1.0;

    // Need matrix to convert to tangent space
    // vec3 binormal = cross(v_WorldNorm, v_WorldTan);
    // mat3 WorldToTan = mat3(normalize(v_WorldTan), normalize(binormal), normalize(v_WorldNorm));
    mat3 WorldToTan = mat3(normalize(v_WorldTan), normalize(v_WorldBitan), normalize(v_WorldNorm));
    
    // Convert the bump normal to tangent space
    vec3 BumpNormal = normalize(WorldToTan * N);

    // Write out the color
    FragColor = vec4(AlbedoColor.rgb, 1.0);

    // Write out the Normal and put depth value in the 'alpha' channel
    FragNormal = vec4(BumpNormal.xyz, 1.0 - gl_FragCoord.z);

    // Material Properties
    FragMatProps = FragCB.MaterialProps;
}

