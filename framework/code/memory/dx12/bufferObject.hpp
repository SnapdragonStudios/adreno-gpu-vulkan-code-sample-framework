//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <d3d12.h>
#include "memoryManager.hpp"    // dx12
#include "memoryMapped.hpp"     // dx12
#include "memory/buffer.hpp"    // agnostic
#include "memory/memory.hpp"    // agnostic


/// Template specialization for creating and holding Dx12 graphics memory buffer objects.
/// @ingroup Memory
template<>
class Buffer<Dx12> : public BufferBase
{
public:
    Buffer() noexcept;
    virtual ~Buffer();
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    using MemoryManager = MemoryManager<Dx12>;
    using MemoryCpuMappedUntyped = MemoryCpuMappedUntyped<Dx12>;

    explicit operator bool() const;

    bool Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, const void* initialData);
    bool Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, MemoryUsage memoryUsage);

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy() override;

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T>
    MapGuard<Dx12, T> Map()
    {
        assert(mManager);
        return { *this, std::move(mManager->Map<T>(mAllocatedBuffer)) };
    }

    uint64_t GetResourceGPUVirtualAddress();

protected:
    template<typename T_GFXAPI, typename T> friend class MapGuard;
    void Unmap(MemoryCpuMappedUntyped buffer);

protected:
    using MemoryAllocatedBuffer = MemoryAllocatedBuffer<Dx12, ID3D12Resource*>;
    MemoryManager* mManager = nullptr;
    MemoryAllocatedBuffer mAllocatedBuffer;
    BufferUsageFlags mBufferUsageFlags = BufferUsageFlags::Unknown;
};
