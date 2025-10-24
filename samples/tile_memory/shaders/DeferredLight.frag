#version 460
//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#define MAX_LIGHT_COUNT 2048
#define MAX_TILE_MATRIX_DIMENSION_SIZE 8
#define MAX_TILE_MATRIX_SIZE (MAX_TILE_MATRIX_DIMENSION_SIZE * MAX_TILE_MATRIX_DIMENSION_SIZE)

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

precision highp float;
precision highp int;

struct LightUB
{
    vec4 lightPosAndRadius; // xyz = pos | w = radius
    vec4 lightColor;
};

struct SceneInfoUB
{
    mat4 projectionInv;
    mat4 view;
    mat4 viewInv;
    mat4 viewProjection;
    mat4 viewProjectionInv; // ViewInv * ProjectionInv
    vec4 projectionInvW;    // w components of ProjectionInv
    vec4 cameraPos;

    vec4 skyLightDirection;
    vec4 skyLightColor;

    vec4 ambientColor;

    uint viewportWidth;
    uint viewportHeight;
    uint clusterCountX;
    uint clusterCountY;
    int lightCount;
    int debugShaders;
    int ignoreLightTiles;
    int tileMemoryEnabled;
};

layout(set = 0, binding = 0) uniform sampler2D u_DiffuseTex;
layout(set = 0, binding = 1) uniform sampler2D u_NormalTex;
layout(set = 0, binding = 2) uniform highp sampler2D u_DepthTex;

layout(std140, set = 0, binding = 3) uniform SceneInfo 
{
    SceneInfoUB sceneInfo;
};

layout(std140, set = 0, binding = 4) uniform Lights 
{
    LightUB lights[MAX_LIGHT_COUNT];
};

layout(std430, set = 0, binding = 5) buffer TileLightIndices
{
    uint lightIndices[];
};

layout(std430, set = 0, binding = 6) buffer TileLightCounts
{
    uint lightCounts[];
};

// Varying's
layout (location = 0) in vec2   v_TexCoord;
layout (location = 1) in vec4   v_VertColor;

// Finally, the output color
layout (location = 0) out vec4 FragColor;



//-----------------------------------------------------------------------------
vec3 ApplyLight(LightUB light, highp vec3 fragWorldSpacePos, highp vec3 fragNormal)
//-----------------------------------------------------------------------------
{
    highp vec3 lightPosition = light.lightPosAndRadius.xyz;
    highp float lightRadius = light.lightPosAndRadius.w;

    highp vec3 lightDir = normalize(lightPosition - fragWorldSpacePos);
    highp float distance = length(lightPosition - fragWorldSpacePos);

    if (distance > lightRadius * 2.0) 
    {
        return vec3(0.0); // Skip lights that do not affect this fragment
    }

    // float attenuation = 1.0 / (distance * distance);
    float attenuation = 1.0 / (distance * distance / (lightRadius * lightRadius));
    float intensity = max(dot(fragNormal, lightDir), 0.0) * light.lightColor.a;

    return light.lightColor.rgb * attenuation * intensity;
}

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    //
    // NON-SHARED SECTION BEGIN
    //

    uint globalClusterCountX = sceneInfo.clusterCountX;
    uint globalClusterCountY = sceneInfo.clusterCountY;

    vec2 screenSize = vec2(sceneInfo.viewportWidth, sceneInfo.viewportHeight);

    float clusterSizeX  = screenSize.x / float(globalClusterCountX);
    float clusterSizeY = screenSize.y / float(globalClusterCountY);

    //
    // NON-SHARED SECTION END
    //
    // (everything below this is common code between the 2 fragment shader versions)
    // (with the exception of the texture sampling/subpassLoad functions below)
    //

    uint currentClusterIndexX = uint(floor((v_TexCoord.x * screenSize.x) / clusterSizeX));
    uint currentClusterIndexY = uint(floor((v_TexCoord.y * screenSize.y) / clusterSizeY));

    uint clusterIndex = currentClusterIndexY * globalClusterCountX + currentClusterIndexX;

    vec3 diffuseColor = texture( u_DiffuseTex, v_TexCoord.xy ).xyz;
    vec3 normal       = texture( u_NormalTex, v_TexCoord.xy ).xyz;
    float depth       = texture( u_DepthTex, v_TexCoord.xy ).r * 2.0 - 1.0;

    highp vec4 clipSpacePos = vec4(
        v_TexCoord.x * 2.0 - 1.0,
        (1.0 - v_TexCoord.y) * 2.0 - 1.0,
        depth,
        1.0
    );

    highp vec4 fragViewSpacePos = sceneInfo.projectionInv * clipSpacePos;
    fragViewSpacePos /= fragViewSpacePos.w;
    highp vec4 fragWorldSpacePos = sceneInfo.viewInv * fragViewSpacePos;
    
    uint lightCount = lightCounts[clusterIndex];
    highp vec3 accumulatedColor = vec3(0.0);

    if(sceneInfo.ignoreLightTiles != 0)
    {
        for (int i = 0; i < sceneInfo.lightCount; ++i) 
        {
            LightUB light = lights[i];

            accumulatedColor += ApplyLight(light, fragWorldSpacePos.xyz, normal);
        }
    }
    else
    {
        for (uint i = 0u; i < lightCount; ++i) 
        {
            uint lightIndex = lightIndices[clusterIndex * uint(MAX_LIGHT_COUNT) + i];
            LightUB light = lights[lightIndex];

            accumulatedColor += ApplyLight(light, fragWorldSpacePos.xyz, normal);
        }
    }

    FragColor = vec4(accumulatedColor * diffuseColor, 1.0);

//////////////////////////////////////////////////////////////////////////////////////
// DEBUG //
//////////////////////////////////////////////////////////////////////////////////////

    if(sceneInfo.debugShaders != 0)
    {
        ivec2 clusterSize = ivec2(clusterSizeX, clusterSizeY);

        // Compute the local coordinate within the tile
        ivec2 pixelCoord = ivec2(v_TexCoord.xy * screenSize);
        ivec2 localCoord = pixelCoord % clusterSize;

        // Draw grid lines along tile boundaries using green
        float lineThickness = 1.0;
        bool drawGrid = (float(localCoord.x) < lineThickness ||
                          float(localCoord.y) < lineThickness ||
                          float(localCoord.x) > float(clusterSize.x) - lineThickness ||
                          float(localCoord.y) > float(clusterSize.y) - lineThickness);
        if(drawGrid)
        {
            FragColor = mix(FragColor, vec4(0.0, 0.7, 1.0, 1.0), 0.7);
        }

        // Draw debug bar: within each tile, reserve a small horizontal region
        // and subdivide it into MAX_LIGHT_COUNT segments. Fill segments (from left) up to tile_light_count.
        float debugBarHeight = 6.0; // height in pixels of the debug bar
        if(float(localCoord.y) < debugBarHeight)
        {
            float factor = float(lightCount) / float(sceneInfo.lightCount);
            float segmentWidth = float(clusterSize.x) / float(sceneInfo.lightCount);
            int segIndex = int(float(localCoord.x) / segmentWidth);
            if(segIndex < int(lightCount))
            {
                FragColor = mix(FragColor, vec4(factor, 1.0 - factor, 0.0, 1.0), 0.7);
            }
        }
    }

//////////////////////////////////////////////////////////////////////////////////////
// DEBUG //
//////////////////////////////////////////////////////////////////////////////////////
}