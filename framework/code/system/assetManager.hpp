// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

//
// AssetMaanger
// Handles file loading from device storage.
// Implementations are expected to be device specific (eg in android/androidAssetManager.cpp)
#include "system/os_common.h"
#include <string>
#include <vector>
#include <istream>
#include <streambuf>
#include <optional>

// Forward declarations
class AAssetManager;
class AssetHandle;

/// Implement std::basic_istream wrapper that contains a fixed size data buffer.
/// Can be filled with AssetManager::LoadFileIntoMemory and then passed in to functions that
/// read from streams (eg functions taking std::istream)
/// @ingroup System
template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class AssetMemStream : public std::basic_istream<CharT, TraitsT> {
    AssetMemStream(const AssetMemStream&) = delete;
    AssetMemStream& operator=(const AssetMemStream&) = delete;
public:
    AssetMemStream()
        : mBuffer()
        , std::istream(&mBuffer)
    {
        this->set_rdbuf(&mBuffer);
    }

    void clear() { mBuffer.clear();  }
    void resize(size_t newSize) { mBuffer.resize(newSize); }
    const CharT* data() const noexcept { return mBuffer.data(); }
    size_t size() const noexcept { return mBuffer.size(); }
    bool empty() const noexcept { return mBuffer.empty(); }

private:
    class AssetStreamBuffer : public std::basic_streambuf<CharT, TraitsT> {
    public:
        // Create stream buffer and take ownership of the passed in pointer.
        AssetStreamBuffer()
        {
            this->setg(mData.data(), mData.data(), mData.data() + mData.size());
        }

        void clear()
        {
            mData = std::move(std::vector<CharT>());
            this->setg(mData.data(), mData.data(), mData.data() + mData.size());
        }
        void resize(size_t newSize)
        {
            mData.resize(newSize);
            this->setg(mData.data(), mData.data(), mData.data() + mData.size());
        }
        const CharT* data() const noexcept { return mData.data(); }
        size_t size() const noexcept { return mData.size(); }
        bool empty() const noexcept { return mData.empty(); }

        std::vector<CharT> mData;
    } mBuffer;
};


/// Handles file loading from device storage.
/// Implementations are expected to be device specific (eg in android/androidAssetManager.cpp)
/// @ingroup System
class AssetManager
{
    AssetManager& operator=(const AssetManager&) = delete;
    AssetManager(const AssetManager&) = delete;
public:
    AssetManager() {}

    /// Set the poiner to Android AssetManager.  On non Android plaforms this pointer is not used.
    void SetAAssetManager(AAssetManager* pAAssetManager) { m_AAssetManager = pAAssetManager; }

    /// Load the contents of the given file in to a given storage type.
    /// NOT zero padded (by default) but can be used with a std::string to correctly handle termination.
    /// @tparam T_Container type of container (eg std::vector<char> or std::string)
    /// @param fileData output data
    /// @return true if successful
    template<typename T_Container>
    bool LoadFileIntoMemory(const std::string& portableFileName, T_Container& fileData)
    {
        AssetHandle* handle = OpenFile(portableFileName);
        if (!handle)
        {
            return false;
        }

        fileData.clear();
        size_t bytesToRead = FileSize(handle);
        fileData.resize(bytesToRead);

        char* pData = (char*)fileData.data();
        while (bytesToRead > 0)
        {
            size_t bytesRead = ReadFile(pData, bytesToRead, handle);
            if (bytesRead > 0)
            {
                pData += bytesRead;
                bytesToRead -= bytesRead;
            }
            else
            {
                LOGE("LoadFileIntoMemory error in ReadFile loading %s", portableFileName.c_str());
                break;
            }
        }

        CloseFile(handle);
        if (bytesToRead != 0)
        {
            fileData.clear();
            return false;
        }
        return true;
    }

    /// Join together two file paths
    inline std::string JoinPath(const std::string& a, const std::string& b) const
    {
        if (a.empty() || a.back() == '\\' || a.back() == '/')
            return a + b;
        else
            return a + std::string("/") + b;
    }

    /// Extract the directory name from a file path
    inline std::string ExtractDirectory(const std::string& filename) const
    {
        const auto o = filename.find_last_of("/\\");
        if (o == std::string::npos)
        {
            return {};
        }
        return filename.substr(0, o);
    }

    // Functions implemented by the platform
public:
    ~AssetManager() {}
protected:
    AssetHandle* OpenFile(const std::string& pPortableFileName);
    size_t FileSize(AssetHandle*) const;
    size_t ReadFile(void* pDest, size_t bytes, AssetHandle* pHandle);
    void CloseFile(AssetHandle*);

    static std::string PortableFilenameToDevicePath(const std::string& pPortableFileName);

private:
    AAssetManager* m_AAssetManager = nullptr;
    std::vector<AssetHandle*> m_OpenHandles;    // Managed by platform implementation
};
