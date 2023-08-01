//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @defgroup Memory
/// Vulkan memory buffer allocation, destruction and mapping (to cpu).
/// 
/// For the most part the memory allocations will go through the Vulkan Memory Allocator library.
/// (deliberately not #included in any headers since it is a very large header-only library)


#include <memory>
#include <cassert>
#include <vulkan/vulkan.h>
#ifdef OS_WINDOWS
#include <vulkan/vulkan_beta.h>
#endif // OS_WINDOWS
#include "memory/memory.hpp"
#include "memory/memoryManager.hpp"
#include "memoryMapped.hpp"

// forward declarations
class Vulkan;
struct VmaAllocator_T;
struct AHardwareBuffer;


//template<typename T_VKTYPE /*VkImage or VkBuffer*/>
class MemoryAbhAllocatedBuffer
{
	MemoryAbhAllocatedBuffer(const MemoryAbhAllocatedBuffer&) = delete;
	MemoryAbhAllocatedBuffer& operator=(const MemoryAbhAllocatedBuffer&) = delete;
public:
	friend class MemoryManager<Vulkan>;
	// Restrict MemoryAbhAllocatedBuffer to not be duplicated and not accidentally deleted (leaking memory).
	MemoryAbhAllocatedBuffer(MemoryAbhAllocatedBuffer&& other);
	MemoryAbhAllocatedBuffer& operator=(MemoryAbhAllocatedBuffer&& other);
	MemoryAbhAllocatedBuffer() {}
//	const T_VKTYPE& GetVkBuffer() const { return buffer; }
	explicit operator bool() const { return static_cast<bool>(allocation); }
private:
	AHardwareBuffer* allocation = nullptr;
//	T_VKTYPE buffer = VK_NULL_HANDLE;	// the allocated vulkan buffer (or VK_NULL_HANDLE)
};


/// Top level API for allocating memory buffers for use by Vulkan
/// @ingroup Memory
template<>
class MemoryManager<Vulkan>
{
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager& operator=(const MemoryManager&) = delete;
public:
	MemoryManager();
	~MemoryManager();
    using tGfxApi = Vulkan;

	/// Initialize the memory manager (must be initialized before using CreateBuffer etc)
	/// @param EnableBufferDeviceAddress enable ability to call GetBufferDeviceAddress
	/// @return true if successfully initialized
	bool Initialize(VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice, VkInstance vkInstance, bool EnableBufferDeviceAddress);
	/// Destroy the memory manager (do before you destroy the Vulkan device)
	void Destroy();

	/// Create buffer in memory and create the associated Vulkan objects
	MemoryAllocatedBuffer<Vulkan, VkBuffer> CreateBuffer(size_t size, BufferUsageFlags bufferUsage, MemoryUsage memoryUsage, VkDescriptorBufferInfo* /*output, optional*/ = nullptr);
	/// Create image in memory and create the associated Vulkan objects
	MemoryAllocatedBuffer<Vulkan, VkImage> CreateImage(const VkImageCreateInfo& imageInfo, MemoryUsage memoryUsage);
	/// Allocation with 'unknown' use type.  Typically you would use CreateBuffer or CreateImage, this is for special cases!
	MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> AllocateMemory(size_t size, uint32_t memoryTypeBits);
	/// Bind the provided image to already allocated memory (assumes the memory was allocated to be compatible)
	/// Memory allocation is transfered from the input MemoryVmaAllocatedBuffer to the returned MemoryVmaAllocatedBuffer (ownership transfer of memory and image)
	MemoryAllocatedBuffer<Vulkan, VkImage> BindImageToMemory(VkImage image, MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>&& memory) const;
	/// Bind the provided buffer to already allocated memory (assumes the memory was allocated to be compatible)
	/// Memory allocation is transfered from the input MemoryVmaAllocatedBuffer to the returned MemoryVmaAllocatedBuffer (ownership transfer of memory and buffer)
	MemoryAllocatedBuffer<Vulkan, VkBuffer> BindBufferToMemory(VkBuffer buffer, MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>&& memory) const;

