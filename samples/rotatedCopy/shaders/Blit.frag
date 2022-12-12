//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Buffer binding locations
#define SHADER_DIFFUSE_TEXTURE_LOC          0
#define SHADER_OVERLAY_TEXTURE_LOC          1

layout(set = 0, binding = SHADER_DIFFUSE_TEXTURE_LOC) uniform sampler2D u_DiffuseTex;
layout(set = 0, binding = SHADER_OVERLAY_TEXTURE_LOC) uniform sampler2D u_OverlayTex;

// Varying's
layout (location = 0) in  vec2 v_TexCoord;

// Finally, the output color
layout (location = 0) out vec4 FragColor;

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    vec2 LocalTexCoord = vec2(v_TexCoord.xy);

    FragColor = texture( u_DiffuseTex, LocalTexCoord );

    // Overlay (GUI)
    vec4 OverlayColor = texture( u_OverlayTex, LocalTexCoord );
    FragColor.rgb =  FragColor.rgb*(1.0-OverlayColor.a) + OverlayColor.rgb;
    FragColor.a = 1.0;
}

