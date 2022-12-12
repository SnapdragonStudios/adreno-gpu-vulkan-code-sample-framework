//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "bufferObject.hpp"


/// Buffer containing VkDrawIndirectCommand or VkDrawIndexedIndirectCommand s
/// @ingroup Memory
class DrawIndirectBufferObject : public BufferObject
{
    DrawIndirectBufferObject(const DrawIndirectBufferObject&) = delete;
    DrawIndirectBufferObject& operator=(const DrawIndirectBufferObject&) = delete;
public:
    DrawIndirectBufferObject(bool indexed);
    DrawIndirectBufferObject(DrawIndirectBufferObject&&) noexcept;
    DrawIndirectBufferObject& operator=(DrawIndirectBufferObject&&) noexcept;
    ~DrawIndirectBufferObject();

    /// Initialization
    template<typename T_DRAW, typename T_PREQUEL> bool Initialize(MemoryManager* pManager, size_t numDraws, const T_DRAW* initialData, const T_PREQUEL* initialPrequelData, const VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    template<typename T_DRAW> bool Initialize(MemoryManager* pManager, size_t numDraws, const T_DRAW* initialData, const VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy();

    /// create a copy of this buffer (including a new copy of the data)
    DrawIndirectBufferObject Copy();

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return mPrequelBytes + GetDrawCommandBytes() * mNumDraws; }

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T> MapGuard<T> Map();

    bool IsIndexed() const { return mIndexed; }
    uint32_t GetDrawCommandBytes() const { return (mIndexed ? sizeof(VkDrawIndexedIndirectCommand) : sizeof(VkDrawIndirectCommand)); }
    auto GetNumDraws() const { return mNumDraws; }
    auto GetBufferOffset() const { return mPrequelBytes; }

protected:
    MapGuard<void> MapVoid();
    bool Initialize(MemoryManager* pManager, size_t numDraws, const void* initialData, const VkBufferUsageFlags usage, uint32_t prequelBytes);

private:
    size_t      mNumDraws = 0;
    uint32_t    mPrequelBytes = 0;   // buffer bytes before the index data begins.
    const bool  mIndexed;
};


template<typename T>
bool DrawIndirectBufferObject::Initialize(MemoryManager* pManager, size_t numDraws, const T* initialData, const VkBufferUsageFlags usage)
{
    assert(sizeof(T) == GetDrawCommandBytes());
    return Initialize(pManager, numDraws, (const void*)initialData, usage, 0);
}

template<typename T_DRAW, typename T_PREQUEL>
bool DrawIndirectBufferObject::Initialize(MemoryManager* pManager, size_t numDraws, const T_DRAW* initialData, const T_PREQUEL* prequelData, const VkBufferUsageFlags usage)
{
    assert(sizeof(T_DRAW) == GetDrawCommandBytes());
    return Initialize(pManager, numDraws, (const void*)initialData, usage, sizeof(T_PREQUEL));
}

template<typename T>
BufferObject::MapGuard<T> DrawIndirectBufferObject::Map()
{
    assert(mPrequelBytes == 0);  // prequel bytes not yet supported in map
    assert(sizeof(T) == GetDrawCommandBytes());
    return BufferObject::Map<T>();
}

inline BufferObject::MapGuard<void> DrawIndirectBufferObject::MapVoid()
{
    return BufferObject::Map<void>();
}
