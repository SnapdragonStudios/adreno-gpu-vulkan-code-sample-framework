//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 450

#if ENABLE_QCOM_IMAGE_PROCESSING
#extension GL_QCOM_image_processing : require
#endif // ENABLE_QCOM_IMAGE_PROCESSING

precision mediump float; precision mediump int;

layout(location = 0) noperspective in vec2 in_TEXCOORD0;

layout(set = 0, binding = 0, std140) uniform WeightInfo
{
    vec4 weights[8];
} _Globals;


layout(set = 0, binding = 1) uniform texture2D SourceTexture;
layout(set = 0, binding = 2) uniform sampler SourceSampler;

#if ENABLE_QCOM_IMAGE_PROCESSING
	layout(set = 0, binding = 3) uniform texture2DArray BloomWeightTexture;
	layout(set = 0, binding = 4) uniform sampler BloomWeightSampler;
#endif // ENABLE_QCOM_IMAGE_PROCESSING

layout(location = 0) out vec4 FragColor;

void main()
{
#if ENABLE_QCOM_IMAGE_PROCESSING
	FragColor = textureWeightedQCOM(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0, sampler2DArray(BloomWeightTexture, BloomWeightSampler));
#else
#if VERT_PASS
	const float StepX = 0;
	const float StepY = 1.0 / 1080.0;
#else
	const float StepX = 1.0 / 1920.0;
	const float StepY = 0;
#endif // VERT_PASS
	int WeightSize = int(_Globals.weights[0][0]);
	FragColor = vec4(0);
	for (int ww = 0; ww < WeightSize; ++ww)
	{
		float coordIndex = float(ww - WeightSize / 2);
		ivec2 weightIndex = ivec2((ww + 1) / 4, (ww + 1) % 4);
		FragColor += texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0 + vec2(coordIndex * StepX, coordIndex * StepY)) * _Globals.weights[weightIndex.x][weightIndex.y];
	}
#endif // ENABLE_QCOM_IMAGE_PROCESSING
}
