//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 450

#if ENABLE_QCOM_IMAGE_PROCESSING
#extension GL_QCOM_image_processing : require
#endif // ENABLE_QCOM_IMAGE_PROCESSING

precision mediump float; precision mediump int;

layout(location = 0) noperspective in vec2 in_TEXCOORD0;

layout(set = 0, binding = 0) uniform texture2D SourceTexture;
layout(set = 0, binding = 1) uniform sampler SourceSampler;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec4 downsampleColor = vec4(0);
#if ENABLE_QCOM_IMAGE_PROCESSING
	downsampleColor = textureBoxFilterQCOM(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0, vec2(2.0, 2.0));
#else
	const float StepX = 1.0 / 1920.0;
	const float StepY = 1.0 / 1080.0;
	downsampleColor = (
		texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0 + vec2(0, 0)) +
		texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0 + vec2(StepX, 0)) +
		texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0 + vec2(0, StepY)) +
		texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0 + vec2(StepX, StepY))
	) / 4.0;
#endif // ENABLE_QCOM_IMAGE_PROCESSING
	float brightness = dot(downsampleColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	if (brightness < 0.35) // Discard darker areas
	{
		downsampleColor = vec4(0, 0, 0, 1.0);
	}

	FragColor = downsampleColor;
	FragColor.a = 1.0;
}
