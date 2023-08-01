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
#define SHADER_ATTRIB_LOC_INSTANCE_MATRIX   1  /* also 2 and 3 */

#define SHADER_VERT_UBO_LOCATION            0

// Vertex rate data
layout (location = SHADER_ATTRIB_LOC_POSITION ) in vec3 a_Position;
// Instance rate data
layout (location = SHADER_ATTRIB_LOC_INSTANCE_MATRIX ) in mat3x4 a_InstanceMatrix3x4;   // use a mat3x4 (row major, ie transposed from 'normal' GLSL, but less input slots)

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_VERT_UBO_LOCATION) uniform VertConstantsBuff 
{
    mat4 VPMatrix;
    vec4 AnimationRotation;
} VertAnimatedCB;

#define PI (3.14159)


void main()
{
    vec3 LocalPosition = a_Position;

    vec3 offset, tmp;
    offset.x = modf(LocalPosition.x*0.01, tmp.x);
    offset.y = 0.0;
    offset.z = modf(LocalPosition.z*0.01, tmp.y);
    float sway = clamp(LocalPosition.y, 0.0, -2.0) * 1.0;
    LocalPosition.x = LocalPosition.x + sin(VertAnimatedCB.AnimationRotation.z)*sway + 1.0*sin(VertAnimatedCB.AnimationRotation.x + 2.0 * PI * offset.x);
    LocalPosition.z = LocalPosition.z + sin(VertAnimatedCB.AnimationRotation.w)*sway + 1.0*sin(VertAnimatedCB.AnimationRotation.y + 2.0 * PI * offset.z);

    vec3 WorldPosition = vec4(LocalPosition, 1.0) * a_InstanceMatrix3x4;  // a_InstanceMatrix is 3x4 to reduce number of input binding slots needed.  Because it is transposed (from the normal glsl expectation of a 4x3) we do a post multiply.
    vec4 TempPos = VertAnimatedCB.VPMatrix * vec4( WorldPosition, 1.0); 

    gl_Position = TempPos;
}
