//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "drawIndirectBufferObject.hpp"



DrawIndirectBufferObject::DrawIndirectBufferObject(bool indexed) : mIndexed(indexed)
{
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBufferObject::~DrawIndirectBufferObject()
{
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBufferObject::DrawIndirectBufferObject(DrawIndirectBufferObject&& other) noexcept : mIndexed(other.mIndexed)
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

DrawIndirectBufferObject& DrawIndirectBufferObject::operator=(DrawIndirectBufferObject&& other) noexcept
{
    BufferObject::operator=(std::move(other));
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

bool DrawIndirectBufferObject::Initialize(MemoryManager* pManager, size_t numDraws, const void* initialData, const VkBufferUsageFlags usage, uint32_t prequelBytes)
{
    mNumDraws = numDraws;
    mPrequelBytes = prequelBytes;

    return BufferObject::Initialize(pManager, (VkDeviceSize)(GetDrawCommandBytes() * mNumDraws + mPrequelBytes), usage, initialData);
}

///////////////////////////////////////////////////////////////////////////////

void DrawIndirectBufferObject::Destroy()
{
    mNumDraws = 0;
    BufferObject::Destroy();
}


///////////////////////////////////////////////////////////////////////////////

DrawIndirectBufferObject DrawIndirectBufferObject::Copy()
{
    DrawIndirectBufferObject copy( IsIndexed() );
    if (copy.Initialize(mManager, GetNumDraws(), MapVoid().data(), mBufferUsageFlags, mPrequelBytes))
    {
        return copy;
    }
    else
    {
        return { IsIndexed() };
    }
}

///////////////////////////////////////////////////////////////////////////////
