// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
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
	friend class MemoryManager;
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
class MemoryManager
{
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager& operator=(const MemoryManager&) = delete;
public:
	enum class MemoryUsage {
		Unknown = 0,
		GpuExclusive,
		CpuExclusive,
		CpuToGpu,
		GpuToCpu,
		CpuCopy,
		GpuLazilyAllocated
	};

	MemoryManager();
	~MemoryManager();

	/// Initialize the memory manager (must be initialized before using CreateBuffer etc)
	bool Initialize(VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice, VkInstance vkInstance, bool );

	/// Create buffer in memory and create the associated Vulkan objects
	MemoryVmaAllocatedBuffer<VkBuffer> CreateBuffer(size_t size, VkBufferUsageFlags bufferUsage, MemoryUsage memoryUsage, VkDescriptorBufferInfo* /*output, optional*/ = nullptr);
	/// Create image in memory and create the associated Vulkan objects
	MemoryVmaAllocatedBuffer<VkImage> CreateImage(const VkImageCreateInfo& imageInfo, MemoryUsage memoryUsage);

	/// Destruction of created buffer
	void Destroy(MemoryVmaAllocatedBuffer<VkBuffer>);
	/// Destruction of created image
	void Destroy(MemoryVmaAllocatedBuffer<VkImage>);

	// Creation of Android Hardware buffer (with a Vulkan object)
	MemoryAbhAllocatedBuffer CreateAndroidHardwareBuffer(size_t size, VkBufferUsageFlags bufferUsage, MemoryUsage memoryUsage);

	/// Map a buffer to cpu memory
	template<typename T_DATATYPE, typename T_VKTYPE>
	MemoryCpuMapped<T_DATATYPE> Map(MemoryVmaAllocatedBuffer<T_VKTYPE>& buffer);

	/// Unmap a buffer from cpu memory
	template<typename T_VKTYPE>
	void Unmap(MemoryVmaAllocatedBuffer<T_VKTYPE>& buffer, MemoryCpuMappedUntyped allocation);

	/// Query the device address (assuming that Vulkan extension was enabled)
	VkDeviceAddress GetBufferDeviceAddress(VkBuffer b) const { return GetBufferDeviceAddressInternal(b); }
	/// Query the device address (assuming that Vulkan extension was enabled)
	template<typename T>
	VkDeviceAddress GetBufferDeviceAddress(const T& b) const { return GetBufferDeviceAddress(b.GetVkBuffer()); }

protected:
	MemoryCpuMappedUntyped MapInt(MemoryVmaAllocation allocation);
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
MemoryCpuMapped<T_DATATYPE> MemoryManager::Map(MemoryVmaAllocatedBuffer<T_VKTYPE>& buffer)
{
	return MapInt(std::move(buffer.allocation));
}

template<typename T_VKTYPE>
void MemoryManager::Unmap(MemoryVmaAllocatedBuffer<T_VKTYPE>& buffer, MemoryCpuMappedUntyped allocation)
{
	assert(allocation.mCpuLocation);
	UnmapInternal(allocation.mAllocation.vmaAllocation, allocation.mCpuLocation);
	allocation.mCpuLocation = nullptr;	// clear ownership
	buffer.allocation = std::move(allocation.mAllocation);
}

inline MemoryCpuMappedUntyped MemoryManager::MapInt(MemoryVmaAllocation allocation)
{
	MemoryCpuMappedUntyped guard(std::move(allocation));
	MapInternal(guard.mAllocation.vmaAllocation, &guard.mCpuLocation);
	return guard;
}
