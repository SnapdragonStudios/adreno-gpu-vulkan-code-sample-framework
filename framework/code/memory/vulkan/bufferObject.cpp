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
#include <cstring>


///////////////////////////////////////////////////////////////////////////////

Buffer<Vulkan>::Buffer()
{
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Vulkan>::Buffer(Buffer<Vulkan>&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Vulkan>& Buffer<Vulkan>::operator=(Buffer<Vulkan>&& other) noexcept
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

Buffer<Vulkan>::~Buffer()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

Buffer<Vulkan>::operator bool() const
{
    return !!mAllocatedBuffer;
}

///////////////////////////////////////////////////////////////////////////////

bool Buffer<Vulkan>::Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, const void* initialData)
{
    MemoryUsage memoryUsage = initialData ? MemoryUsage::CpuToGpu : MemoryUsage::GpuExclusive;
    if ((bufferUsageFlags & BufferUsageFlags::TransferSrc) != 0)
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

bool Buffer<Vulkan>::Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, MemoryUsage memoryUsage)
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

bool Buffer<Vulkan>::Update(MemoryManager* pManager, size_t dataSize, const void* newData)
{
    if (newData)
    {
        auto mappedGuard = Map<uint8_t>();
        memcpy(mappedGuard.data(), newData, dataSize);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Buffer<Vulkan>::GetMeshData(MemoryManager* pManager, size_t dataSize, void* outputData) const
{
    auto* pThisNonConst = const_cast<Buffer<Vulkan>*>(this);   // we remove the const-ness of 'this' so we can do the map/unmap.  Treating this as "no harm no foul" - our class on exit is identical to what it is on entry!
    if (outputData)
    {
        auto mappedGuard = pThisNonConst->Map<uint8_t>();
        memcpy(outputData, mappedGuard.data(), dataSize);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void Buffer<Vulkan>::Destroy()
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

void Buffer<Vulkan>::Unmap(MemoryCpuMappedUntyped buffer)
{
    mManager->Unmap(mAllocatedBuffer, std::move(buffer));
}

///////////////////////////////////////////////////////////////////////////////
