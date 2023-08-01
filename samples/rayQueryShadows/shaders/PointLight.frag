//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_query : enable

// Buffer binding locations
#define SHADER_FRAG_UBO_LOCATION            0
#define SHADER_ALBEDO_TEXTURE_LOC           1
#define SHADER_NORMAL_TEXTURE_LOC           2
#define SHADER_RAY_TRACE_AS_LOC             3

layout(set = 0, binding = SHADER_ALBEDO_TEXTURE_LOC) uniform sampler2D       u_AlbedoTex; // Albedo RGB, Specular is in A
layout(set = 0, binding = SHADER_NORMAL_TEXTURE_LOC) uniform sampler2D       u_NormalTex;
layout(set = 0, binding = SHADER_RAY_TRACE_AS_LOC) uniform accelerationStructureEXT as_RayTrace;

// Processing just one light against the gbuffer.

// Uniforms
layout(std140, set = 0, binding = SHADER_FRAG_UBO_LOCATION) uniform PointLightUniformBuff 
{
    //
    // Vertex shader
    //
    mat4   MVPMatrix;

    //
    // Fragment shader (could split in to 2 uniform buffers, one per shader stage)
    //
    mat4   ProjectionInv;
    mat4   ViewInv;

    vec4   CameraPos;
    vec3   LightPosition;
    float  LightIntensity;
    vec4   LightColor;
    float  LightRadius;    // Radius of the physical light (not its range of influence)
    float  LightCutoff;    // Cutoff brightness for the light
    float  SpecScale;
    float  SpecPower;
    vec2   WindowSize;
} PointLightUB;

// Finally, the output color
layout (location = 0) out vec4 FragColor;


//-----------------------------------------------------------------------------
vec3 ScreenToWorld(vec2 ScreenCoord/*0-1 range*/, float Depth/*0-1*/)
//-----------------------------------------------------------------------------
{
    vec4 ClipSpacePosition = vec4((ScreenCoord * 2.0) - vec2(1.0), Depth, 1.0);
    ClipSpacePosition.y = -ClipSpacePosition.y;
    vec4 ViewSpacePosition = PointLightUB.ProjectionInv * ClipSpacePosition;

    // Perspective division
    ViewSpacePosition /= vec4(ViewSpacePosition.w);

    vec4 WorldSpacePosition = PointLightUB.ViewInv * ViewSpacePosition;
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
	vec2 LocalTexCoord = gl_FragCoord.xy / PointLightUB.WindowSize;

    // ********************************
    // Albedo Color
    // ********************************
    vec4 AlbedoColor = texture( u_AlbedoTex, LocalTexCoord.xy );

    // ********************************
    // Normal (and depth)
    // ********************************
    vec4 NormalWithDepth = texture( u_NormalTex, LocalTexCoord.xy );
    vec3 WorldNormal = NormalWithDepth.xyz;
    float Depth = 1.0 - NormalWithDepth.w;

    FragColor.a = 1.0;

    // Determine World position of pixel
    vec3 WorldPos = ScreenToWorld( LocalTexCoord, Depth );
    // Add in a little bias (along the surface normal) to account for z-depth accuracy.
    WorldPos += WorldNormal;

    // ********************************
    // Raytraced Shadow
    // ********************************
    float ShadowAtten = 1.0f;   // Assume not in shadow

    // Calculate how far away the light is (and its direction)
    vec3 WorldToLightVec = PointLightUB.LightPosition.xyz - WorldPos;
    float LightDistance = length(WorldToLightVec) - PointLightUB.LightRadius;
    vec3 DirectionToLight =  normalize(WorldToLightVec);

    float MinDistance = 0.1;

    // Setup the Shadow ray query (to the light).
    {
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery, as_RayTrace, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF/*cullMask*/, WorldPos, MinDistance, DirectionToLight, LightDistance);

        // Traverse the query.
        while(rayQueryProceedEXT(rayQuery))
        {
        }

        // Determine if the shadow query collided.
        if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        {
          // Got an intersection == Shadow
          //float intersectionDistance = rayQueryGetIntersectionTEXT(rayQuery, true);
          ShadowAtten = 0.0;//intersectionDistance / (LightDistance + 0.1);
        }
    }

    // ********************************
    // Light falloff
    // ********************************

    float LightAtten = LightFalloff(WorldToLightVec, PointLightUB.LightRadius, PointLightUB.LightCutoff, PointLightUB.LightIntensity);

    vec3 EyeDir = normalize(PointLightUB.CameraPos.xyz - WorldPos);

    vec3 WorldToLightNorm = normalize(WorldToLightVec);

    float LightAng = max(0.0, dot(WorldToLightNorm, WorldNormal));

    // Spec (blinn-phong)
    vec3 LightDir = WorldToLightNorm;
    vec3 HalfVector = normalize(LightDir + EyeDir);
    float Spec = AlbedoColor.a/*spec scale*/ * pow(max(dot(WorldNormal,HalfVector),0.0), PointLightUB.SpecPower) * PointLightUB.SpecScale;

    vec3 LightAmt = ShadowAtten * PointLightUB.LightColor.rgb * (Spec + LightAng ) * LightAtten;

    FragColor.rgb = AlbedoColor.rgb * LightAmt;

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    //FragColor.rgb = Irradiance;
    //FragColor.rgb = WorldNormal;
    //FragColor.rgb = vec3(1.0, 0.0f, 0.0);//AlbedoColor.rgb;
    //FragColor.rgb = vec3(AmbientOcclusion,AmbientOcclusion,AmbientOcclusion);
    //FragColor.a = 1.0;
    //FragColor = vec4(0.8, 0.2, 0.8, 1.0);
    //FragColor = vec4(LightAmt.x, LightAmt.y, 1.0, 1.0);
}

