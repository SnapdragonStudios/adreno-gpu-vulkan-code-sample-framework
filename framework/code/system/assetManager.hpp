//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

//
// AssetMaanger
// Handles file loading from device storage.
// Implementations are expected to be device specific (eg in android/androidAssetManager.cpp)
#include "system/os_common.h"
#include <assert.h>
#include <istream>
#include <optional>
#include <streambuf>
#include <string>
#include <vector>

// Forward declarations
class AAssetManager;
class AssetManager;
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
        : std::istream(&mBuffer)
        , mBuffer()
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


/// @brief Wrapper around an open AssetHandle.
/// Provides a safe asset handle (will close when guard is destroyed or on EarlyClose() )
class AssetHandleGuard {
public:
    friend class AssetManager;
    AssetHandleGuard() {}
    AssetHandleGuard( AssetManager* pAssetManager, AssetHandle* pAssetHandle ) : m_AssetManager( pAssetManager ), m_AssetHandle( pAssetHandle ) { assert( (m_AssetManager != nullptr) == (m_AssetHandle != nullptr) ); }
    AssetHandleGuard( AssetHandleGuard&& src ) noexcept
    {
        *this = std::move( src );
    }
    AssetHandleGuard& operator=( AssetHandleGuard&& src ) noexcept
    {
        if (this != &src)
        {
            m_AssetManager = src.m_AssetManager;
            src.m_AssetManager = nullptr;
            m_AssetHandle = src.m_AssetHandle;
            src.m_AssetHandle = nullptr;
        }
        return *this;
    }
    ~AssetHandleGuard()
    {
        Release();
    }
    void EarlyRelease()
    {
        Release();
    }
    explicit operator bool() const { return m_AssetHandle != nullptr; }
private:
    AssetHandleGuard( const AssetHandleGuard& ) = delete;
    AssetHandleGuard& operator=( const AssetHandleGuard& ) = delete;
    void Release();
    AssetManager* m_AssetManager = nullptr;
    AssetHandle* m_AssetHandle = nullptr;
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
    friend class AssetHandleGuard;

    /// Set the pointer to Android AssetManager.  On non Android plaforms this pointer is not used.
    void SetAAssetManager(AAssetManager* pAAssetManager) { m_AAssetManager = pAAssetManager; }

    /// Set the location of the external files directory.  On non Android plaforms this string is not used.
    void SetAndroidExternalFilesDir(const std::string& s) { m_AndroidExternalFilesDir = s; }

    /// Load the contents of the given file in to a given storage type.
    /// NOT zero padded (by default) but can be used with a std::string to correctly handle termination.
    /// @tparam T_Container type of container (eg std::vector<char> or std::string)
    /// @param fileData output data
    /// @return true if successful
    template<typename T_Container>
    bool LoadFileIntoMemory(const std::string& portableFileName, T_Container& fileData)
    {
        AssetHandle* handle = OpenFile(portableFileName, Mode::Read);
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

    size_t GetFileSize(const std::string& portableFileName) 
    {
        AssetHandle* handle = OpenFile(portableFileName, Mode::Read);
        if (!handle)
        {
            return 0;
        }

        return FileSize(handle);
    }

    AssetHandleGuard OpenFile( const std::string& portableFilename )
    {
        auto* fileHandle = OpenFile( portableFilename, Mode::Read );
        if (fileHandle)
            return { this, fileHandle };
        else
            return {};
    }

    /// @brief Read data from given file (handle) into supplied buffer
    /// @param file open file handle
    /// @param pDestination pointer to memory location where data should be copied into
    /// @param maxBytesToRead maximum number of bytes to read into pDestination
    /// @return number of bytes read (0 on error)
    size_t ReadFile( const AssetHandleGuard& file, void* pDestination, size_t maxBytesToRead)
    {
        assert( pDestination );
        uint8_t* pData = (uint8_t*) pDestination;
        size_t totalBytesRead = 0;
        while (maxBytesToRead > 0)
        {
            size_t bytesRead = ReadFile( pData, maxBytesToRead, file.m_AssetHandle );
            if (bytesRead > 0)
            {
                pData += bytesRead;
                maxBytesToRead -= bytesRead;
                totalBytesRead += bytesRead;
            }
            else
            {
                LOGE( "ReadFile error");
                break;
            }
        }
        return totalBytesRead;
    }

    /// Save the contents of the given storage container to a named file.
    /// @tparam T_Container type of container (eg std::vector<char> or std::string)
    /// @param fileData saved data
    /// @return true if successful
    template<typename T_Container>
    bool SaveMemoryToFile(const std::string& portableFileName, const T_Container& fileData)
    {
        AssetHandle* handle = OpenFile(portableFileName, Mode::Write);
        if (!handle)
        {
            return false;
        }

        char* pData = (char*)fileData.data();
        size_t bytesToWrite = fileData.size();
        while (bytesToWrite > 0)
        {
            size_t bytesWritten = WriteFile(pData, bytesToWrite, handle);
            if (bytesWritten > 0)
            {
                pData += bytesWritten;
                bytesToWrite -= bytesWritten;
            }
            else
            {
                LOGE("SaveMemoryToFile error in WriteFile saving %s", portableFileName.c_str());
                break;
            }
        }

        CloseFile(handle);
        if (bytesToWrite != 0)
        {
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
    enum class Mode { Read, Write };
    AssetHandle* OpenFile(const std::string& pPortableFileName, Mode);
    size_t FileSize(AssetHandle*) const;
    size_t ReadFile(void* pDest, size_t bytes, AssetHandle* pHandle);
    size_t WriteFile(const void* psrc, size_t bytes, AssetHandle* pHandle);
    void CloseFile(AssetHandle*);

    std::string PortableFilenameToDevicePath(const std::string& pPortableFileName);

private:
    AAssetManager* m_AAssetManager = nullptr;
    std::string m_AndroidExternalFilesDir;
    std::vector<AssetHandle*> m_OpenHandles;    // Managed by platform implementation
};


inline void AssetHandleGuard::Release()
{
    if (m_AssetManager)
    {
        assert( m_AssetHandle );
        m_AssetManager->CloseFile( m_AssetHandle );
        m_AssetHandle = nullptr;
        m_AssetManager = nullptr;
    }
    assert( m_AssetHandle == nullptr );
}
