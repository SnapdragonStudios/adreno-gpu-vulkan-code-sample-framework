//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "drawIndirectBufferObject.hpp"


DrawIndirectBuffer<Vulkan>::DrawIndirectBuffer(bool indexed) noexcept : BufferT<Vulkan>(), mIndexed(indexed)
{
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBuffer<Vulkan>::~DrawIndirectBuffer()
{
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBuffer<Vulkan>::DrawIndirectBuffer(DrawIndirectBuffer<Vulkan>&& other) noexcept : mIndexed(other.mIndexed)
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBuffer<Vulkan>& DrawIndirectBuffer<Vulkan>::operator=(DrawIndirectBuffer<Vulkan>&& other) noexcept
{
    BufferT<Vulkan>::operator=(std::move(other));
    if (&other != this)
    {
        assert(mIndexed == other.mIndexed);
        mNumDraws = other.mNumDraws;
        other.mNumDraws = 0;
        mPrequelBytes = other.mPrequelBytes;
        other.mPrequelBytes = 0;
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

bool DrawIndirectBuffer<Vulkan>::Initialize(MemoryManager* pManager, size_t numDraws, const void* initialData, const BufferUsageFlags usage, const void* prequelData, uint32_t prequelBytes)
{
    mNumDraws = numDraws;
    mPrequelBytes = prequelBytes;

    MemoryUsage memoryUsage = initialData ? MemoryUsage::CpuToGpu : MemoryUsage::GpuExclusive;
    if ((usage & BufferUsageFlags::TransferSrc) != 0)
        memoryUsage = MemoryUsage::CpuToGpu;

    if (!BufferT<Vulkan>::Initialize(pManager, (VkDeviceSize)(GetDrawCommandBytes() * mNumDraws + mPrequelBytes), usage, memoryUsage))
    {
        return false;
    }

    if (initialData)
    {
        auto mappedGuard = BufferT<Vulkan>::Map<uint8_t>();
        auto initialDataSize = GetDrawCommandBytes() * mNumDraws;
        if (prequelBytes)
            memcpy(mappedGuard.data(), prequelData, prequelBytes);
        else
            memset(mappedGuard.data(), 0, prequelBytes);
        memcpy(mappedGuard.data() + prequelBytes, initialData, initialDataSize);
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

void DrawIndirectBuffer<Vulkan>::Destroy()
{
    mNumDraws = 0;
    BufferT<Vulkan>::Destroy();
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBuffer<Vulkan> DrawIndirectBuffer<Vulkan>::Copy(BufferUsageFlags newUsageFlags)
{
    if (newUsageFlags == BufferUsageFlags::Unknown)
        newUsageFlags = mBufferUsageFlags;
    DrawIndirectBuffer copy( IsIndexed() );
    auto data = MapVoid();
    if (copy.Initialize(mManager, GetNumDraws(), static_cast<std::byte*>(data.data()) + mPrequelBytes, newUsageFlags, data.data(), mPrequelBytes))
    {
        return copy;
    }
    else
    {
        return { IsIndexed() };
    }
}

///////////////////////////////////////////////////////////////////////////////

MapGuard<Vulkan, void> DrawIndirectBuffer<Vulkan>::MapVoid()
{
    return BufferT<Vulkan>::Map<void>();
}
