// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "memoryManager.hpp"
#include "vulkan/vulkan.hpp"
#include <cassert>

//
// Include the VulkanMemoryAllocator AND compile the implementation in this cpp (is a header-only library)
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vulkan/vulkan.h>
#if OS_WINDOWS
// Include Beta on Windows (does not currently exist on Android)
#include <vulkan/vulkan_beta.h>
#endif // OS_WINDOWS
#include "VulkanMemoryAllocator/src/vk_mem_alloc.h"


// Ensure our enum matches the one in VMA.  This way we can pass out enum straight through to VMA (and not have to include the 700kb vma header everywhere)
static_assert(static_cast<VmaMemoryUsage>(MemoryManager::MemoryUsage::GpuExclusive) == VMA_MEMORY_USAGE_GPU_ONLY, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryManager::MemoryUsage::CpuExclusive) == VMA_MEMORY_USAGE_CPU_ONLY, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryManager::MemoryUsage::CpuToGpu) == VMA_MEMORY_USAGE_CPU_TO_GPU, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryManager::MemoryUsage::GpuToCpu) == VMA_MEMORY_USAGE_GPU_TO_CPU, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryManager::MemoryUsage::CpuCopy) == VMA_MEMORY_USAGE_CPU_COPY, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryManager::MemoryUsage::GpuLazilyAllocated) == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED, "enum mismatch");

MemoryManager::MemoryManager()
{
}

///////////////////////////////////////////////////////////////////////////////

MemoryManager::~MemoryManager()
{
	vmaDestroyAllocator(mVmaAllocator);	// safe to pass nullptr
}

///////////////////////////////////////////////////////////////////////////////

bool MemoryManager::Initialize(VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice, VkInstance vkInstance, bool )
{
	assert(!mVmaAllocator);
	mGpuDevice = vkDevice;

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = vkPhysicalDevice;
	allocatorInfo.device = vkDevice;
	allocatorInfo.instance = vkInstance;
	allocatorInfo.flags = 0;

	VkResult result = vmaCreateAllocator(&allocatorInfo, &mVmaAllocator);
	if (result != VK_SUCCESS)
	{
		mVmaAllocator = nullptr;
		return false;
	}

	if ((allocatorInfo.flags & VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT) != 0)
	{
		PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(vkInstance, "vkGetDeviceProcAddr");
#if VK_KHR_buffer_device_address
		mFpGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddressKHR)fpGetDeviceProcAddr(vkDevice, "vkGetBufferDeviceAddressKHR");
#elif VK_EXT_buffer_device_address
		mFpGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddressEXT)fpGetDeviceProcAddr(vkDevice, "vkGetBufferDeviceAddressEXT");
