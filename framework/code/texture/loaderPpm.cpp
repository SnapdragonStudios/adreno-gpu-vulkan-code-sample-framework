//============================================================================================================
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include <memory>
#include <cassert>
#include "system/assetManager.hpp"
#include "loaderPpm.hpp"
#include "texture.hpp"

TexturePpmFileWrapper::TexturePpmFileWrapper(TexturePpmFileWrapper&& other) noexcept : m_Data(std::move(other.m_Data))
{
//    std::swap(m_ppmTexture, other.m_ppmTexture);
}
TexturePpmFileWrapper& TexturePpmFileWrapper::operator=(TexturePpmFileWrapper&& other) noexcept
{
    if (this != &other)
    {
        //assert(m_ppmTexture==nullptr);
        //this->m_ppmTexture = other.m_ppmTexture;
        //other.m_ppmTexture = nullptr;
        this->m_Data = std::move(other.m_Data);
    }
    return *this;
}

void TexturePpmFileWrapper::Release()
{
    m_Data = {};
    //if (m_ppmTexture)
    //    ppmTexture_Destroy(m_ppmTexture);
    //m_ppmTexture = nullptr;
}

TexturePpmBase::~TexturePpmBase() noexcept
{
    Release();
}

bool TexturePpmBase::Initialize()
{
    return true;
}

TexturePpmFileWrapper TexturePpmBase::LoadFile( AssetManager& assetManager, const char* const pFileName ) const
{
    std::vector<uint8_t> fileData;
    if (!assetManager.LoadFileIntoMemory( pFileName, fileData ))
    {
        LOGE( "Error reading texture file: %s", pFileName );
        return {};
    }

    if (fileData.size() < 20)
        return {};
    auto fileDataIt = fileData.begin();
    if (*fileDataIt++ != 'P' || *fileDataIt++ != '6' || !isspace( *fileDataIt++ ))
        return {};
    while (isspace( *fileDataIt ))
        ++fileDataIt;
    uint32_t width = 0, height = 0, maxColor = 0;
    while (isdigit( *fileDataIt ))
        width = width * 10 + uint32_t( *fileDataIt++ - '0' );
    while (isspace( *fileDataIt ))
        ++fileDataIt;
    while (isdigit( *fileDataIt ))
        height = height * 10 + uint32_t( *fileDataIt++ - '0' );
    while (isspace( *fileDataIt ))
        ++fileDataIt;
    while (isdigit( *fileDataIt ))
        maxColor = maxColor * 10 + uint32_t( *fileDataIt++ - '0' );
    uint32_t bytesPerPPMPixel = maxColor < 256 ? 1 : 2;
    if (!isspace( *fileDataIt++ ))
        return {};
    // Images follow.
    size_t singleImageBytes = width * height * 3 * bytesPerPPMPixel;
    // If there are multiple images treat this as a 3d texture.
    size_t dataOffset = fileDataIt - fileData.begin();
    size_t dataBytes = fileData.size() - dataOffset;
    uint32_t depth = (uint32_t)( dataBytes / singleImageBytes );
    // Copy image data in to correct format for copying in to vulkan texture.

    TextureFormat format = TextureFormat::R8G8_UNORM;
    size_t bytesPerPixel = 2;
    std::vector<uint8_t> targetData;
    targetData.resize( width * height * depth * bytesPerPixel );
    if (bytesPerPixel == 1)
    {
        for (auto& target : targetData)
        {
            target = *fileDataIt;
            fileDataIt += 3;
        }
    }
    else
    {
        auto targetIt = targetData.begin();
        for (size_t i = 0; i < width * height * depth; ++i)
        {
            *targetIt = *fileDataIt;
            ++targetIt;
            ++fileDataIt;
            *targetIt = *fileDataIt;
            ++targetIt;
            ++fileDataIt;
            if (bytesPerPixel > 2)
            {
                *targetIt = *fileDataIt;
                ++targetIt;
            }
            if (bytesPerPixel > 3)
            {
                *targetIt = 255;   // no alpha in ppm
                ++targetIt;
            }
            ++fileDataIt;
        }
    }

    TexturePpmFileWrapper ppmData{ width, height, depth, format, std::move(targetData) };
    return ppmData;
}
