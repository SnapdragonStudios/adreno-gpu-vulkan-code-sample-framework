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
#define SHADER_FRAG_UBO_LOCATION            1

layout (location = SHADER_ATTRIB_LOC_POSITION ) in vec4 a_Position;

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_VERT_UBO_LOCATION) uniform VertConstantsBuff 
{
    mat4 MVPMatrix;
    mat4 ModelMatrix;
    mat4 ShadowMatrix;
} VertCB;

// Varying's
// None!

void main()
{
    // Position only
    gl_Position = VertCB.MVPMatrix * vec4(a_Position.xyz, 1.0); 

    // Currently, Depth is being set in range [Near, Far] = [-1, 0].
    // Code thinks it is correctly setting [Near, Far] = [0, 1]
    // Until this is fixed, adjust the depth to desired range.
    // gl_Position.z += 1.0;
}
