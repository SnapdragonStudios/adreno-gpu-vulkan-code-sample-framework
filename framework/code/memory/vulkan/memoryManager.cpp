//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "memoryManager.hpp"
#include "vulkan/vulkan.hpp"
#include <cassert>

//
// Include the VulkanMemoryAllocator AND compile the implementation in this cpp (is a header-only library)
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vulkan/vulkan.h>
#include "VulkanMemoryAllocator/src/vk_mem_alloc.h"


// Ensure our enum matches the one in VMA.  This way we can pass out enum straight through to VMA (and not have to include the 700kb vma header everywhere)
static_assert(static_cast<VmaMemoryUsage>(MemoryUsage::GpuExclusive) == VMA_MEMORY_USAGE_GPU_ONLY, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryUsage::CpuExclusive) == VMA_MEMORY_USAGE_CPU_ONLY, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryUsage::CpuToGpu) == VMA_MEMORY_USAGE_CPU_TO_GPU, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryUsage::GpuToCpu) == VMA_MEMORY_USAGE_GPU_TO_CPU, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryUsage::CpuCopy) == VMA_MEMORY_USAGE_CPU_COPY, "enum mismatch");
static_assert(static_cast<VmaMemoryUsage>(MemoryUsage::GpuLazilyAllocated) == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED, "enum mismatch");

MemoryManager<Vulkan>::MemoryManager()
{
}

///////////////////////////////////////////////////////////////////////////////

MemoryManager<Vulkan>::~MemoryManager()
{
	Destroy();
}

///////////////////////////////////////////////////////////////////////////////

