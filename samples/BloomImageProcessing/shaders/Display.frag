//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 450

precision mediump float; precision mediump int;

layout(location = 0) noperspective in vec2 in_TEXCOORD0;

layout(set = 0, binding = 0) uniform texture2D SourceTexture;
layout(set = 0, binding = 1) uniform sampler SourceSampler;

layout(set = 0, binding = 2) uniform texture2D BlurTexture;
layout(set = 0, binding = 3) uniform sampler BlurSampler;

layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0) +
	            texture(sampler2D(BlurTexture, BlurSampler), in_TEXCOORD0);
}
