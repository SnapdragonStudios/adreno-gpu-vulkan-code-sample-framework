// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <vulkan/vulkan.h>
#include "memoryMapped.hpp"
#include "memoryManager.hpp"

// Forward declarations
struct AHardwareBuffer_Desc;
struct AHardwareBuffer;

/// Defines a simple base object for creating and holding Vulkan memory buffer objects.
/// @ingroup Memory
class BufferObject
{
    BufferObject& operator=(const BufferObject&) = delete;
    BufferObject(const BufferObject&) = delete;
protected:
public:
    BufferObject();
    virtual ~BufferObject();
	BufferObject(BufferObject&&);
	BufferObject& operator=(BufferObject&&);

    bool Initialize( MemoryManager* pManager, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, const void* initialData );
	bool Initialize(MemoryManager* pManager, size_t size, VkBufferUsageFlags bufferUsageFlags, MemoryManager::MemoryUsage memoryUsage);
	//bool Initialize(MemoryManager* pManager, VkBufferUsageFlags usageFlags, const AHardwareBuffer_Desc& hardwareBufferDesc, const void* initialData);
    //bool Initialize(MemoryManager* pManagere, VkBufferUsageFlags usageFlags, const AHardwareBuffer* pAHardwareBuffer);

    /// destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy();

	// Accessors
	const VkBuffer GetVkBuffer() const { return mVmaBuffer.GetVkBuffer(); }

	/// Memory map guard that automatically unmaps when it goes out of scope
	template<typename T>
	class MapGuard : public MemoryCpuMapped<T>
	{
		MapGuard(const MapGuard&) = delete;
		MapGuard& operator=(const MapGuard&) = delete;
		MapGuard& operator=(MapGuard&& other) = delete;
	public:
		MapGuard(BufferObject& buffer, MemoryCpuMapped<T> mapped)
			: MemoryCpuMapped<T>(std::move(mapped))	// Only ever one owner of a MemoryCpuMapped
			, mBuffer(&buffer)						// not passed as a pointer so we know it's never initialized null
		{}
		~MapGuard()
		{
			if (mBuffer)	// nullptr if EarlyUnmap'ed
			{
				mBuffer->Unmap(std::move(*this));
			}
		}
		void EarlyUnmap()
		{
			assert(mBuffer);	// error on second call to EarlyUnmap
			mBuffer->Unmap(std::move(*this));
			mBuffer = nullptr;
		}
		MapGuard(MapGuard&& other)
			: MemoryCpuMapped<T>(std::move(other))
			, mBuffer(other.mBuffer)
		{
			other.mBuffer = nullptr;
		}
	private:
		BufferObject* mBuffer;
	};

	/// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T>
	MapGuard<T> Map()
    {
        assert(mManager);
        return { *this, std::move(mManager->Map<T>(mVmaBuffer)) };
    }

protected:
	void Unmap(MemoryCpuMappedUntyped buffer)
	{
		mManager->Unmap(mVmaBuffer, std::move(buffer));
	}

    MemoryManager* mManager = nullptr;
    MemoryVmaAllocatedBuffer<VkBuffer> mVmaBuffer;
	VkBufferUsageFlags mBufferUsageFlags = 0;
};

