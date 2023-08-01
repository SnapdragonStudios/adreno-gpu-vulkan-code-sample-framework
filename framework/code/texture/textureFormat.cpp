//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "textureFormat.hpp"


//-----------------------------------------------------------------------------
bool FormatHasStencil( TextureFormat format )
//-----------------------------------------------------------------------------
{
    switch( format ) {
    case TextureFormat::D32_SFLOAT_S8_UINT:
    case TextureFormat::D24_UNORM_S8_UINT:
    case TextureFormat::D16_UNORM_S8_UINT:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool FormatHasDepth( TextureFormat format )
//-----------------------------------------------------------------------------
{
    switch (format) {
    case TextureFormat::D32_SFLOAT_S8_UINT:
    case TextureFormat::D24_UNORM_S8_UINT:
    case TextureFormat::D16_UNORM_S8_UINT:
    case TextureFormat::D16_UNORM:
    case TextureFormat::D32_SFLOAT:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool FormatIsCompressed( TextureFormat format )
//-----------------------------------------------------------------------------
{
    switch( format ) {
    case TextureFormat::BC1_RGB_UNORM_BLOCK:
    case TextureFormat::BC1_RGB_SRGB_BLOCK:
    case TextureFormat::BC1_RGBA_UNORM_BLOCK:
    case TextureFormat::BC1_RGBA_SRGB_BLOCK:
    case TextureFormat::BC2_UNORM_BLOCK:
    case TextureFormat::BC2_SRGB_BLOCK:
    case TextureFormat::BC3_UNORM_BLOCK:
    case TextureFormat::BC3_SRGB_BLOCK:
    case TextureFormat::BC4_UNORM_BLOCK:
    case TextureFormat::BC4_SNORM_BLOCK:
    case TextureFormat::BC5_UNORM_BLOCK:
    case TextureFormat::BC5_SNORM_BLOCK:
    case TextureFormat::BC6H_UFLOAT_BLOCK:
    case TextureFormat::BC6H_SFLOAT_BLOCK:
    case TextureFormat::BC7_UNORM_BLOCK:
    case TextureFormat::BC7_SRGB_BLOCK:
    case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK:
    case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK:
    case TextureFormat::ETC2_R8G8B8A1_UNORM_BLOCK:
    case TextureFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
    case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK:
    case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
    case TextureFormat::EAC_R11_UNORM_BLOCK:
    case TextureFormat::EAC_R11_SNORM_BLOCK:
    case TextureFormat::EAC_R11G11_UNORM_BLOCK:
    case TextureFormat::EAC_R11G11_SNORM_BLOCK:
    case TextureFormat::ASTC_4x4_UNORM_BLOCK:
    case TextureFormat::ASTC_4x4_SRGB_BLOCK:
    case TextureFormat::ASTC_5x4_UNORM_BLOCK:
    case TextureFormat::ASTC_5x4_SRGB_BLOCK:
    case TextureFormat::ASTC_5x5_UNORM_BLOCK:
    case TextureFormat::ASTC_5x5_SRGB_BLOCK:
    case TextureFormat::ASTC_6x5_UNORM_BLOCK:
    case TextureFormat::ASTC_6x5_SRGB_BLOCK:
    case TextureFormat::ASTC_6x6_UNORM_BLOCK:
    case TextureFormat::ASTC_6x6_SRGB_BLOCK:
    case TextureFormat::ASTC_8x5_UNORM_BLOCK:
    case TextureFormat::ASTC_8x5_SRGB_BLOCK:
    case TextureFormat::ASTC_8x6_UNORM_BLOCK:
    case TextureFormat::ASTC_8x6_SRGB_BLOCK:
    case TextureFormat::ASTC_8x8_UNORM_BLOCK:
    case TextureFormat::ASTC_8x8_SRGB_BLOCK:
    case TextureFormat::ASTC_10x5_UNORM_BLOCK:
    case TextureFormat::ASTC_10x5_SRGB_BLOCK:
    case TextureFormat::ASTC_10x6_UNORM_BLOCK:
    case TextureFormat::ASTC_10x6_SRGB_BLOCK:
    case TextureFormat::ASTC_10x8_UNORM_BLOCK:
    case TextureFormat::ASTC_10x8_SRGB_BLOCK:
    case TextureFormat::ASTC_10x10_UNORM_BLOCK:
    case TextureFormat::ASTC_10x10_SRGB_BLOCK:
    case TextureFormat::ASTC_12x10_UNORM_BLOCK:
    case TextureFormat::ASTC_12x10_SRGB_BLOCK:
    case TextureFormat::ASTC_12x12_UNORM_BLOCK:
    case TextureFormat::ASTC_12x12_SRGB_BLOCK:
    case TextureFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case TextureFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case TextureFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case TextureFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case TextureFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case TextureFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case TextureFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case TextureFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool FormatIsSrgb( TextureFormat format )
//-----------------------------------------------------------------------------
{
    switch (format) {
    case TextureFormat::R8_SRGB:
    case TextureFormat::R8G8_SRGB:
    case TextureFormat::R8G8B8_SRGB:
    case TextureFormat::B8G8R8_SRGB:
    case TextureFormat::R8G8B8A8_SRGB:
    case TextureFormat::B8G8R8A8_SRGB:
    case TextureFormat::A8B8G8R8_SRGB_PACK32:
    case TextureFormat::BC1_RGB_SRGB_BLOCK:
    case TextureFormat::BC1_RGBA_SRGB_BLOCK:
    case TextureFormat::BC2_SRGB_BLOCK:
    case TextureFormat::BC3_SRGB_BLOCK:
    case TextureFormat::BC7_SRGB_BLOCK:
    case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK:
    case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK:
    case TextureFormat::ETC2_R8G8B8A1_UNORM_BLOCK:
    case TextureFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
    case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK:
    case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
    case TextureFormat::EAC_R11_UNORM_BLOCK:
    case TextureFormat::EAC_R11_SNORM_BLOCK:
    case TextureFormat::EAC_R11G11_UNORM_BLOCK:
    case TextureFormat::EAC_R11G11_SNORM_BLOCK:
    case TextureFormat::ASTC_4x4_UNORM_BLOCK:
    case TextureFormat::ASTC_4x4_SRGB_BLOCK:
    case TextureFormat::ASTC_5x4_UNORM_BLOCK:
    case TextureFormat::ASTC_5x4_SRGB_BLOCK:
    case TextureFormat::ASTC_5x5_UNORM_BLOCK:
    case TextureFormat::ASTC_5x5_SRGB_BLOCK:
    case TextureFormat::ASTC_6x5_UNORM_BLOCK:
    case TextureFormat::ASTC_6x5_SRGB_BLOCK:
    case TextureFormat::ASTC_6x6_UNORM_BLOCK:
    case TextureFormat::ASTC_6x6_SRGB_BLOCK:
    case TextureFormat::ASTC_8x5_UNORM_BLOCK:
    case TextureFormat::ASTC_8x5_SRGB_BLOCK:
    case TextureFormat::ASTC_8x6_UNORM_BLOCK:
    case TextureFormat::ASTC_8x6_SRGB_BLOCK:
    case TextureFormat::ASTC_8x8_UNORM_BLOCK:
    case TextureFormat::ASTC_8x8_SRGB_BLOCK:
    case TextureFormat::ASTC_10x5_UNORM_BLOCK:
    case TextureFormat::ASTC_10x5_SRGB_BLOCK:
    case TextureFormat::ASTC_10x6_UNORM_BLOCK:
    case TextureFormat::ASTC_10x6_SRGB_BLOCK:
    case TextureFormat::ASTC_10x8_UNORM_BLOCK:
    case TextureFormat::ASTC_10x8_SRGB_BLOCK:
    case TextureFormat::ASTC_10x10_UNORM_BLOCK:
    case TextureFormat::ASTC_10x10_SRGB_BLOCK:
    case TextureFormat::ASTC_12x10_UNORM_BLOCK:
    case TextureFormat::ASTC_12x10_SRGB_BLOCK:
    case TextureFormat::ASTC_12x12_UNORM_BLOCK:
    case TextureFormat::ASTC_12x12_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}
