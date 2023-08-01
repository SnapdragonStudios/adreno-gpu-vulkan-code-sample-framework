//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "bufferObject.hpp"
#include "memoryManager.hpp"
#include <cassert>


///////////////////////////////////////////////////////////////////////////////

BufferT<Vulkan>::BufferT()
{
}

///////////////////////////////////////////////////////////////////////////////

BufferT<Vulkan>::BufferT(BufferT<Vulkan>&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

BufferT<Vulkan>& BufferT<Vulkan>::operator=(BufferT<Vulkan>&& other) noexcept
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

BufferT<Vulkan>::~BufferT()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

BufferT<Vulkan>::operator bool() const
{
    return !!mAllocatedBuffer;
}

///////////////////////////////////////////////////////////////////////////////

bool BufferT<Vulkan>::Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, const void* initialData)
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

bool BufferT<Vulkan>::Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, MemoryUsage memoryUsage)
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

void BufferT<Vulkan>::Destroy()
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

void BufferT<Vulkan>::Unmap(MemoryCpuMappedUntyped buffer)
{
    mManager->Unmap(mAllocatedBuffer, std::move(buffer));
}

///////////////////////////////////////////////////////////////////////////////