bool MemoryManager<Vulkan>::Initialize(VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice, VkInstance vkInstance, bool EnableBufferDeviceAddress)
{
	assert(!mVmaAllocator);
	mGpuDevice = vkDevice;

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = vkPhysicalDevice;
	allocatorInfo.device = vkDevice;
	allocatorInfo.instance = vkInstance;
	allocatorInfo.flags = EnableBufferDeviceAddress ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0;

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

void MemoryManager<Vulkan>::Destroy()
{
	vmaDestroyAllocator(mVmaAllocator);	// safe to pass nullptr
	mVmaAllocator = nullptr;
}

static VkBufferUsageFlags BufferUsageToVk(BufferUsageFlags usage)
{
    VkBufferUsageFlags vkUsage = 0;
    if ((usage & BufferUsageFlags::TransferSrc) != 0)
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if ((usage & BufferUsageFlags::TransferDst) != 0)
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if ((usage & BufferUsageFlags::Storage) != 0)
        vkUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if ((usage & BufferUsageFlags::Uniform) != 0)
        vkUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if ((usage & BufferUsageFlags::Index) != 0)
        vkUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if ((usage & BufferUsageFlags::Vertex) != 0)
        vkUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if ((usage & BufferUsageFlags::Indirect) != 0)
        vkUsage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if ((usage & BufferUsageFlags::AccelerationStructure) != 0)
    {
        vkUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
        vkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    if ((usage & BufferUsageFlags::AccelerationStructureBuild) != 0)
        vkUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    if ((usage & BufferUsageFlags::ShaderBindingTable) != 0)
        vkUsage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    if ((usage & BufferUsageFlags::ShaderDeviceAddress) != 0)
        vkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    static const BufferUsageFlags allKnown = BufferUsageFlags::TransferSrc | BufferUsageFlags::TransferDst | BufferUsageFlags::Storage | BufferUsageFlags::Uniform | BufferUsageFlags::Index | BufferUsageFlags::Vertex | BufferUsageFlags::Indirect | BufferUsageFlags::AccelerationStructure | BufferUsageFlags::AccelerationStructureBuild | BufferUsageFlags::ShaderBindingTable | BufferUsageFlags::ShaderDeviceAddress;
    if ((usage | allKnown) != allKnown)
    {
        assert(0 && "if check missing");
    }
    return vkUsage;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkBuffer> MemoryManager<Vulkan>::CreateBuffer(size_t size, BufferUsageFlags bufferUsage, MemoryUsage memoryUsage, VkDescriptorBufferInfo* pDescriptorBufferInfo)
{
	assert(memoryUsage != MemoryUsage::Unknown);
	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = (VkDeviceSize)size;
	bufferInfo.usage = BufferUsageToVk(bufferUsage);

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = static_cast<VmaMemoryUsage>(memoryUsage);
	
    MemoryAllocatedBuffer<Vulkan, VkBuffer> vmaAllocatedBuffer;
	VmaAllocationInfo vmaAllocationInfo;
	VkResult result = vmaCreateBuffer(mVmaAllocator, &bufferInfo, &allocInfo, &vmaAllocatedBuffer.buffer, (VmaAllocation*)&vmaAllocatedBuffer.allocation.allocation, &vmaAllocationInfo);
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

MemoryAbhAllocatedBuffer MemoryManager<Vulkan>::CreateAndroidHardwareBuffer(size_t size, BufferUsageFlags bufferUsage, MemoryUsage memoryUsage)
{
	assert(0 && "unimplemented");
	return {};
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkImage> MemoryManager<Vulkan>::CreateImage(const VkImageCreateInfo& imageInfo, MemoryUsage memoryUsage)
{
	assert(memoryUsage != MemoryUsage::Unknown);
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = static_cast<VmaMemoryUsage>(memoryUsage);

	MemoryAllocatedBuffer<Vulkan, VkImage> vmaAllocatedImage;
	VkResult result = vmaCreateImage(mVmaAllocator, &imageInfo, &allocInfo, &vmaAllocatedImage.buffer, (VmaAllocation*)&vmaAllocatedImage.allocation.allocation, nullptr);
	if (result != VK_SUCCESS)
	{
		return {};
	}
	return vmaAllocatedImage;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> MemoryManager<Vulkan>::AllocateMemory(size_t size, uint32_t memoryTypeBits)
{
	VkMemoryRequirements memoryRequirements{ .size = size, .memoryTypeBits = memoryTypeBits };

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
	allocInfo.memoryTypeBits = memoryTypeBits;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> vmaAllocatedMemory;
	VkResult result = vmaAllocateMemory(mVmaAllocator, &memoryRequirements, &allocInfo, (VmaAllocation*)&vmaAllocatedMemory.allocation.allocation, nullptr);
	if (result != VK_SUCCESS)
	{
		return {};
	}
	vmaAllocatedMemory.buffer = static_cast<VmaAllocation>(vmaAllocatedMemory.allocation.allocation)->GetMemory();
	return vmaAllocatedMemory;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkImage> MemoryManager<Vulkan>::BindImageToMemory(VkImage image, MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>&& memory) const
{
	VkResult result = vkBindImageMemory(mGpuDevice, image, static_cast<VmaAllocation>(memory.allocation.allocation)->GetMemory(), static_cast<VmaAllocation>(memory.allocation.allocation)->GetOffset());
	if (result != VK_SUCCESS)
	{
		return {};
	}
	MemoryAllocatedBuffer<Vulkan, VkImage> imageMemory;
	imageMemory.allocation = std::move(memory.allocation);
	imageMemory.buffer = image;
	return imageMemory;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkBuffer> MemoryManager<Vulkan>::BindBufferToMemory(VkBuffer buffer, MemoryAllocatedBuffer<Vulkan, VkDeviceMemory>&& memory) const
{
	VkResult result = vkBindBufferMemory(mGpuDevice, buffer, static_cast<VmaAllocation>(memory.allocation.allocation)->GetMemory(), static_cast<VmaAllocation>(memory.allocation.allocation)->GetOffset());
	if (result != VK_SUCCESS)
	{
		return {};
	}
	MemoryAllocatedBuffer<Vulkan, VkBuffer> bufferMemory;
	bufferMemory.allocation = std::move(memory.allocation);
	bufferMemory.buffer = buffer;
	return bufferMemory;
}

///////////////////////////////////////////////////////////////////////////////

bool MemoryManager<Vulkan>::CopyData( VkCommandBuffer vkCommandBuffer, const MemoryAllocatedBuffer<Vulkan, VkBuffer>& src, MemoryAllocatedBuffer<Vulkan, VkBuffer>& dst, size_t copySize, size_t srcOffset, size_t dstOffset)
{
    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = copySize;
    mVmaAllocator->GetVulkanFunctions().vkCmdCopyBuffer( vkCommandBuffer, src.buffer, dst.buffer, 1, &copyRegion );
    return true;
}

///////////////////////////////////////////////////////////////////////////////

void MemoryManager<Vulkan>::Destroy(MemoryAllocatedBuffer<Vulkan, VkBuffer> vmaAllocatedBuffer)
{
	assert(mVmaAllocator);
	vmaDestroyBuffer(mVmaAllocator, vmaAllocatedBuffer.buffer, static_cast<VmaAllocation>(vmaAllocatedBuffer.allocation.allocation));
	// Set the allocated buffer to a clean (deletable) state.
	vmaAllocatedBuffer.allocation.clear();
	vmaAllocatedBuffer.buffer = VK_NULL_HANDLE;
	// no error code from vmaDestroyBuffer so assume it worked (is also safe to pass an empty vmaAllocatedBuffer)
}

///////////////////////////////////////////////////////////////////////////////

void MemoryManager<Vulkan>::Destroy(MemoryAllocatedBuffer<Vulkan, VkImage> vmaAllocatedImage)
{
	assert(mVmaAllocator);
	vmaDestroyImage(mVmaAllocator, vmaAllocatedImage.buffer, static_cast<VmaAllocation>(vmaAllocatedImage.allocation.allocation));
	// Set the allocated buffer to a clean (deletable) state.
	vmaAllocatedImage.allocation.clear();
	vmaAllocatedImage.buffer = VK_NULL_HANDLE;
	// no error code from vmaDestroyImage so assume it worked (is also safe to pass an empty vmaAllocatedImage)
}

///////////////////////////////////////////////////////////////////////////////

void MemoryManager<Vulkan>::Destroy(MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> vmaAllocatedMemory)
{
	assert(mVmaAllocator);
    vmaFreeMemory(mVmaAllocator, (VmaAllocation_T*)vmaAllocatedMemory.allocation.allocation);
	// Set the allocated buffer to a clean (deletable) state.
	vmaAllocatedMemory.allocation.clear();
	vmaAllocatedMemory.buffer = VK_NULL_HANDLE;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> MemoryManager<Vulkan>::DestroyBufferOnly(MemoryAllocatedBuffer<Vulkan, VkBuffer> vmaAllocatedBuffer)
{
	assert(mVmaAllocator);
	vkDestroyBuffer(mGpuDevice, vmaAllocatedBuffer.buffer, nullptr);
	MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> allocatedMemory;
	allocatedMemory.buffer = static_cast<VmaAllocation>(vmaAllocatedBuffer.allocation.allocation)->GetMemory();
	allocatedMemory.allocation = std::move(vmaAllocatedBuffer.allocation);
	return allocatedMemory;
}

///////////////////////////////////////////////////////////////////////////////

MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> MemoryManager<Vulkan>::DestroyBufferOnly(MemoryAllocatedBuffer<Vulkan, VkImage> vmaAllocatedImage)
{
	assert(mVmaAllocator);
	vkDestroyImage(mGpuDevice, vmaAllocatedImage.buffer, nullptr);
	MemoryAllocatedBuffer<Vulkan, VkDeviceMemory> allocatedMemory;
	allocatedMemory.buffer = static_cast<VmaAllocation>(vmaAllocatedImage.allocation.allocation)->GetMemory();
	allocatedMemory.allocation = std::move(vmaAllocatedImage.allocation);
	return allocatedMemory;
}

///////////////////////////////////////////////////////////////////////////////

VkDeviceAddress MemoryManager<Vulkan>::GetBufferDeviceAddressInternal(VkBuffer buffer) const
{
	assert(mFpGetBufferDeviceAddress != nullptr);	// need EnableBufferDeviceAddress parameter to be set on Initialize

	VkBufferDeviceAddressInfoEXT bufferAddressInfo = {};
	bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT;
	bufferAddressInfo.buffer = buffer;
	return mFpGetBufferDeviceAddress(mGpuDevice, &bufferAddressInfo);
}

///////////////////////////////////////////////////////////////////////////////

void MemoryManager<Vulkan>::MapInternal(void* vmaAllocation, void** outCpuLocation)
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

///////////////////////////////////////////////////////////////////////////////

void MemoryManager<Vulkan>::UnmapInternal(void* vmaAllocation, void* cpuLocation)
{
	assert(cpuLocation);
	vmaUnmapMemory(mVmaAllocator, static_cast<VmaAllocation>(vmaAllocation));
}
