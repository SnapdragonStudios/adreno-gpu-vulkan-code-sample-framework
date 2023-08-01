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

// These start back over at 0!
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Vertex rate data
layout (location = SHADER_ATTRIB_LOC_POSITION ) in vec4 a_Position;
// Instance rate data
layout (location = SHADER_ATTRIB_LOC_INSTANCE_MATRIX ) in mat3x4 a_InstanceMatrix3x4;   // use a mat3x4 (row major, ie transposed from 'normal' GLSL, but less input slots)

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_VERT_UBO_LOCATION) uniform VertConstantsBuff 
{
    mat4 MVPMatrix;
} VertCB;

// Varying's
// None!

void main()
{
    vec3 WorldPosition = vec4(a_Position.xyz, 1.0) * a_InstanceMatrix3x4;  // a_InstanceMatrix is 3x4 to reduce number of input binding slots needed.  Because it is transposed (from the normal glsl expectation of a 4x3) we do a post multiply.
    gl_Position = VertCB.MVPMatrix * vec4(WorldPosition, 1.0); 
}
