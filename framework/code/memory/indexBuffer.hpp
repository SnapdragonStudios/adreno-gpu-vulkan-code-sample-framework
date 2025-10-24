//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "buffer.hpp"
#include <span>


/// Templated (by graphics api) buffer containing vertex indices
/// DO NOT specialize this template (use IndexBuffer for that)
/// @ingroup Memory
template<typename T_GFXAPI>
class IndexBufferT: public Buffer<T_GFXAPI>
{
    IndexBufferT(const IndexBufferT&) = delete;
    IndexBufferT& operator=(const IndexBufferT&) = delete;
public:
    IndexBufferT(IndexType) noexcept;
    //IndexBuffer(VkIndexType) noexcept;
    IndexBufferT(IndexBufferT&&) noexcept;
    IndexBufferT& operator=(IndexBufferT&&) noexcept;
    ~IndexBufferT();
    using MemoryManager = MemoryManager<T_GFXAPI>;

    /// Initialization
    template<typename T> bool Initialize( MemoryManager* pManager, const std::span<const T> initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Index);

    /// Initialization
    bool Initialize( MemoryManager* pManager, size_t numIndices, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Index );

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy();

    /// create a copy of this index buffer (including a new copy of the data)
    IndexBufferT Copy();

    /// Create a copy of this buffer with (potentially) different usage.  Will add vkCmdCopyBuffer commands to the provided 'copy' command buffer (caller's resposibility to execute this command list before either of the BufferObjects are destroyed).
    //IndexBufferT Copy(VkCommandBuffer copyCommandBuffer, BufferUsageFlags bufferUsage = BufferUsageFlags::Index | BufferUsageFlags::TransferDst, MemoryUsage memoryUsage = MemoryUsage::GpuExclusive ) const;

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return GetIndexTypeBytes() * mNumIndices; }

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T> MapGuard<T_GFXAPI, T> Map();

    auto GetIndexType() const { return mIndexType; }
    uint32_t GetIndexTypeBytes() const { switch (mIndexType) { case IndexType::IndexU8: return 1; case IndexType::IndexU16: return 2; case IndexType::IndexU32: return 4; default: assert(0); return 1; } }
    auto GetNumIndices() const { return mNumIndices; }

protected:
    MapGuard<T_GFXAPI, void> MapVoid();
    bool Initialize(MemoryManager* pManager, size_t numIndices, const void* initialData, const bool dspUsable, const BufferUsageFlags usage);

protected:
    size_t mNumIndices = 0;
    const IndexType mIndexType;
    bool mDspUsable = false;
    //std::unique_ptr<MappableBuffer> mDspBuffer;
};


/// @brief IndexBuffer class that should be specialized to implement GFXAPI specific functionality.
/// @tparam T_GFXAPI 
template<typename T_GFXAPI>
class IndexBuffer : public IndexBufferT<T_GFXAPI>
{
public:
    IndexBuffer(IndexType i) noexcept : IndexBufferT<T_GFXAPI>(i) {}
    //IndexBuffer(VkIndexType) noexcept;
    IndexBuffer(IndexBuffer<T_GFXAPI>&& o) noexcept : IndexBufferT<T_GFXAPI>(std::move(o)) {}
    IndexBuffer<T_GFXAPI>& operator=(IndexBuffer<T_GFXAPI>&& o) noexcept { return IndexBufferT<T_GFXAPI>::operator=(o); };
    ~IndexBuffer() {}

    static_assert(sizeof(IndexBuffer<T_GFXAPI>) != sizeof(IndexBufferT<T_GFXAPI>));   // Ensure this class template is specialized (and not used as-is)
};


template<typename T_GFXAPI>
IndexBufferT<T_GFXAPI>::IndexBufferT(IndexType indexType) noexcept : Buffer<T_GFXAPI>(), mIndexType(indexType)
{}

//template<typename T_GFXAPI>
//IndexBufferT<T_GFXAPI>::IndexBufferT(VkIndexType) noexcept;

template<typename T_GFXAPI>
IndexBufferT<T_GFXAPI>::IndexBufferT(IndexBufferT<T_GFXAPI>&& other) noexcept : Buffer<T_GFXAPI>(std::move(other)), mIndexType(other.mIndexType)
{
    assert(mIndexType == other.mIndexType);
    mNumIndices = other.mNumIndices;
    other.mNumIndices = 0;
    mDspUsable = other.mDspUsable;
}

template<typename T_GFXAPI>
IndexBufferT<T_GFXAPI>& IndexBufferT<T_GFXAPI>::operator=(IndexBufferT<T_GFXAPI>&& other) noexcept
{
    Buffer<T_GFXAPI>::operator=(std::move(other));
    if (&other != this)
    {
        assert(mIndexType == other.mIndexType);
        mNumIndices = other.mNumIndices;
        other.mNumIndices = 0;
        mDspUsable = other.mDspUsable;
    }
    return *this;
}

template<typename T_GFXAPI>
IndexBufferT<T_GFXAPI>::~IndexBufferT() {}

template<typename T_GFXAPI> template<typename T>
bool IndexBufferT<T_GFXAPI>::Initialize(MemoryManager* pManager, const std::span<const T> initialData, const bool dspUsable, const BufferUsageFlags usage)
{
    assert(sizeof(T) == GetIndexTypeBytes());
    return Initialize(pManager, initialData.size(), initialData.data(), dspUsable, usage);
}

template<typename T_GFXAPI>
bool IndexBufferT<T_GFXAPI>::Initialize(MemoryManager* pManager, size_t numIndices, const bool dspUsable, const BufferUsageFlags usage)
{
    return Initialize(pManager, numIndices, (const void*)nullptr, dspUsable, usage);
}

template<typename T_GFXAPI>
bool IndexBufferT<T_GFXAPI>::Initialize(MemoryManager* pManager, size_t numIndices, const void* initialData, const bool dspUsable, const BufferUsageFlags usage)
{
    mNumIndices = numIndices;
    mDspUsable = dspUsable;

    if (dspUsable)
    {
        assert(0); // dsp currently unsupported
        return false;
    }
    else
    {
        return Buffer<T_GFXAPI>::Initialize(pManager, GetIndexTypeBytes() * mNumIndices, usage, initialData);
    }
}

template<typename T_GFXAPI>
void IndexBufferT<T_GFXAPI>::Destroy()
{
    Buffer<T_GFXAPI>::Destroy();
}

template<typename T_GFXAPI>
template<typename T>
MapGuard<T_GFXAPI, T> IndexBufferT<T_GFXAPI>::Map()
{
    assert( sizeof(T) == GetIndexTypeBytes() );
    return Buffer<T_GFXAPI>::template Map<T>();
}

template<typename T_GFXAPI>
MapGuard<T_GFXAPI, void> IndexBufferT<T_GFXAPI>::MapVoid()
{
    return Buffer<T_GFXAPI>::template Map<void>();
}
