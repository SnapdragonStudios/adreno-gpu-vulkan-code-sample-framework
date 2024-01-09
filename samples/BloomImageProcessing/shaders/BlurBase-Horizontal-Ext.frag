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

layout(set = 0, binding = 0, std140) uniform WeightInfo
{
    vec4 weights[8];
} _Globals;


layout(set = 0, binding = 1) uniform texture2D SourceTexture;
layout(set = 0, binding = 2) uniform sampler SourceSampler;
layout(set = 0, binding = 3) uniform texture2DArray BloomWeightTexture;
layout(set = 0, binding = 4) uniform sampler BloomWeightSampler;

layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = textureWeightedQCOM(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0, sampler2DArray(BloomWeightTexture, BloomWeightSampler));
}
