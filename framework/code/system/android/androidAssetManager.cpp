//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

/// @file androidAssetManager.cpp
/// Platform specific implementation of AssetManager class.
/// @ingroup System

#include "../assetManager.hpp"
#include "system/os_common.h"
#include <android/asset_manager.h>
#include <cstdio>

//-----------------------------------------------------------------------------
// Define a class to hold the file handle pointers.
class AssetHandle
{
public:
    AssetHandle(FILE* fp, size_t fileSize)
        : mFp(fp)
        , mAAsset(nullptr)
        , mFileSize(fileSize)
    {}
    AssetHandle(AAsset* aa, size_t fileSize)
        : mFp(nullptr)
        , mAAsset(aa)
        , mFileSize(fileSize)
    {}
    ~AssetHandle()
    {
        assert(mFp == nullptr);   // expecting the fp to be cleared by Fileclose()
        assert(mAAsset == nullptr);   // expecting the pAAsset to be cleared by Fileclose()
    }
protected:
    friend class AssetManager;
    FILE* mFp;
    AAsset* mAAsset;
    size_t mFileSize;
};

//-----------------------------------------------------------------------------
AssetHandle* AssetManager::OpenFile(const std::string& portableFilename, Mode mode)
//-----------------------------------------------------------------------------
{
    if (portableFilename.empty())
        return nullptr;

    std::vector<char> fileBuffer;

    //
    // Attempt to load/save from storage.

    // Fix the filename
    const auto deviceFilename = PortableFilenameToDevicePath(portableFilename);

    // Open the file and see what is to be seen
    switch (mode) {
    case Mode::Read:
    {
        FILE* fp = fopen(deviceFilename.c_str(), "rb");
        if (fp != nullptr)
        {
            // Get the file length
            fseek(fp, 0, SEEK_END);
            const auto fileSize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            return new AssetHandle(fp, fileSize);
        }
        else
        {
            //
            // Fall back to using AAssetManager (attempt to open file inside the apk)
            //LOGE("Unable to open file %s attempting to load from apk", deviceFilename.c_str());

            // Asset name needs to have / seperators and no ./ preamble!
            std::string aAssetFilename;
            const auto skipPreambleOffset = std::max(portableFilename.find_first_not_of("./\\"), (size_t)0);
            std::transform(portableFilename.begin() + skipPreambleOffset, portableFilename.end(), std::back_inserter(aAssetFilename), [](char c) { return c == '\\' ? '/' : c; });

            AAsset* pAAsset = AAssetManager_open(m_AAssetManager, aAssetFilename.c_str(), AASSET_MODE_STREAMING);
            if (pAAsset != nullptr)
            {
                // Get the file length
                const size_t fileSize = AAsset_getLength(pAAsset);

                return new AssetHandle(pAAsset, fileSize);
            }
        }
        break;
    }
    case Mode::Write:
    {
        // Mode::Write
        FILE* fp = fopen(deviceFilename.c_str(), "wb");
        if (fp != nullptr)
        {
            return new AssetHandle(fp, 0);
        }
        break;
    }
    }

    LOGE("Unable to open file %s", portableFilename.c_str());
    return nullptr;
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
    size_t bytesRead;
    if (pHandle->mFp)
    {
        // Read from file handle.
        bytesRead = fread(pDest, sizeof(char), bytes, pHandle->mFp);
    }
    else if (pHandle->mAAsset)
    {
        // Read from android asset.
        bytesRead = AAsset_read(pHandle->mAAsset, pDest, bytes);
    }
    else
    {
        return -1;
    }
    return bytesRead;
}

//-----------------------------------------------------------------------------
size_t AssetManager::WriteFile(const void* pSrc, size_t bytes, AssetHandle* pHandle)
//-----------------------------------------------------------------------------
{
    size_t bytesWritten = 0;
    if (pHandle->mFp)
    {
        // Write to file handle.
        size_t ret;
        do {
            ret = fwrite(pSrc, sizeof(char), bytes, pHandle->mFp);
            if (ret > 0)
                bytesWritten += ret;
        } while (ret > 0);
    }
    else if (pHandle->mAAsset)
    {
        // does not support write
        assert(0);
        return -1;
    }
    else
    {
        return -1;
    }
    pHandle->mFileSize += bytesWritten;
    return bytesWritten;
}

//-----------------------------------------------------------------------------
void AssetManager::CloseFile(AssetHandle* pHandle)
//-----------------------------------------------------------------------------
{
    if (pHandle->mFp)
        fclose(pHandle->mFp);
    pHandle->mFp = nullptr;
    if (pHandle->mAAsset)
        AAsset_close(pHandle->mAAsset);
    pHandle->mAAsset = nullptr;
    delete pHandle;
}

//-----------------------------------------------------------------------------
std::string AssetManager::PortableFilenameToDevicePath(const std::string& portableFilename)
//-----------------------------------------------------------------------------
{
    std::string output;
    output.reserve(28 + portableFilename.length() + strlen(gpAndroidAppName) + 2);
    output.assign(m_AndroidExternalFilesDir);
    if (output.back() != '/')
        output.push_back('/');
    std::transform(portableFilename.begin(), portableFilename.end(), std::back_inserter(output), [](char c) { return c == '\\' ? '/' : c; });
    return output;
}