#endif
		if (!mFpGetBufferDeviceAddress)
		{
			// When we go to Vulkan 1.2 switch to use vkGetBufferDeviceAddress
			assert(0 && "vkGetBufferDeviceAddressKHR/vkGetBufferDeviceAddressEXT not available, needed for VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT");
			return false;
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

MemoryVmaAllocatedBuffer<VkBuffer> MemoryManager::CreateBuffer(size_t size, VkBufferUsageFlags bufferUsage, MemoryManager::MemoryUsage memoryUsage, VkDescriptorBufferInfo* pDescriptorBufferInfo)
{
	assert(memoryUsage != MemoryUsage::Unknown);
	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = (VkDeviceSize)size;
	bufferInfo.usage = bufferUsage;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = static_cast<VmaMemoryUsage>(memoryUsage);
	
	MemoryVmaAllocatedBuffer<VkBuffer> vmaAllocatedBuffer;
	VmaAllocationInfo vmaAllocationInfo;
	VkResult result = vmaCreateBuffer(mVmaAllocator, &bufferInfo, &allocInfo, &vmaAllocatedBuffer.buffer, (VmaAllocation*)&vmaAllocatedBuffer.allocation.vmaAllocation, &vmaAllocationInfo);
	if (result != VK_SUCCESS)
	{
		return {};
	}
	if (pDescriptorBufferInfo)
	{
		*pDescriptorBufferInfo = { vmaAllocatedBuffer.GetVkBuffer(), 0, size };
	}
	return vmaAllocatedBuffer;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAbhAllocatedBuffer MemoryManager::CreateAndroidHardwareBuffer(size_t size, VkBufferUsageFlags bufferUsage, MemoryUsage memoryUsage)
{
	assert(0);
	return {};
}

///////////////////////////////////////////////////////////////////////////////

MemoryVmaAllocatedBuffer<VkImage> MemoryManager::CreateImage(const VkImageCreateInfo& imageInfo, MemoryManager::MemoryUsage memoryUsage)
{
	assert(memoryUsage != MemoryUsage::Unknown);
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = static_cast<VmaMemoryUsage>(memoryUsage);

	MemoryVmaAllocatedBuffer<VkImage> vmaAllocatedImage;
	VkResult result = vmaCreateImage(mVmaAllocator, &imageInfo, &allocInfo, &vmaAllocatedImage.buffer, (VmaAllocation*)&vmaAllocatedImage.allocation.vmaAllocation, nullptr);
	if (result != VK_SUCCESS)
	{
		return {};
	}
	return vmaAllocatedImage;
}

///////////////////////////////////////////////////////////////////////////////

void MemoryManager::Destroy(MemoryVmaAllocatedBuffer<VkBuffer> vmaAllocatedBuffer)
{
	assert(mVmaAllocator);
	vmaDestroyBuffer(mVmaAllocator, vmaAllocatedBuffer.buffer, static_cast<VmaAllocation>(vmaAllocatedBuffer.allocation.vmaAllocation));
	// Set the allocated buffer to a clean (deletable) state.
	vmaAllocatedBuffer.allocation.clear();
	vmaAllocatedBuffer.buffer = VK_NULL_HANDLE;
	// no error code from vmaDestroyBuffer so assume it worked (is also safe to pass an empty vmaAllocatedBuffer)
}

void MemoryManager::Destroy(MemoryVmaAllocatedBuffer<VkImage> vmaAllocatedImage)
{
	assert(mVmaAllocator);
	vmaDestroyImage(mVmaAllocator, vmaAllocatedImage.buffer, static_cast<VmaAllocation>(vmaAllocatedImage.allocation.vmaAllocation));
	// Set the allocated buffer to a clean (deletable) state.
	vmaAllocatedImage.allocation.clear();
	vmaAllocatedImage.buffer = VK_NULL_HANDLE;
	// no error code from vmaDestroyImage so assume it worked (is also safe to pass an empty vmaAllocatedImage)
}

VkDeviceAddress MemoryManager::GetBufferDeviceAddressInternal(VkBuffer buffer) const
{
	VkBufferDeviceAddressInfoEXT bufferAddressInfo = {};
	bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT;
	bufferAddressInfo.buffer = buffer;
	return mFpGetBufferDeviceAddress(mGpuDevice, &bufferAddressInfo);
}

void MemoryManager::MapInternal(void* vmaAllocation, void** outCpuLocation)
{
	assert(outCpuLocation != nullptr);
	assert(*outCpuLocation == nullptr);	// mapped twice?
	assert(vmaAllocation != nullptr);	// Likely caused by a double mapping of this data
	if (vmaMapMemory(mVmaAllocator, static_cast<VmaAllocation>(vmaAllocation), outCpuLocation) != VK_SUCCESS)
	{
		outCpuLocation = nullptr;
		assert(0);
	}
}

void MemoryManager::UnmapInternal(void* vmaAllocation, void* cpuLocation)
{
	assert(cpuLocation);
	vmaUnmapMemory(mVmaAllocator, static_cast<VmaAllocation>(vmaAllocation));
}
