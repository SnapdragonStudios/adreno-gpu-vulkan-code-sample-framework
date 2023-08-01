//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

// Blit.frag

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

// Buffer binding locations
#define SHADER_FRAG_UBO_LOCATION            0
#define SHADER_ALBEDO_TEXTURE_LOC           1
#define SHADER_NORMAL_TEXTURE_LOC           2
#define SHADER_AO_TEXTURE_LOC               3
#define SHADER_SHADOW_TEXTURE_LOC           4

layout(set = 0, binding = SHADER_ALBEDO_TEXTURE_LOC) uniform sampler2D u_AlbedoTex;
layout(set = 0, binding = SHADER_NORMAL_TEXTURE_LOC) uniform sampler2D u_NormalTex;
//layout(set = 0, binding = SHADER_DEPTH_TEXTURE_LOC) uniform sampler2D  u_DepthTex;
layout(set = 0, binding = SHADER_AO_TEXTURE_LOC) uniform sampler2D     u_AOTex;
layout(set = 0, binding = SHADER_SHADOW_TEXTURE_LOC) uniform sampler2D u_ShadowMap;

// Varying's
layout (location = 0) in vec2   v_TexCoord;

// We run all the lights in the same shader, ideally we would render each light individually (using their screen space bounding area) and add each lights contribution seperately - can go to 100s of lights that way.
#define NUM_POINT_LIGHTS 4
#define NUM_SPOT_LIGHTS 4

// Uniforms
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff
{
    mat4 ProjectionInv;
    mat4 ViewInv;
    mat4 ViewProjectionInv; // ViewInv * ProjectionInv
    mat4 WorldToShadow;
    vec4 ProjectionInvW;    // w components of ProjectionInv
    vec4 CameraPos;

    vec4 LightPositions[NUM_POINT_LIGHTS];
    vec4 SpotLightPositions[NUM_SPOT_LIGHTS];
    vec4 LightColors[NUM_POINT_LIGHTS];
    vec4 SpotLightColors[NUM_SPOT_LIGHTS];

    vec4 AmbientColor;

    float AmbientOcclusionScale;
    int Width;
    int Height;

} FragCB;

// Finally, the output color
layout (location = 0) out vec4 FragColor;

#include "ShadowShared.h"


