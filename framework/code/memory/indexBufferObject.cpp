// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "indexBufferObject.hpp"



IndexBufferObject::IndexBufferObject(VkIndexType indexType) : mIndexType(indexType)
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
        return BufferObject::Initialize(pManager, (VkDeviceSize)(GetIndexTypeBytes() * mNumIndices), usage, initialData);
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
