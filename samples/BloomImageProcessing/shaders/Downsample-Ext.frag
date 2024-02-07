//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 450

#extension GL_QCOM_image_processing : require

precision mediump float; precision mediump int;

layout(location = 0) noperspective in vec2 in_TEXCOORD0;

layout(set = 0, binding = 0) uniform texture2D SourceTexture;
layout(set = 0, binding = 1) uniform sampler SourceSampler;

layout(location = 0) out vec4 FragColor;

vec3 pow3(vec3 v, float p) { return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p)); }

void main()
{
	vec4 downsampleColor = vec4(0);
	downsampleColor = textureBoxFilterQCOM(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0, vec2(2.0, 2.0));
	downsampleColor.rgb = pow3(downsampleColor.rgb, 1.0/2.2);

	float brightness = dot(downsampleColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	if (brightness < 0.35) // Discard darker areas
	{
		downsampleColor = vec4(0, 0, 0, 1.0);
	}

	FragColor = downsampleColor;
	FragColor.a = 1.0;
}
