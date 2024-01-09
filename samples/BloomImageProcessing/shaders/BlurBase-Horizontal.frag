//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 450

precision mediump float; precision mediump int;

layout(location = 0) noperspective in vec2 in_TEXCOORD0;

layout(set = 0, binding = 0, std140) uniform WeightInfo
{
    vec4 weights[8];
} _Globals;

layout(set = 0, binding = 1) uniform texture2D SourceTexture;
layout(set = 0, binding = 2) uniform sampler SourceSampler;

layout(location = 0) out vec4 FragColor;

void main()
{
	const float StepX = 1.0 / 1920.0;
	const float StepY = 0;

	int WeightSize = int(_Globals.weights[0][0]);
	FragColor = vec4(0);
	for (int ww = 0; ww < WeightSize; ++ww)
	{
		float coordIndex = float(ww - WeightSize / 2);
		ivec2 weightIndex = ivec2((ww + 1) / 4, (ww + 1) % 4);
		FragColor += texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0 + vec2(coordIndex * StepX, coordIndex * StepY)) * _Globals.weights[weightIndex.x][weightIndex.y];
	}
}
