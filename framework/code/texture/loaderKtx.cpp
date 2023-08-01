//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include <ktx.h>  // KTX-Software
#include <memory>
#include <cassert>
#include "system/assetManager.hpp"
#include "loaderKtx.hpp"
#include "../lib/gl_format.h"

TextureKtxFileWrapper::~TextureKtxFileWrapper() noexcept
{
    Release();
}
TextureKtxFileWrapper::TextureKtxFileWrapper(TextureKtxFileWrapper&& other) noexcept : m_fileData(std::move(other.m_fileData))
{
    std::swap(m_ktxTexture, other.m_ktxTexture);
}
TextureKtxFileWrapper& TextureKtxFileWrapper::operator=(TextureKtxFileWrapper&& other) noexcept
{
    if (this != &other)
    {
        assert(m_ktxTexture==nullptr);
        this->m_ktxTexture = other.m_ktxTexture;
        other.m_ktxTexture = nullptr;
        this->m_fileData = std::move(other.m_fileData);
    }
    return *this;
}

void TextureKtxFileWrapper::Release()
{
    if (m_ktxTexture)
        ktxTexture_Destroy(m_ktxTexture);
    m_ktxTexture = nullptr;
}

TextureKtx::~TextureKtx() noexcept
{
    Release();
}

bool TextureKtx::Initialize()
{
    return true;
}

TextureKtxFileWrapper TextureKtx::LoadFile(AssetManager& assetManager, const char* const pFileName) const
{
    std::vector<uint8_t> fileData;
    if (!assetManager.LoadFileIntoMemory(pFileName, fileData))
    {
        LOGE("Error reading texture file: %s", pFileName);
        return {};
    }

    ///HACK: some of our ktx files have gl internal format and gl format set to be the same thing, which is the ktx library doesnt like.
    struct KtxHeader {
        uint8_t   identifier[12];
        uint32_t  endianness;
        uint32_t  glType;
        uint32_t  glTypeSize;
        uint32_t  glFormat;
        uint32_t  glInternalFormat;
        uint32_t  glBaseInternalFormat;
        uint32_t  pixelWidth;
        uint32_t  pixelHeight;
        uint32_t  pixelDepth;
        uint32_t  numberOfArrayElements;
        uint32_t  numberOfFaces;
        uint32_t  numberOfMipmapLevels;
        uint32_t  bytesOfKeyValueData;
    };
    KtxHeader* pHeader = (KtxHeader*)fileData.data();

    ktx_uint8_t ktx_identifier[] = KTX_IDENTIFIER_REF;
    if (fileData.size() >= sizeof(KtxHeader) && memcmp(pHeader->identifier, ktx_identifier, sizeof(ktx_identifier)) == 0)
    {
        const auto glFormat = pHeader->glFormat;
        const auto glType = pHeader->glType;

        // Internal format should be sized but some exporters write it as untyped, see if we can fix that!
        if ((glFormat == pHeader->glInternalFormat) && glFormat != 0)
        {
            if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RGB)
            {
                pHeader->glInternalFormat = GL_RGB8;
            }
            else if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RED)
            {
                pHeader->glInternalFormat = GL_R8;
            }
            else if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RGBA)
            {
                //pHeader->glInternalFormat = GL_SRGB8_ALPHA8_EXT;
                pHeader->glInternalFormat = GL_RGBA8;
            }
        }
    }
    ///ENDHACK

    TextureKtxFileWrapper textureData {std::move(fileData)};
    if (KTX_SUCCESS != ktxTexture_CreateFromMemory(textureData.m_fileData.data(), textureData.m_fileData.size(), KTX_TEXTURE_CREATE_NO_FLAGS, &textureData.m_ktxTexture))
        return {};
    return textureData;
}
