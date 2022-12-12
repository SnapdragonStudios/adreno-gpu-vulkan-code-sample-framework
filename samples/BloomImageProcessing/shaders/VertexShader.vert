//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 450

invariant gl_Position;

layout(location = 0) noperspective out mediump vec2 out_var_TEXCOORD0;

vec2 GetQuadVertexPosition(uint vertexID)
{
	float x = float(((uint(vertexID) + 2u) / 3u)%2u);
	float y = float(((uint(vertexID) + 1u) / 3u)%2u); 

	return vec2(x, y);
}

void main()
{
	vec2 in_var_ATTRIBUTE0 = GetQuadVertexPosition(gl_VertexIndex);
    float _44 = 2.0 * in_var_ATTRIBUTE0.x;
    float _45 = _44 - 1.0;
    float _47 = 2.0 * in_var_ATTRIBUTE0.y;
    float _48 = 1.0 - _47;
    vec4 _49 = vec4(_45, _48, 0.0, 1.0);
    vec2 _55 = in_var_ATTRIBUTE0;
	_55.y = 1.0 - _55.y;
    out_var_TEXCOORD0 = _55;
    gl_Position = _49;
}

