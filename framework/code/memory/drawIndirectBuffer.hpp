//============================================================================================================
//
//
//                  Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "buffer.hpp"

/// Templated (by graphics api) buffer containing draw indirect commands
/// @ingroup Memory
template<typename T_GFXAPI>
class DrawIndirectBuffer: public Buffer<T_GFXAPI>
{
    DrawIndirectBuffer(const DrawIndirectBuffer&) = delete;
    DrawIndirectBuffer& operator=(const DrawIndirectBuffer&) = delete;
public:
    enum class eType {
        Draw,
        IndexedDraw,
        MeshTasks
    };

    DrawIndirectBuffer(DrawIndirectBuffer::eType indirectBufferType) noexcept;
    DrawIndirectBuffer(DrawIndirectBuffer&&) noexcept;
    DrawIndirectBuffer& operator=(DrawIndirectBuffer&&) noexcept;
    ~DrawIndirectBuffer();

    /// Buffer initialization with a prequel block (and then a block of type T_DRAW data for each potential draw)
    template<typename T_DRAW, typename T_PREQUEL> bool Initialize(MemoryManager<T_GFXAPI>* pManager, size_t numDraws, const T_DRAW* initialData, const T_PREQUEL* initialPrequelData, const BufferUsageFlags usage = BufferUsageFlags::Indirect);
    /// Buffer initialization with a block of type T_DRAW data for each potential draw
    template<typename T_DRAW> bool Initialize(MemoryManager<T_GFXAPI>* pManager, size_t numDraws, const T_DRAW* initialData, const BufferUsageFlags usage = BufferUsageFlags::Indirect);

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy();

    /// create a copy of this index buffer (including a new copy of the data)
    /// @param newUsageFlags make the copy with new usage flags (default to copying the source usage flags)
    DrawIndirectBuffer Copy(BufferUsageFlags newUsageFlags = BufferUsageFlags::Unknown);

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return mPrequelBytes + GetDrawCommandBytes() * mNumDraws; }

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T> MapGuard<T_GFXAPI, T> Map();

    //bool IsIndexed() const { return mIndexed; }
    eType GetIndirectBufferType() const { return mIndirectBufferType; }
    uint32_t GetDrawCommandBytes() const;
    auto GetNumDraws() const { return mNumDraws; }
    auto GetBufferOffset() const { return mPrequelBytes; }

protected:
    MapGuard<T_GFXAPI, void> MapVoid();
    bool Initialize(MemoryManager<T_GFXAPI>* pManager, size_t numDraws, const void* initialData, const BufferUsageFlags usage, const void* prequelData, uint32_t prequelBytes);

private:
    size_t                      mNumDraws = 0;
    uint32_t                    mPrequelBytes = 0;   // buffer bytes before the index data begins.
    const eType                 mIndirectBufferType;
};


template<typename T_GFXAPI>
DrawIndirectBuffer<T_GFXAPI>::DrawIndirectBuffer(eType indirectBufferType) noexcept : Buffer<T_GFXAPI>(), mIndirectBufferType(indirectBufferType)
{}

template<typename T_GFXAPI>
DrawIndirectBuffer<T_GFXAPI>::DrawIndirectBuffer(DrawIndirectBuffer&& other) noexcept : mIndirectBufferType(other.mIndirectBufferType )
{
    *this = std::move(other);
}

template<typename T_GFXAPI>
DrawIndirectBuffer<T_GFXAPI>& DrawIndirectBuffer<T_GFXAPI>::operator=(DrawIndirectBuffer&& other) noexcept
{
    Buffer<T_GFXAPI>::operator=(std::move(other));
    if (&other != this)
    {
        assert(mIndirectBufferType == other.mIndirectBufferType);
        mNumDraws = other.mNumDraws;
        other.mNumDraws = 0;
        mPrequelBytes = other.mPrequelBytes;
        other.mPrequelBytes = 0;
    }
    return *this;
}

