//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "indexBufferObject.hpp"



IndexBufferObject::IndexBufferObject(VkIndexType indexType) : BufferObject(), mIndexType(indexType)
{
}

///////////////////////////////////////////////////////////////////////////////

IndexBufferObject::~IndexBufferObject()
{
}

///////////////////////////////////////////////////////////////////////////////

IndexBufferObject::IndexBufferObject(IndexBufferObject&& other) noexcept : mIndexType(other.mIndexType)
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

IndexBufferObject& IndexBufferObject::operator=(IndexBufferObject&& other) noexcept
{
    BufferObject::operator=(std::move(other));
    if (&other != this)
    {
        assert(mIndexType == other.mIndexType);
        mNumIndices = other.mNumIndices;
        other.mNumIndices = 0;
        mDspUsable = other.mDspUsable;
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

bool IndexBufferObject::Initialize(MemoryManager* pManager, size_t numIndices, const void* initialData, const bool dspUsable, const VkBufferUsageFlags usage)
{
    mNumIndices = numIndices;
    mDspUsable = dspUsable;

    if (dspUsable)
    {
        mManager = pManager;
        assert(0); // dsp currently unsupported
        return false;
    }
    else
    {
        return BufferObject::Initialize(pManager, GetIndexTypeBytes() * mNumIndices, usage, initialData);
    }
}

///////////////////////////////////////////////////////////////////////////////

void IndexBufferObject::Destroy()
{
    mNumIndices = 0;
    BufferObject::Destroy();
}


///////////////////////////////////////////////////////////////////////////////

IndexBufferObject IndexBufferObject::Copy()
{
    IndexBufferObject copy( GetIndexType() );
    if (copy.Initialize(mManager, GetNumIndices(), MapVoid().data(), mDspUsable, mBufferUsageFlags))
    {
        return copy;
    }
    else
    {
        return { GetIndexType() };
    }
}

///////////////////////////////////////////////////////////////////////////////

IndexBufferObject IndexBufferObject::Copy( VkCommandBuffer vkCommandBuffer, VkBufferUsageFlags bufferUsage, MemoryManager::MemoryUsage memoryUsage ) const
{
    IndexBufferObject copy( GetIndexType() );

    copy.mNumIndices = mNumIndices;
    copy.mDspUsable = mDspUsable;

    if (mDspUsable)
    {
        assert( 0 ); // dsp currently unsupported
        return copy;
    }

    if (((mBufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0) || ((bufferUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0))
    {
        // treat this as an error - use Copy() for buffers that are not setup for transfer
        assert( 0 );
        return copy;
    }

    // Create the buffer we are copying into.
    size_t size = GetIndexTypeBytes() * mNumIndices;
    if (!copy.BufferObject::Initialize(mManager, size, bufferUsage, memoryUsage))
        return copy;

    // Use gpu copy commands to copy buffer data
    if (!mManager->CopyData( vkCommandBuffer, mVmaBuffer, copy.mVmaBuffer, size, 0, 0 ))
    {
        copy.Destroy();
        return copy;
    }
    return copy;
}

///////////////////////////////////////////////////////////////////////////////

