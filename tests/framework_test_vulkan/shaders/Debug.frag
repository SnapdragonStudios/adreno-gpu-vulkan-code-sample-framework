//=============================================================================
//
//
//                  Copyright (c) 2020 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

// Debug.frag

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Uniform buffer locations
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Texture Locations
#define SHADER_DIFFUSE_TEXTURE_LOC          2
#define SHADER_NORMAL_TEXTURE_LOC           3
#define SHADER_SHADOW_DEPTH_TEXTURE_LOC     4
#define SHADER_SHADOW_COLOR_TEXTURE_LOC     5
#define SHADER_ENVIRONMENT_TEXTURE_LOC      6
#define SHADER_IRRADIANCE_TEXTURE_LOC       7
#define SHADER_REFLECT_TEXTURE_LOC          8

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff 
{
    vec4    Color;
    vec4    EyePos;
    vec4    LightDir;
    vec4    LightColor;
} FragCB;

// Textures
layout(set = 0, binding = SHADER_DIFFUSE_TEXTURE_LOC) uniform sampler2D             u_DiffuseTex;

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
    // Base Lighting
    // ********************************
    // Get base color from the color texture
    vec4 DiffuseColor = texture( u_DiffuseTex, v_TexCoord.xy );
    // vec4 DiffuseColor = vec4(0.8, 0.8, 0.8, 1.0);
    DiffuseColor.xyzw *= FragCB.Color.xyzw;

    // Adjust by vertex color.
    DiffuseColor.xyzw *= v_VertColor.xyzw;

    // Need the normal
    vec3 BumpNormal = normalize(v_WorldNorm.xyz);

    // Light direction is from world position to light position
    // vec3 LightDir = normalize(FragCB.LightPos.xyz - v_WorldPos.xyz);
    
    // Can now figure out the diffuse amount
    float DiffuseAmount = max(0.25, dot(BumpNormal.xyz, -FragCB.LightDir.xyz));
    
    // For specular, we need Half vector
    vec3 EyeDir = normalize(FragCB.EyePos.xyz - v_WorldPos.xyz);
    vec3 Half = normalize(EyeDir - FragCB.LightDir.xyz);
    float Specular = max(0.0, dot(BumpNormal.xyz, Half));
    Specular = pow(Specular, FragCB.LightColor.w); 

    // ********************************
    // Start adding in the colors
    // ********************************

    vec3 LightTotal = DiffuseAmount * DiffuseColor.xyz * FragCB.LightColor.xyz;

    // TODO: This equation may no longer be correct since removed many parts
    vec3 FinalColor =   LightTotal + Specular * FragCB.LightColor.xyz;

    // Write out the color and put depth value in the alpha channel
    FragColor = vec4(FinalColor.rgb, FragCB.Color.a);

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    FragColor = vec4(DiffuseColor.xyz, 1.0);
}

