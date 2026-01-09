//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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



/// Wraps the Vk Allocation handle.  Represents an allocation out of 'vulkan' memory.  Cannot be copied (only moved) - single owner.
/// Passed around between whomever is 'owner' of the allocation (when mapping etc)
/// @ingroup Memory
template<>
class MemoryAllocation<Vulkan> final
{
    friend class MemoryManager<Vulkan>;
public:
    MemoryAllocation( const MemoryAllocation& ) = delete;
    MemoryAllocation& operator=( const MemoryAllocation& ) = delete;
    MemoryAllocation() {}
    MemoryAllocation( MemoryAllocation&& other ) noexcept;
    MemoryAllocation& operator=( MemoryAllocation&& other ) noexcept;
    ~MemoryAllocation() { assert( allocation == nullptr ); }	// protect accidental deletion (leak)
    explicit operator bool() const { return allocation != nullptr; }
private:
    void clear() { allocation = nullptr; }
    void* allocation = nullptr;             // anonymous handle (gpx api specific)
};

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
    const MemoryAllocation<Vulkan>& GetMemoryAllocation() const { return allocation; }
	explicit operator bool() const { return static_cast<bool>(allocation); }
private:
	MemoryAllocation<Vulkan> allocation;
	T_VKTYPE buffer = 0;	// the allocated vulkan buffer (or VK_NULL_HANDLE)
};

//
// MemoryAllocation implementation
//
inline MemoryAllocation<Vulkan>::MemoryAllocation( MemoryAllocation<Vulkan>&& other ) noexcept
{
    allocation = other.allocation;
    other.allocation = nullptr;
}

inline MemoryAllocation<Vulkan>& MemoryAllocation<Vulkan>::operator=( MemoryAllocation<Vulkan>&& other ) noexcept
{
    if (&other != this) {
        allocation = other.allocation;
        other.allocation = nullptr;
    }
    return *this;
}

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
