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

layout(set = 0, binding = 0) uniform texture2D SourceTexture;
layout(set = 0, binding = 1) uniform sampler SourceSampler;

layout(set = 0, binding = 2) uniform texture2D BlurTexture;
layout(set = 0, binding = 3) uniform sampler BlurSampler;

layout(location = 0) out vec4 FragColor;

vec3 pow3(vec3 v, float p) { return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p)); }

void main()
{
    vec4 src = texture(sampler2D(SourceTexture, SourceSampler), in_TEXCOORD0);
    src.rgb = pow3(src.rgb, 1.0/2.2);
    vec4 bloom = texture(sampler2D(BlurTexture, BlurSampler), in_TEXCOORD0);
    
    FragColor = bloom + src;
}
