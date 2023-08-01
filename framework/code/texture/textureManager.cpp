//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "system/assetManager.hpp"
#include "textureManager.hpp"
#include "loaderKtx.hpp"
#include "loaderPpm.hpp"
#include <queue>

TextureManager::TextureManager() noexcept
{
}

TextureManager::~TextureManager()
{
    Release();
}

bool TextureManager::Initialize()
{
    if (m_LoadingThreadWorker.Initialize("TextureThreadWorker", 4) <= 0)
        return false;
    return m_Loader->Initialize();
}

void TextureManager::Release()
{
    m_LoadingThreadWorker.Terminate();
}

std::unique_ptr<Texture> TextureManager::CreateTextureObject( GraphicsApiBase& gfxApi, uint32_t Width, uint32_t Height, TextureFormat Format, TEXTURE_TYPE TexType, const char* pName, uint32_t Msaa, TEXTURE_FLAGS Flags )
{
    CreateTexObjectInfo createInfo{};
    createInfo.uiWidth = Width;
    createInfo.uiHeight = Height;
    createInfo.Format = Format;
    createInfo.TexType = TexType;
    createInfo.Flags = Flags;
    createInfo.pName = pName;
    createInfo.Msaa = Msaa;
    return CreateTextureObject( gfxApi, createInfo );
}
