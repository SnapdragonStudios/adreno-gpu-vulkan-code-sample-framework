//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

// Buffer binding locations
#define SHADER_FRAG_UBO_LOCATION            0
#define SHADER_ALBEDO_TEXTURE_LOC           1
#define SHADER_NORMAL_TEXTURE_LOC           2
#define SHADER_RT_SHADOW_TEXTURE_LOC        3
#define SHADER_IRRADIANCE_TEXTURE_LOC       4

layout(set = 0, binding = SHADER_ALBEDO_TEXTURE_LOC) uniform sampler2D       u_AlbedoTex; // Albedo RGB, Specular is in A
layout(set = 0, binding = SHADER_NORMAL_TEXTURE_LOC) uniform sampler2D       u_NormalTex;
layout(set = 0, binding = SHADER_RT_SHADOW_TEXTURE_LOC) uniform sampler2D    u_ShadowTex;   // ray trace/query generated shadow texture
layout(set = 0, binding = SHADER_IRRADIANCE_TEXTURE_LOC) uniform samplerCube u_IrradianceTex;

// Varying's
layout (location = 0) in vec2   v_TexCoord;

// Uniforms
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform FragConstantsBuff
{
    mat4 ProjectionInv;
    mat4 ViewInv;
    mat4 WorldToShadow; // For rasterized depth shadow
    vec4 CameraPos;

    // Point Light
    vec4 PointLightPosition;
    vec4 PointLightColor;

    // Directional Light
    vec4 DirectionalLightDirection;
    vec4 DirectionalLightColor;

    vec4 AmbientColor;

    float PointLightRadius; // of the light itself (not the area of influence)
    float PointLightCutoff;

    float SpecScale;
    float SpecPower;
    float irradianceAmount;
    float irradianceMipLevel;
    float AmbientOcclusionScale;
    int Width;
    int Height;

} FragCB;

// Finally, the output color
layout (location = 0) out vec4 FragColor;


//-----------------------------------------------------------------------------
vec3 ScreenToWorld(vec2 ScreenCoord/*0-1 range*/, float Depth/*0-1*/)
//-----------------------------------------------------------------------------
{
    vec4 ClipSpacePosition = vec4((ScreenCoord * 2.0) - vec2(1.0), Depth, 1.0);
    ClipSpacePosition.y = -ClipSpacePosition.y;
    vec4 ViewSpacePosition = FragCB.ProjectionInv * ClipSpacePosition;

    // Perspective division
    ViewSpacePosition /= vec4(ViewSpacePosition.w);

    vec4 WorldSpacePosition = FragCB.ViewInv * ViewSpacePosition;
    return WorldSpacePosition.xyz;
}


//-----------------------------------------------------------------------------
float LightFalloff(vec3 WorldToLightVec, float LightRadius, float LightCutoff, float LightIntensity)
//-----------------------------------------------------------------------------
{
    // calculate normalized light vector and distance to sphere light surface
    vec3 L = WorldToLightVec;
    float distance = length(L);
    float d = max(distance - LightRadius, 0.0);
    L /= distance;
     
    // calculate basic attenuation
    float denom = (d/LightRadius) + 1.0;
    float LightAttenuation = 1.0 / (denom*denom);
     
    // scale and bias attenuation
    LightAttenuation = LightIntensity * (LightAttenuation - (LightCutoff / LightIntensity)) / (1.0 - (LightCutoff / LightIntensity));
    LightAttenuation = max(LightAttenuation, 0);

    return LightAttenuation;
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
    // Image Based Lighting
    // ********************************
    // Irradiance replaces ambient
    vec3 IrradianceLookup = vec3(Normal.x, -Normal.y, Normal.z);
    vec3 IrradianceMapColor = textureLod(u_IrradianceTex, IrradianceLookup, FragCB.irradianceMipLevel).rgb;
    vec3 Irradiance = FragCB.irradianceAmount * IrradianceMapColor.xyz;

    //FragColor.rgb = AlbedoColor.rgb;
    FragColor.a = 1.0;

    // Determine World position of pixel
    vec3 WorldPos = ScreenToWorld( LocalTexCoord, Depth );

    // ********************************
    // Shadow
    // ********************************
#if 1
    // Raytraced Shadow
    float ShadowAtten = texture( u_ShadowTex, LocalTexCoord.xy ).x;
#else
    float ShadowAtten = 1.0;
#endif

    vec3 EyeDir = normalize(FragCB.CameraPos.xyz - WorldPos);

    vec3 LightAmt = vec3(0.0);//Ambient;

    {
        vec4 LightPos = FragCB.PointLightPosition; //w is intensity, xyz world position
        vec3 WorldToLightVec = LightPos.xyz - WorldPos;
        float LightDist2 = dot(WorldToLightVec, WorldToLightVec);
        vec3 WorldToLightNorm = normalize(WorldToLightVec);

        float LightAtten = LightFalloff(WorldToLightVec, FragCB.PointLightRadius, FragCB.PointLightCutoff, FragCB.PointLightPosition.w);

        float LightAng = max(0.0, dot(WorldToLightNorm, Normal));

        // Spec (blinn-phong)
        vec3 LightDir = WorldToLightNorm;
        vec3 HalfVector = normalize(LightDir + EyeDir);
        float Spec = AlbedoColor.a/*spec scale*/ * pow(max(dot(Normal,HalfVector),0.0), FragCB.SpecPower) * FragCB.SpecScale;

        LightAmt += ShadowAtten * FragCB.PointLightColor.rgb * (Spec + LightAng ) * LightAtten;
    }

    FragColor.rgb = AlbedoColor.rgb * (LightAmt + Irradiance);

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    //FragColor.rgb = Irradiance;
    //FragColor.rgb = Normal;
    //FragColor.rgb = WorldPos;
    //FragColor.rgb = vec3(1.0, 0.0f, 0.0);//AlbedoColor.rgb;
    //FragColor.rgb = vec3(AmbientOcclusion,AmbientOcclusion,AmbientOcclusion);
    //FragColor.a = 1.0;
    // FragColor = vec4(0.8, 0.2, 0.8, 1.0);
    //FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}

