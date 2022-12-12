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
#include "tinyobjloader/tiny_obj_loader.h"

///////////////////////////////////////////////////////////////////////////////

BufferObject::BufferObject()
{
}

///////////////////////////////////////////////////////////////////////////////

BufferObject::BufferObject(BufferObject&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

BufferObject& BufferObject::operator=(BufferObject&& other) noexcept
{
    if (this != &other)
    {
        mManager = other.mManager;
        other.mManager = nullptr;
        mVmaBuffer = std::move(other.mVmaBuffer);
        mBufferUsageFlags = other.mBufferUsageFlags;
        other.mBufferUsageFlags = 0;
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

BufferObject::~BufferObject()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

BufferObject::operator bool() const
{
    return !!mVmaBuffer;
}

///////////////////////////////////////////////////////////////////////////////

bool BufferObject::Initialize(MemoryManager* pManager, size_t size, VkBufferUsageFlags bufferUsageFlags, const void* initialData)
{
    MemoryManager::MemoryUsage memoryUsage = initialData ? MemoryManager::MemoryUsage::CpuToGpu : MemoryManager::MemoryUsage::GpuExclusive;
    if ((bufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0)
        memoryUsage = MemoryManager::MemoryUsage::CpuToGpu;

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

bool BufferObject::Initialize(MemoryManager* pManager, size_t size, VkBufferUsageFlags bufferUsageFlags, MemoryManager::MemoryUsage memoryUsage)
{
    assert(!mManager);
    if (!pManager)
    {
        return false;
    }
    mManager = pManager;

    mVmaBuffer = mManager->CreateBuffer(size, bufferUsageFlags, memoryUsage);
    mBufferUsageFlags = bufferUsageFlags;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void BufferObject::Destroy()
{
    if (!mManager)
    {
        assert(!mVmaBuffer);    // ensure we don't have an orphaned buffer (somehow)
        return;
    }
    mManager->Destroy( std::move(mVmaBuffer) );
    mManager = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
