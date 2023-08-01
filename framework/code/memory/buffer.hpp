//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <utility>
#include "memory.hpp"
#include "memoryMapped.hpp"

// Forward declarations
struct AHardwareBuffer_Desc;
struct AHardwareBuffer;
template<typename T_GFXAPI> class MemoryManager;
template<typename T_GFXAPI> class MemoryCpuMappedUntyped;
template<typename T_GFXAPI> class BufferT;
template<typename T_GFXAPI, typename T_BUFFERTYPE> class MemoryCpuMapped;


/// @brief Base class (virtual) for memory buffers.  Platform agnostic.
class Buffer
{
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
protected:
    Buffer() noexcept {};
public:
    virtual ~Buffer() {}
    virtual void Destroy() = 0;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;
};


template<typename T_GFXAPI, typename T>
class MapGuard : public MemoryCpuMapped<T_GFXAPI, T>
{
    MapGuard(const MapGuard&) = delete;
    MapGuard& operator=(const MapGuard&) = delete;
    MapGuard& operator=(MapGuard&& other) = delete;
public:
    MapGuard( BufferT< T_GFXAPI>& buffer, MemoryCpuMapped<T_GFXAPI, T> mapped)
        : MemoryCpuMapped<T_GFXAPI, T>(std::move(mapped))	// Only ever one owner of a MemoryCpuMapped
        , mBuffer(&buffer)						// not passed as a pointer so we know it's never initialized null
    {}
    ~MapGuard()
    {
        if (mBuffer)	// nullptr if EarlyUnmap'ed
        {
            mBuffer->Unmap(std::move(*this));
        }
    }
    void EarlyUnmap()
    {
        assert(mBuffer);	// error on second call to EarlyUnmap
        mBuffer->Unmap(std::move(*this));
        mBuffer = nullptr;
    }
    MapGuard(MapGuard&& other)
        : MemoryCpuMapped<T_GFXAPI, T>(std::move(other))
        , mBuffer(other.mBuffer)
    {
        other.mBuffer = nullptr;
    }
private:
    BufferT< T_GFXAPI>* mBuffer;
};


/// Defines a simple templated (gfx api agnostic) object for creating and holding graphics memory buffer objects.
/// Expected for only the specialized version to be used!
/// @ingroup Memory
template<typename T_GFXAPI>
class BufferT : public Buffer
{
    BufferT& operator=(const BufferT&) = delete;
    BufferT(const BufferT&) = delete;
protected:
public:
    BufferT() noexcept = delete;                    // this template class must be specialized
    virtual ~BufferT() = delete;                    // this template class must be specialized
    BufferT(BufferT&&) noexcept = delete;           // this template class must be specialized
    BufferT& operator=(BufferT&&) noexcept = delete;// this template class must be specialized

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy() override = delete;               // this template class must be specialized

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T>
    MapGuard<T_GFXAPI, T> Map() = delete;           // this template class must be specialized

    static_assert(sizeof(BufferT<T_GFXAPI>) != sizeof(Buffer));   // Ensure this class template is specialized (and not used as-is)
};

