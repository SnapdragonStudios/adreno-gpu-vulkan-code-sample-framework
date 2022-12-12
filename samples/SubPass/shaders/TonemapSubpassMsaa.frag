//=============================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//==============================================================================================

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Buffer binding locations
#define SHADER_DIFFUSE_TEXTURE_LOC          0

#define SHADER_DIFFUSE_SUBPASS_INPUT        0

layout (input_attachment_index = SHADER_DIFFUSE_SUBPASS_INPUT, binding = SHADER_DIFFUSE_TEXTURE_LOC) uniform subpassInputMS u_DiffuseInput;

// Varying's
layout (location = 0) in vec2   v_TexCoord;

// Finally, the output color
layout (location = 0) out vec4 FragColor;


// ACES Filmic Tone-mapping
vec3 ACESFilmic(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
//    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
    return (x*(a*x+b))/(x*(c*x+d)+e);
}

// ACES Filmic Tone-mapping for Rec2020 display.  Input is linear with 1.0 being 80nit.  Output maps to ~12.5 at 1000nits
vec3 ACESFilmicRec2020( vec3 x )
{
    float a = 15.8f;
    float b = 2.12f;
    float c = 1.2f;
    float d = 5.92f;
    float e = 1.9f;
    return ( x * ( a * x + b ) ) / ( x * ( c * x + d ) + e );
}

// Convert to scRGB - same as sRGB conversion (ie linear near zero and then approximation to gamma 2.2 curve) except negative values are also handled.
// If we wanted a sRGB only conversion we would clamp inputs to 0-1 and not have to handle negative cases.
// Could also allow input >1 which allows for HDR but does not give the wider color space of scRGB (enabled by negative rgb components).
float ToSCRGB(float x)
{
    float n = abs(x);
    n = n > 0.0031308 ? (1.055 * pow(n, 1.0/2.4) - 0.055) : (n * 12.92);
    return x > 0 ? n : -n;
}
float FromSCRGB(float x)
{
    float n = abs(x);
    n = n > 0.04045 ? pow((n + 0.055)/1.055, 2.4) : (n / 12.92);
    return x > 0 ? n : -n;
}


//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    vec2 LocalTexCoord = vec2(v_TexCoord.xy);

    // ********************************
    // Texture Colors
    // ********************************
    // Get base color from the color texture
    //vec4 DiffuseColor = texture( u_DiffuseTex, LocalTexCoord.xy );
    vec4 DiffuseColor = subpassLoad( u_DiffuseInput, gl_SampleID );

    // Apply darkening/lightening control
    //float lerp01 = min(1,FragCB.Diffuse);
    //float lerp12 = max(0,FragCB.Diffuse-1);
    //DiffuseColor = DiffuseColor * lerp01 + lerp12 - lerp12 * DiffuseColor;

    // ********************************
    // Alpha Blending
    // ********************************
    FragColor.rgb = DiffuseColor.rgb;
    FragColor.a = 1.0;

    // Color mapping
    FragColor.rgb = ACESFilmic(FragColor.rgb);

    // ********************************
    // sRGB conversion (if speed was a factor we would probably hardcode and have 2 blit shaders)
    // ********************************
//    if (FragCB.sRGB == 1)
    {
//        FragColor.r = ToSCRGB(FragColor.r);
//        FragColor.g = ToSCRGB(FragColor.g);
//        FragColor.b = ToSCRGB(FragColor.b);
    }
}