	/// Destruction of created buffer
	void Destroy(MemoryAllocatedBuffer<Vulkan, VkBuffer>);
	/// Destruction of created image
	void Destroy(MemoryAllocatedBuffer<Vulkan, VkImage>);
	/// Destruction of a memory buffer (without a bound vkImage or vkBuffer)
	void Destroy(MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> vmaAllocatedMemory);
	/// Destruction of just the bound buffer (not the underlying device memory)
	MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> DestroyBufferOnly(MemoryAllocatedBuffer<Vulkan, VkBuffer>);
	/// Destruction of just the bound image (not the underlying device memory)
	MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> DestroyBufferOnly(MemoryAllocatedBuffer<Vulkan, VkImage>);

	// Creation of Android Hardware buffer (with a Vulkan object)
	MemoryAbhAllocatedBuffer CreateAndroidHardwareBuffer(size_t size, BufferUsageFlags bufferUsage, MemoryUsage memoryUsage);

	/// Map a buffer to cpu memory
	template<typename T_DATATYPE, typename T_VKTYPE>
	MemoryCpuMapped<Vulkan, T_DATATYPE> Map(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>& buffer);

	/// Unmap a buffer from cpu memory
	template<typename T_VKTYPE>
	void Unmap(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>& buffer, MemoryCpuMappedUntyped<Vulkan> allocation);

    /// Copy data in one buffer into another.  Assumes buffers created with appropriate VK_BUFFER_USAGE_TRANSFER_SRC_BIT and VK_BUFFER_USAGE_TRANSFER_DST_BIT
    bool CopyData(VkCommandBuffer vkCommandBuffer, const MemoryAllocatedBuffer<Vulkan, VkBuffer>& src, MemoryAllocatedBuffer<Vulkan, VkBuffer>& dst, size_t copySize, size_t srcOffset = 0, size_t dstOffset = 0);

	/// Query the device address (assuming that Vulkan extension was enabled)
	VkDeviceAddress GetBufferDeviceAddress(VkBuffer b) const { return GetBufferDeviceAddressInternal(b); }
	/// Query the device address (assuming that Vulkan extension was enabled)
	template<typename T>
	VkDeviceAddress GetBufferDeviceAddress(const T& b) const { return GetBufferDeviceAddress(b.GetVkBuffer()); }

protected:
	MemoryCpuMappedUntyped<Vulkan> MapInt(MemoryAllocation<Vulkan> allocation);
private:
	VkDeviceAddress GetBufferDeviceAddressInternal(VkBuffer buffer) const;
	void MapInternal(void* vmaAllocation, void** outCpuLocation);
	void UnmapInternal(void* vmaAllocation, void* cpuLocation);
private:
	VmaAllocator_T*					mVmaAllocator = nullptr;
	VkDevice						mGpuDevice = VK_NULL_HANDLE;
#if VK_KHR_buffer_device_address
	PFN_vkGetBufferDeviceAddressKHR mFpGetBufferDeviceAddress = nullptr;
#elif VK_EXT_buffer_device_address
	PFN_vkGetBufferDeviceAddressEXT mFpGetBufferDeviceAddress = nullptr;
#endif // VK_KHR_buffer_device_address | VK_EXT_buffer_device_address
};


template<typename T_DATATYPE, typename T_VKTYPE>
MemoryCpuMapped<Vulkan, T_DATATYPE> MemoryManager<Vulkan>::Map(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>& buffer)
{
	return MapInt(std::move(buffer.allocation));
}

template<typename T_VKTYPE>
void MemoryManager<Vulkan>::Unmap(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>& buffer, MemoryCpuMappedUntyped<Vulkan> allocation)
{
	assert(allocation.mCpuLocation);
	UnmapInternal(allocation.mAllocation.allocation, allocation.mCpuLocation);
	allocation.mCpuLocation = nullptr;	// clear ownership
	buffer.allocation = std::move(allocation.mAllocation);
}

inline MemoryCpuMappedUntyped<Vulkan> MemoryManager<Vulkan>::MapInt(MemoryAllocation<Vulkan> allocation)
{
	MemoryCpuMappedUntyped guard(std::move(allocation));
	MapInternal(guard.mAllocation.allocation, &guard.mCpuLocation);
	return guard;
}
