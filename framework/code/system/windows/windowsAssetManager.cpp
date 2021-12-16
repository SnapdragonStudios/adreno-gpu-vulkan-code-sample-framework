// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/// @file windowsAssetManager.cpp
/// Platform specific implementation of AssetManager class.
/// @ingroup System

#include "../assetManager.hpp"
#include "system/os_common.h"
#include <cstdio>
#include <cassert>
#include <algorithm>


//-----------------------------------------------------------------------------
// Define a class to hold the file handle pointers.
class AssetHandle
{
public:
    AssetHandle(FILE* fp, size_t fileSize)
        : mFp(fp)
        , mFileSize(fileSize)
    {}
    ~AssetHandle()
    {
        assert(mFp==nullptr);   // expecting the fp to be cleared by Fileclose()
    }
    FILE* mFp;
    const size_t mFileSize;
};


//-----------------------------------------------------------------------------
AssetHandle* AssetManager::OpenFile(const std::string& pPortableFileName)
//-----------------------------------------------------------------------------
{
    if (pPortableFileName.empty())
        return nullptr;

    // Fix the filename
    const auto deviceFilename = PortableFilenameToDevicePath(pPortableFileName);

    // Open the file and see what is to be seen
    FILE* fp = fopen(deviceFilename.c_str(), "rb");
    if (fp == nullptr)
    {
        LOGE("Unable to open file: %s (errno=%d)", deviceFilename.c_str(), (int)errno);
        return nullptr;
    }

    // Get the file length
    fseek(fp, 0, SEEK_END);
    const auto fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    return new AssetHandle(fp, fileSize);
}

//-----------------------------------------------------------------------------
size_t AssetManager::FileSize(AssetHandle* pHandle) const
//-----------------------------------------------------------------------------
{
    return pHandle->mFileSize;
}

//-----------------------------------------------------------------------------
size_t AssetManager::ReadFile(void* pDest, size_t bytes, AssetHandle* pHandle)
//-----------------------------------------------------------------------------
{
    return fread(pDest, sizeof(char), bytes, pHandle->mFp);
}

//-----------------------------------------------------------------------------
void AssetManager::CloseFile(AssetHandle* pHandle)
//-----------------------------------------------------------------------------
{
    fclose(pHandle->mFp);
    pHandle->mFp = nullptr;
    delete pHandle;
}


//-----------------------------------------------------------------------------
std::string AssetManager::PortableFilenameToDevicePath(const std::string& portableFileName)
//-----------------------------------------------------------------------------
{
    std::string output;
    output.reserve(portableFileName.length());
    std::transform(portableFileName.begin(), portableFileName.end(), std::back_inserter(output), [](char c) { return c == '/' ? '\\' : c; });
    return output;
}
