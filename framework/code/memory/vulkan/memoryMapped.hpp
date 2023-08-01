//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cassert>
#include <memory>
#include "memory/memoryMapped.hpp"


// forward declarations
class Vulkan;
template<typename T_VKTYPE> class MemoryAllocatedBuffer<Vulkan, T_VKTYPE>;


/// Vulkan template specialization of MemoryAllocatedBuffer
/// Represents a memory block allocated by VMA and that has either a VkImage or VkBuffer
/// @tparam T_VKTYPE underlying Vulkan buffer type - VkImage or VkBuffer
/// @ingroup Memory
template<typename T_VKTYPE /*VkImage or VkBuffer*/>
class MemoryAllocatedBuffer<Vulkan, T_VKTYPE>
{
    MemoryAllocatedBuffer(const MemoryAllocatedBuffer<Vulkan, T_VKTYPE>&) = delete;
    MemoryAllocatedBuffer& operator=(const MemoryAllocatedBuffer<Vulkan, T_VKTYPE>&) = delete;
public:
    template<class Vulkan> friend class MemoryManager;
	// Restrict MemoryAllocatedBuffer to not be duplicated and not accidentally deleted (leaking memory).
	MemoryAllocatedBuffer(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>&& other) noexcept;
	MemoryAllocatedBuffer& operator=(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>&& other) noexcept;
    MemoryAllocatedBuffer() noexcept {}
    const T_VKTYPE& GetVkBuffer() const { return buffer; }
	explicit operator bool() const { return static_cast<bool>(allocation); }
private:
	MemoryAllocation<Vulkan> allocation;
	T_VKTYPE buffer = 0;	// the allocated vulkan buffer (or VK_NULL_HANDLE)
};

//
// MemoryAllocatedBuffer vulkan specialization implementation
//
template<typename T_VKTYPE>
MemoryAllocatedBuffer<Vulkan, T_VKTYPE>::MemoryAllocatedBuffer(MemoryAllocatedBuffer<Vulkan, T_VKTYPE>&& other) noexcept
	: allocation(std::move(other.allocation))
{
	buffer = other.buffer;
	other.buffer = 0;
}

template<typename T_VKTYPE>
MemoryAllocatedBuffer<Vulkan, T_VKTYPE>& MemoryAllocatedBuffer<Vulkan, T_VKTYPE>::operator=(MemoryAllocatedBuffer&& other) noexcept
{
	if (this != &other) {
		allocation = std::move(other.allocation);
		buffer = other.buffer;
		other.buffer = 0;
	}
	return *this;
}
