//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// TextureFuncts.h
//      Vulkan texture handling support

#include "texture/texture.hpp"

/// Save texture data to 'filename'
/// @returns true on success
bool SaveTextureData(const char* pFileName, TextureFormat format, int width, int height, const void* data);
