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

// Define available features
#define ENABLE_SHADOWS
#define ENABLE_REFLECTIONS
#define ENABLE_REFRACTION

// Uniform buffer locations
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Texture Locations
#define SHADER_EMISSIVE_TEXTURE_LOC       2

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff 
{
    vec4    Color;
    vec4    EyePos;
    vec4    LightDir;
    vec4    LightColor;

    // X: Normal Height
    // Y: Normal Mirror Reflect Amount
    // Z: Not Used
    // W: Not Used
    vec4    NormalHeight;

    vec4    MirrorAmount;

    // X: Screen Width
    // Y: Screen Height
    // Z: One Width Pixel
    // W: One Height Pixel
    vec4    ScreenSize;

    // X: Shadow Width
    // Y: Shadow Height
    // Z: One Width Pixel
    // W: One Height Pixel
    vec4    ShadowSize;

    // X: Mip Level
    // Y: Reflect Amount
    // Z: Reflect Fresnel Min
    // W: Reflect Fresnel Max
    vec4    ReflectParams;

    // X: Mip Level
    // Y: IBL Amount
    // Z: Not Used
    // W: Not Used
    vec4    IrradianceParams;
} FragCB;

// Textures
layout(set = 0, binding = SHADER_EMISSIVE_TEXTURE_LOC) uniform sampler2D u_EmissiveTex;

// Varying's
layout (location = 0) in vec2   v_TexCoord;
layout (location = 1) in vec3   v_WorldPos;
layout (location = 2) in vec3   v_WorldNorm;
layout (location = 3) in vec3   v_WorldTan;
layout (location = 4) in vec3   v_WorldBitan;
layout (location = 5) in vec4   v_ShadowCoord;
layout (location = 6) in vec4   v_VertColor;

// Finally, the output color
layout (location = 0) out vec4 FragColor;

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    // ********************************
    // Base Lighting
    // ********************************
    // Get emissive color from the color texture
    vec4 EmissiveColor = texture( u_EmissiveTex, v_TexCoord.xy );
    //EmissiveColor.xyzw *= FragCB.Color.xyzw;

    // Adjust by vertex color.
    //EmissiveColor.xyzw *= v_VertColor.xyzw;

    vec3 FinalColor = EmissiveColor.xyz * 4.0;

    // Write out the color and put depth value in the alpha channel
    FragColor = vec4(FinalColor.rgb, FragCB.Color.a);
}
