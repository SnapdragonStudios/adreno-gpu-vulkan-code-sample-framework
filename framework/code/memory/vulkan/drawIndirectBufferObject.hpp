//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "vulkan/vulkan.hpp"
#include "bufferObject.hpp"
#include "memory/drawIndirectBuffer.hpp"

class Vulkan;
using DrawIndirectBufferObject = DrawIndirectBuffer<Vulkan>;


/// Buffer containing VkDrawIndirectCommand or VkDrawIndexedIndirectCommand (specialized for Vulkan)
/// @ingroup Memory
template<>
class DrawIndirectBuffer<Vulkan> : public BufferT<Vulkan>
{
    DrawIndirectBuffer(const DrawIndirectBuffer&) = delete;
    DrawIndirectBuffer& operator=(const DrawIndirectBuffer&) = delete;
public:
    DrawIndirectBuffer(bool indexed) noexcept;
    DrawIndirectBuffer(DrawIndirectBuffer&&) noexcept;
    DrawIndirectBuffer& operator=(DrawIndirectBuffer&&) noexcept;
    ~DrawIndirectBuffer();

    /// Initialization
    template<typename T_DRAW, typename T_PREQUEL> bool Initialize(MemoryManager* pManager, size_t numDraws, const T_DRAW* initialData, const T_PREQUEL* initialPrequelData, const BufferUsageFlags usage = BufferUsageFlags::Indirect);
    template<typename T_DRAW> bool Initialize(MemoryManager* pManager, size_t numDraws, const T_DRAW* initialData, const BufferUsageFlags usage = BufferUsageFlags::Indirect);

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy();

    /// create a copy of this buffer (including a new copy of the data)
    /// @param newUsageFlags make the copy with new usage flags (default to copying the source usage flags)
    DrawIndirectBuffer Copy(BufferUsageFlags newUsageFlags = BufferUsageFlags::Unknown);

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return mPrequelBytes + GetDrawCommandBytes() * mNumDraws; }

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T> MapGuard<Vulkan, T> Map();

    bool IsIndexed() const { return mIndexed; }
    uint32_t GetDrawCommandBytes() const { return (mIndexed ? sizeof(VkDrawIndexedIndirectCommand) : sizeof(VkDrawIndirectCommand)); }
    auto GetNumDraws() const { return mNumDraws; }
    auto GetBufferOffset() const { return mPrequelBytes; }

protected:
    MapGuard<Vulkan, void> MapVoid();
    bool Initialize(MemoryManager* pManager, size_t numDraws, const void* initialData, const BufferUsageFlags usage, const void* prequelData, uint32_t prequelBytes);

private:
    size_t      mNumDraws = 0;
    uint32_t    mPrequelBytes = 0;   // buffer bytes before the index data begins.
    const bool  mIndexed;
};


template<typename T>
bool DrawIndirectBuffer<Vulkan>::Initialize(MemoryManager* pManager, size_t numDraws, const T* initialData, const BufferUsageFlags usage)
{
    assert(sizeof(T) == GetDrawCommandBytes());
    return Initialize(pManager, numDraws, (const void*)initialData, usage, nullptr, 0);
}

template<typename T_DRAW, typename T_PREQUEL>
bool DrawIndirectBuffer<Vulkan>::Initialize(MemoryManager* pManager, size_t numDraws, const T_DRAW* initialData, const T_PREQUEL* prequelData, const BufferUsageFlags usage)
{
    assert(sizeof(T_DRAW) == GetDrawCommandBytes());
    return Initialize(pManager, numDraws, (const void*)initialData, usage, prequelData, sizeof(T_PREQUEL));
}

template<typename T>
MapGuard<Vulkan, T> DrawIndirectBuffer<Vulkan>::Map()
{
    assert(mPrequelBytes == 0);  // prequel bytes not yet supported in map
    assert(sizeof(T) == GetDrawCommandBytes());
    return BufferT<Vulkan>::template Map<T>();
}