//-----------------------------------------------------------------------------
vec3 ScreenToWorld(vec2 ScreenCoord/*0-1 range*/, float Depth/*0-1*/)
//-----------------------------------------------------------------------------
{
#if 0
    // Original ScreenToWorld implementation...
    // Performs a matrix multipy to get the ViewSpacePositon and another matrix multiply to take that position to the world space (after perspective divide)
    vec4 ClipSpacePosition = vec4((ScreenCoord * 2.0) - vec2(1.0), Depth, 1.0);
    ClipSpacePosition.y = -ClipSpacePosition.y;

    vec4 ViewSpacePosition = FragCB.ProjectionInv * ClipSpacePosition;

    // Perspective division
    ViewSpacePosition /= vec4(ViewSpacePosition.w);

    vec4 WorldSpacePosition = FragCB.ViewInv * ViewSpacePosition;
    return WorldSpacePosition.xyz;
#endif
    // Faster ScreenToWorld does one dotproduct with the inverse projection matrix to the perspective divisor and does one full (xyz) matrix multiply which is then perspective divided
    // Thanks to David McAllister for pointing this out.
    vec4 ClipSpacePosition = vec4((ScreenCoord * 2.0) - vec2(1.0), Depth, 1.0);
    ClipSpacePosition.y = -ClipSpacePosition.y;

    //  Just one dp4 to calculate w, so 3(4-1) dp4 calculations can be saved from previous mat4*vec4
    float ViewSpacePositionW = dot(FragCB.ProjectionInvW, ClipSpacePosition);
    
    vec3 WorldSpacePosition = (FragCB.ViewProjectionInv * ClipSpacePosition).xyz;
    return WorldSpacePosition.xyz/ViewSpacePositionW;
}

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    vec2 LocalTexCoord = vec2(v_TexCoord.xy);

    // ********************************
    // Albedo Color
    // ********************************
    vec4 AlbedoColor = texture( u_AlbedoTex, LocalTexCoord.xy );

    // ********************************
    // Normal
    // ********************************
    vec4 NormalWithDepth = texture( u_NormalTex, LocalTexCoord.xy );
    vec3 Normal = NormalWithDepth.xyz;
    float Depth = 1.0 - NormalWithDepth.w;
    //float Depth = texture( u_DepthTex, LocalTexCoord.xy ).x;  // no longer using u_DepthTex so we (poentially) avoid a depth resolve operation before reading (instead gbuffer Normal.w has the depth)

    // ********************************
    // Ambient Occlusion
    // ********************************
    float AmbientOcclusion = texture( u_AOTex, LocalTexCoord.xy ).x;

    FragColor.rgb = AlbedoColor.rgb;
    FragColor.a = 1.0;

    // Determine World position of pixel
    vec3 WorldPos = ScreenToWorld( LocalTexCoord, Depth );

    // Calculate ambient (fixed value with darkening by Ambient Occlusion)
    vec3 Ambient = FragCB.AmbientColor.rgb;
    Ambient = Ambient * (1.0 - min(1.0, (1.0 - AmbientOcclusion) * FragCB.AmbientOcclusionScale));

    // Factor in shadow
    vec4 ShadowPos = FragCB.WorldToShadow * vec4(WorldPos, 1.0);
    // convert -1 to 1 screen co-ord range to texture 0 to 1.
    ShadowPos.xy = ShadowPos.xy * 0.5 + 0.5;
    // Lookup the Shadow amount
    float ShadowAtten = GetShadowAmount(ShadowPos);

    vec3 EyeDir = normalize(FragCB.CameraPos.xyz - WorldPos);

    vec3 LightAmt = Ambient;

    for(int LightIdx=0;LightIdx < NUM_SPOT_LIGHTS;++LightIdx)
    {
        vec4 LightPos = FragCB.SpotLightPositions[LightIdx]; //w is intensity, xyz world position
        vec3 WorldToLightVec = LightPos.xyz - WorldPos;
        float WorldToLightDist2 = dot(WorldToLightVec, WorldToLightVec);
        vec3 WorldToLightNorm = normalize(WorldToLightVec);

        float SpotFalloffAng = dot(vec3(0.0, 1.0, 0.0), WorldToLightNorm);
		float SpotFalloff =  clamp((SpotFalloffAng - 0.8) / 0.2, 0.0, 1.0);

        float LightAng = max(0.0, dot( WorldToLightNorm, Normal));

        // Spec (blinn-phong)
        vec3 LightDir = WorldToLightNorm;
        vec3 HalfVector = normalize(LightDir+EyeDir);
        float Spec = pow(max(dot(Normal,HalfVector),0.0), 100) * 1.5;

        LightAmt += ShadowAtten * SpotFalloff * FragCB.SpotLightColors[LightIdx].rgb * (Spec + LightAng) * LightPos.w / (1.0 + WorldToLightDist2);
    }


    for(int LightIdx=0;LightIdx < NUM_POINT_LIGHTS;++LightIdx)
    {
        vec4 LightPos = FragCB.LightPositions[LightIdx]; //w is intensity, xyz world position
        vec3 WorldToLightVec = LightPos.xyz - WorldPos;
        float LightDist2 = dot(WorldToLightVec, WorldToLightVec);
        vec3 WorldToLightNorm = normalize(WorldToLightVec);

        float LightAng = max(0.0, dot(WorldToLightNorm, Normal));

        // Spec (blinn-phong)
        vec3 LightDir = WorldToLightNorm;
        vec3 HalfVector = normalize(LightDir + EyeDir);
        float Spec = pow(max(dot(Normal,HalfVector),0.0), 100) * 1.5;

        LightAmt += FragCB.LightColors[LightIdx].rgb * (Spec + LightAng ) * LightPos.w / (1.0 + LightDist2);
    }

    FragColor.rgb = AlbedoColor.rgb * LightAmt;

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    //FragColor.rgb = Normal.rgb;
    //FragColor.a = 1.0;
    // FragColor = vec4(0.8, 0.2, 0.8, 1.0);
}

