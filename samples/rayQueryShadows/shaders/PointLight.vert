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

#define SHADER_ATTRIB_LOC_POSITION          0

// These start back over at 0!
#define SHADER_VERT_UBO_LOCATION            0

layout (location = SHADER_ATTRIB_LOC_POSITION ) in vec3 a_Position;

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_VERT_UBO_LOCATION) uniform PointLightUniformBuff 
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

void main()
{
    // Position and text coord are simple (Except Y in inverted on screen compared to OpenGL)
    vec4 TempPos = PointLightUB.MVPMatrix * vec4(a_Position.xyz, 1.0); 
    gl_Position = vec4(TempPos.x, -TempPos.y, TempPos.z, TempPos.w);
}
