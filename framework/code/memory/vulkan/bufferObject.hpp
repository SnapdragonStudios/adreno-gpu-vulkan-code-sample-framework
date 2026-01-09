//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <volk/volk.h>
#include "memory/buffer.hpp"
#include "memory/memory.hpp"
#include "memoryManager.hpp"
#include "memoryMapped.hpp"

// Forward declarations
struct AHardwareBuffer_Desc;
struct AHardwareBuffer;
template<typename T_GFXAPI> class Buffer;


/// Defines a simple (vulkan specialized) object for creating and holding Vulkan memory buffer objects.
/// @ingroup Memory
template<>
class Buffer<Vulkan> : public BufferBase
{
    Buffer<Vulkan>& operator=(const Buffer<Vulkan>&) = delete;
    Buffer(const Buffer<Vulkan>&) = delete;
protected:
public:
    Buffer();
    virtual ~Buffer();
	Buffer(Buffer<Vulkan>&&) noexcept;
	Buffer& operator=(Buffer<Vulkan>&&) noexcept;

    using MemoryManager = MemoryManager<Vulkan>;
    using MemoryCpuMappedUntyped = MemoryCpuMappedUntyped<Vulkan>;

    explicit operator bool() const;

    bool Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, const void* initialData);
	bool Initialize(MemoryManager* pManager, size_t size, BufferUsageFlags bufferUsageFlags, MemoryUsage memoryUsage);
	//bool Initialize(MemoryManager* pManager, BufferUsageFlags usageFlags, const AHardwareBuffer_Desc& hardwareBufferDesc, const void* initialData);
    //bool Initialize(MemoryManager* pManagere, BufferUsageFlags usageFlags, const AHardwareBuffer* pAHardwareBuffer);

    bool Update(MemoryManager* pManager, size_t dataSize, const void* newData);
    bool GetMeshData(MemoryManager* pManager, size_t dataSize, void* outputData) const;

    /// destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy();

	// Accessors
	const VkBuffer GetVkBuffer() const { return mAllocatedBuffer.GetVkBuffer(); }

	/// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T>
	MapGuard<Vulkan, T> Map()
    {
        assert(mManager);
        return { *this, std::move(mManager->Map<T>(mAllocatedBuffer)) };
    }

protected:
    template<typename T_GFXAPI, typename T> friend class MapGuard;
    void Unmap(MemoryCpuMappedUntyped buffer);

protected:
    using MemoryAllocatedBuffer = MemoryAllocatedBuffer<Vulkan, VkBuffer>;
    MemoryManager* mManager = nullptr;
    MemoryAllocatedBuffer mAllocatedBuffer;
	BufferUsageFlags mBufferUsageFlags = BufferUsageFlags::Unknown;
};
