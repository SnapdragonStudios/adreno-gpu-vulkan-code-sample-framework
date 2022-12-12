//================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//================================================================================================

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

    // X: Normal Height
    // Y: Normal Mirror Reflect Amount
    // Z: Not Used
    // W: Not Used
    vec4    NormalHeight;

} FragCB;

#define NORMAL_HEIGHT           FragCB.NormalHeight.x
#define NORMAL_MIRROR_AMOUNT    FragCB.NormalHeight.y

#define REFLECT_MIP_LEVEL       FragCB.ReflectParams.x
#define REFLECT_AMOUNT          FragCB.ReflectParams.y
#define REFLECT_FRESNEL_MIN     FragCB.ReflectParams.z
#define REFLECT_FRESNEL_MAX     FragCB.ReflectParams.w

#define IRRADIANCE_MIP_LEVEL    FragCB.IrradianceParams.x
#define IRRADIANCE_AMOUNT       FragCB.IrradianceParams.y

// Textures
layout(set = 0, binding = SHADER_DIFFUSE_TEXTURE_LOC) uniform sampler2D             u_DiffuseTex;
layout(set = 0, binding = SHADER_NORMAL_TEXTURE_LOC) uniform sampler2D              u_NormalTex;

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
// Output Normal
layout (location = 1) out vec4 FragNormal;



//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    // ********************************
    // Base (albedo) color
    // ********************************
    // Get color from the color texture
    vec4 AlbedoColor = texture( u_DiffuseTex, v_TexCoord.xy );
    // vec4 AlbedoColor = vec4(0.8, 0.8, 0.8, 1.0);
    AlbedoColor.xyzw *= FragCB.Color.xyzw;

    // Adjust by vertex color.
    AlbedoColor.xyzw *= v_VertColor.xyzw;

    // Get base normal from the bump texture
    vec4 NormTexValue = texture( u_NormalTex, v_TexCoord.xy );
    vec3 N = NormTexValue.xyz * 2.0 - 1.0;

    N.xy *= NORMAL_HEIGHT;
    N = normalize(N);
    // N = vec3(0.0, 0.0, 1.0); // If we want to hard code 

    // Need matrix to convert to tangent space
    // vec3 binormal = cross(v_WorldNorm, v_WorldTan);
    // mat3 WorldToTan = mat3(normalize(v_WorldTan), normalize(binormal), normalize(v_WorldNorm));
    mat3 WorldToTan = mat3(normalize(v_WorldTan), normalize(v_WorldBitan), normalize(v_WorldNorm));
    
    // Convert the bump normal to tangent space
    vec3 BumpNormal = normalize(WorldToTan * N);

    // Write out the color and put depth value in the alpha channel
    FragColor = vec4(AlbedoColor.rgb, FragCB.Color.a);
    // Write out the Normal
    FragNormal = vec4(BumpNormal.xyz, 1.0 - gl_FragCoord.z);

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    // FragColor = vec4(Specular, Specular, Specular, 1.0);
    // FragColor = vec4(BumpNormal.xyz, 1.0);
    // FragColor = ShadowAmount;
}