template<typename T_GFXAPI>
DrawIndirectBuffer<T_GFXAPI>::~DrawIndirectBuffer() {}

template<typename T_GFXAPI> template<typename T>
bool DrawIndirectBuffer<T_GFXAPI>::Initialize(MemoryManager<T_GFXAPI>* pManager, size_t numDraws, const T* initialData, const BufferUsageFlags usage)
{
    assert(sizeof(T) == GetDrawCommandBytes());
    return Initialize(pManager, numDraws, (const void*)initialData, usage, nullptr, 0);
}

template<typename T_GFXAPI> template<typename T_DRAW, typename T_PREQUEL>
bool DrawIndirectBuffer<T_GFXAPI>::Initialize(MemoryManager<T_GFXAPI>* pManager, size_t numDraws, const T_DRAW* initialData, const T_PREQUEL* prequelData, const BufferUsageFlags usage)
{
    assert(sizeof(T_DRAW) == GetDrawCommandBytes());
    return Initialize(pManager, numDraws, (const void*)initialData, usage, prequelData, sizeof(T_PREQUEL));
}

template<typename T_GFXAPI>
bool DrawIndirectBuffer<T_GFXAPI>::Initialize(MemoryManager<T_GFXAPI>* pManager, size_t numDraws, const void* initialData, const BufferUsageFlags usage, const void* prequelData, uint32_t prequelBytes )
{
    mNumDraws = numDraws;
    mPrequelBytes = prequelBytes;

    MemoryUsage memoryUsage = initialData ? MemoryUsage::CpuToGpu : MemoryUsage::GpuExclusive;
    if ((usage & BufferUsageFlags::TransferSrc) != 0)
        memoryUsage = MemoryUsage::CpuToGpu;

    if (!Buffer<T_GFXAPI>::Initialize( pManager, GetDrawCommandBytes() * mNumDraws + mPrequelBytes, usage, memoryUsage ))
    {
        return false;
    }

    if (initialData)
    {
        auto mappedGuard = Buffer<T_GFXAPI>::template Map<uint8_t>();
        auto initialDataSize = GetDrawCommandBytes() * mNumDraws;
        if (prequelBytes)
            memcpy( mappedGuard.data(), prequelData, prequelBytes );
        else
            memset( mappedGuard.data(), 0, prequelBytes );
        memcpy( mappedGuard.data() + prequelBytes, initialData, initialDataSize );
    }
    return true;
}

template<typename T_GFXAPI>
void DrawIndirectBuffer<T_GFXAPI>::Destroy()
{
    mNumDraws = 0;
    Buffer<T_GFXAPI>::Destroy();
}

template<typename T_GFXAPI>
DrawIndirectBuffer<T_GFXAPI> DrawIndirectBuffer<T_GFXAPI>::Copy( BufferUsageFlags newUsageFlags )
{
    if (newUsageFlags == BufferUsageFlags::Unknown)
        newUsageFlags = Buffer<T_GFXAPI>::template mBufferUsageFlags<>;
    DrawIndirectBuffer copy( GetIndirectBufferType() );
    auto data = MapVoid();
    if (copy.Initialize( Buffer<T_GFXAPI>::template mManager<>, GetNumDraws(), static_cast<std::byte*>(data.data()) + mPrequelBytes, newUsageFlags, data.data(), mPrequelBytes ))
    {
        return copy;
    }
    else
    {
        return {GetIndirectBufferType()};
    }
}

template<typename T_GFXAPI> template<typename T>
MapGuard<T_GFXAPI, T> DrawIndirectBuffer<T_GFXAPI>::Map()
{
    assert( mPrequelBytes == 0 );  // prequel bytes not yet supported in map
    assert( sizeof( T ) == GetDrawCommandBytes() );
    return Buffer<T_GFXAPI>::template Map<T>();
}

template<typename T_GFXAPI>
MapGuard<T_GFXAPI, void> DrawIndirectBuffer<T_GFXAPI>::MapVoid()
{
    return Buffer<T_GFXAPI>::template Map<void>();
}
