//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

// TextureFuncts.cpp
//      Vulkan texture handling support

#include "texture/textureFormat.hpp"
#include "texture/vulkan/texture.hpp"
#include "vulkan/vulkan.hpp"
#include <cinttypes>
#define STB_IMAGE_IMPLEMENTATION
#include "tinygltf/stb_image.h"
#include "tinygltf/stb_image_write.h"

//-----------------------------------------------------------------------------
bool SaveTextureData( const char* pFileName, TextureFormat format, int width, int height, const void* data )
//-----------------------------------------------------------------------------
{
    int bytesPerTexel = 1;
    int components = 0;
    switch (format)
    {
    case TextureFormat::R8G8B8A8_SRGB:
        components = 4;
        break;
    case TextureFormat::R8G8B8_SRGB:
        components = 3;
        break;
    case TextureFormat::R8G8_SRGB:
    case TextureFormat::R8G8_UNORM:
        components = 2;
        break;
    case TextureFormat::R8_SRGB:
    case TextureFormat::R8_UNORM:
        components = 1;
        break;
    default:
        LOGE( "SaveTextureData: format %s not supported for output", Vulkan::VulkanFormatString( TextureFormatToVk(format) ) );
        return false;
    }

    {
        int error = stbi_write_png( pFileName, width, height, components, data, width * components * bytesPerTexel );
        return (error != 0);
    }
}
