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
#define SHADER_FRAG_UBO_LOCATION            1

// Texture Locations
#define SHADER_DIFFUSE_TEXTURE_LOC            2
#define SHADER_NORMAL_TEXTURE_LOC             3
#define SHADER_EMISSIVE_TEXTURE_LOC           4
#define SHADER_METALLIC_ROUGHNESS_TEXTURE_LOC 5

#define NUM_SPOT_LIGHTS (4)

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff 
{
    vec4 Color;
    vec4 ORM;

} FragCB;

#ifndef PI
#define PI (3.14159265359)
#endif

// Textures
layout(set = 0, binding = SHADER_DIFFUSE_TEXTURE_LOC)            uniform sampler2D u_DiffuseTex;
layout(set = 0, binding = SHADER_NORMAL_TEXTURE_LOC)             uniform sampler2D u_NormalTex;
layout(set = 0, binding = SHADER_EMISSIVE_TEXTURE_LOC)           uniform sampler2D u_EmissiveTex;
layout(set = 0, binding = SHADER_METALLIC_ROUGHNESS_TEXTURE_LOC) uniform sampler2D u_MetallicRoughnessTex;

// Varying's
layout (location = 0) in vec2   v_TexCoord;
layout (location = 1) in vec3   v_WorldPos;
layout (location = 2) in vec3   v_WorldNorm;
layout (location = 3) in vec3   v_WorldTan;
layout (location = 4) in vec3   v_WorldBitan;
layout (location = 5) in vec4   v_ShadowCoord;
layout (location = 6) in vec4   v_VertColor;

// Output color
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 NormalColor;

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    vec2 LocalTexCoord = vec2(v_TexCoord.xy);

    // ********************************
    // Base (albedo) color
    // ********************************
    // Get color from the color texture

    vec4 DiffuseColor = texture( u_DiffuseTex, v_TexCoord.xy );
    DiffuseColor.xyzw *= FragCB.Color.xyzw;

    if(DiffuseColor.a < 0.5)
    {
        discard;
    }

    // Adjust by vertex color.
    DiffuseColor.xyzw *= v_VertColor.xyzw;

    vec4 Emissive = texture( u_EmissiveTex, v_TexCoord.xy );
    vec4 MetallicRoughness = texture( u_MetallicRoughnessTex, v_TexCoord.xy );

    // Get base normal from the bump texture
    vec3 Normal = texture( u_NormalTex, v_TexCoord.xy ).rgb;
    Normal = Normal * 2.0 - 1.0;

	mat3 TBN = mat3(normalize(v_WorldTan), normalize(v_WorldBitan), normalize(v_WorldNorm));
    Normal = normalize(TBN * Normal);

    NormalColor = vec4(Normal, 0.0);
    FragColor.rgb = DiffuseColor.rgb;
    FragColor.a = DiffuseColor.a;
}