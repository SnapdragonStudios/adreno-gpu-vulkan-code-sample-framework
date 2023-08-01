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
#define SHADER_ATTRIB_LOC_NORMAL            1
#define SHADER_ATTRIB_LOC_TEXCOORD0         2
#define SHADER_ATTRIB_LOC_COLOR             3
#define SHADER_ATTRIB_LOC_TANGENT           4
#define SHADER_ATTRIB_LOC_INSTANCE_MATRIX   5  /* also 6 and 7 */

// These start back over at 0!
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Vertex rate data
layout (location = SHADER_ATTRIB_LOC_POSITION ) in vec3 a_Position;
layout (location = SHADER_ATTRIB_LOC_NORMAL   ) in vec3 a_Normal;
layout (location = SHADER_ATTRIB_LOC_TANGENT  ) in vec4 a_Tangent;
layout (location = SHADER_ATTRIB_LOC_TEXCOORD0) in vec2 a_TexCoord;
layout (location = SHADER_ATTRIB_LOC_COLOR    ) in vec4 a_Color;
// Instance rate data
layout (location = SHADER_ATTRIB_LOC_INSTANCE_MATRIX ) in mat3x4 a_InstanceMatrix3x4;   // use a mat3x4 (row major, ie transposed from 'normal' GLSL, but less input slots)

// Uniform Constant Buffer
layout(std140, set = 0, binding = SHADER_VERT_UBO_LOCATION) uniform VertConstantsBuff 
{
    mat4 VPMatrix;
} VertCB;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

// Varying's
layout (location = 0) out vec2    v_TexCoord;
layout (location = 1) out vec3    v_WorldPos;
layout (location = 2) out vec3    v_WorldNorm;
layout (location = 3) out vec3    v_WorldTan;
layout (location = 4) out vec3    v_WorldBitan;
layout (location = 5) out vec4    v_VertColor;


void main()
{
    vec3 WorldPosition = vec4(a_Position, 1.0) * a_InstanceMatrix3x4;  // a_InstanceMatrix is 3x4 to reduce number of input binding slots needed.  Because it is transposed (from the normal glsl expectation of a 4x3) we do a post multiply.

    // Position and text coord are simple (Except Y in inverted on screen compared to OpenGL)
    vec4 TempPos = VertCB.VPMatrix * vec4( WorldPosition, 1.0); 
    gl_Position = vec4(TempPos.x, -TempPos.y, TempPos.z, TempPos.w);
    v_TexCoord = vec2(a_TexCoord.x, a_TexCoord.y);

    // Need Position in world space
    v_WorldPos = WorldPosition;

    // Need  Normal, Tangent, and Bitangent in world space.  Normalize since the a_InstanceMatrix3x4 may be scaled.
    v_WorldNorm = normalize(vec4(a_Normal.xyz, 0.0) * a_InstanceMatrix3x4);
    v_WorldTan = normalize(vec4(a_Tangent.xyz, 0.0) * a_InstanceMatrix3x4);
    v_WorldBitan = cross(v_WorldNorm, v_WorldTan);

    // Color is simple attribute color
    v_VertColor.xyzw = vec4(a_Color.xyz, 1.0);

}
