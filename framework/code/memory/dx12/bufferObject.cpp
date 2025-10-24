//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "bufferObject.hpp"
#include "memoryManager.hpp"
#include <cassert>
//#include <type_traits>

Buffer<Dx12>::Buffer() noexcept
{
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Dx12>::Buffer(Buffer&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Dx12>& Buffer<Dx12>::operator=(Buffer<Dx12>&& other) noexcept
{
    if (this != &other)
    {
        mManager = other.mManager;
        other.mManager = nullptr;
        mAllocatedBuffer = std::move(other.mAllocatedBuffer);
        mBufferUsageFlags = other.mBufferUsageFlags;
        other.mBufferUsageFlags = BufferUsageFlags::Unknown;
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Dx12>::~Buffer()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Dx12>::operator bool() const
{
    return !!mAllocatedBuffer;
}

///////////////////////////////////////////////////////////////////////////////
template< class Enum >
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept { return static_cast<std::underlying_type_t<Enum>>(e); };

bool Buffer<Dx12>::Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, const void* initialData)
{
    MemoryUsage memoryUsage = initialData ? MemoryUsage::CpuToGpu : MemoryUsage::GpuExclusive;
    if ((to_underlying(bufferUsageFlags) & to_underlying(BufferUsageFlags::TransferSrc)) != 0)
        memoryUsage = MemoryUsage::CpuToGpu;

    if (!Initialize(pManager, size, bufferUsageFlags, memoryUsage))
    {
        return false;
    }

    if (initialData)
    {
        auto mappedGuard = Map<uint8_t>();
        memcpy(mappedGuard.data(), initialData, size);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Buffer<Dx12>::Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, MemoryUsage memoryUsage)
{
    assert(!mManager);
    if (!pManager)
    {
        return false;
    }
    mManager = pManager;

    mAllocatedBuffer = mManager->CreateBuffer(size, bufferUsageFlags, memoryUsage);
    mBufferUsageFlags = bufferUsageFlags;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void Buffer<Dx12>::Destroy()
{
    if (!mManager)
    {
        assert(!mAllocatedBuffer);    // ensure we don't have an orphaned buffer (somehow)
        return;
    }
    mManager->Destroy( std::move(mAllocatedBuffer) );
    mManager = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Buffer<Dx12>::Unmap(MemoryCpuMappedUntyped buffer)
{
    mManager->Unmap(mAllocatedBuffer, std::move(buffer));
}

///////////////////////////////////////////////////////////////////////////////

uint64_t Buffer<Dx12>::GetResourceGPUVirtualAddress()
{
    return mAllocatedBuffer.GetResource()->GetGPUVirtualAddress(); /*GetGPUVirtualAddress is not const, so this function cannot be either */
}
