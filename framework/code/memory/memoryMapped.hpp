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

// forward declarations
template<typename T_VKTYPE> class MemoryVmaAllocatedBuffer;


/// Wraps the VMA VmaAllocation handle.  Represents an allocation out of 'vulkan' memory.  Cannot be copied (only moved) - single owner.
/// Passed around between whomever is 'owner' of the allocation (when mapping etc)
/// @ingroup Memory
class MemoryVmaAllocation
{
	friend class MemoryManager;
public:
	MemoryVmaAllocation(const MemoryVmaAllocation&) = delete;
	MemoryVmaAllocation& operator=(const MemoryVmaAllocation&) = delete;
	MemoryVmaAllocation() {}
	MemoryVmaAllocation(MemoryVmaAllocation&& other) noexcept;
	MemoryVmaAllocation& operator=(MemoryVmaAllocation&& other) noexcept;
	~MemoryVmaAllocation() { assert(!vmaAllocation); }	// protect accidental deletion (leak)
	explicit operator bool() const { return vmaAllocation != nullptr; }
private:
	void clear() { vmaAllocation = nullptr; }
	void* vmaAllocation = nullptr;			// actually a VmaAllocation, but dont want to pollute everything with he vma allocator header when the MemoryManager should be the only entry point
};


/// Represents a memory block allocated by VMA and that has either a VkImage or VkBuffer
/// @tparam T_VKTYPE underlying Vulkan buffer type - VkImage or VkBuffer
/// @ingroup Memory
template<typename T_VKTYPE /*VkImage or VkBuffer*/>
class MemoryVmaAllocatedBuffer
{
	MemoryVmaAllocatedBuffer(const MemoryVmaAllocatedBuffer&) = delete;
	MemoryVmaAllocatedBuffer& operator=(const MemoryVmaAllocatedBuffer&) = delete;
public:
	friend class MemoryManager;
	// Restrict MemoryVmaAllocatedBuffer to not be duplicated and not accidentally deleted (leaking memory).
	MemoryVmaAllocatedBuffer(MemoryVmaAllocatedBuffer&& other) noexcept;
	MemoryVmaAllocatedBuffer& operator=(MemoryVmaAllocatedBuffer&& other) noexcept;
	MemoryVmaAllocatedBuffer() {}
	const T_VKTYPE& GetVkBuffer() const { return buffer; }
	explicit operator bool() const { return static_cast<bool>(allocation); }
private:
	MemoryVmaAllocation allocation;
	T_VKTYPE buffer = VK_NULL_HANDLE;	// the allocated vulkan buffer (or VK_NULL_HANDLE)
};


/// Handle for a memory block that is mapped for cpu use.
/// Holds ownership of the allocated memory block while it has cpu access.
/// User is expected to call UnMap to move owership of the memory block back away from the cpu guard.
/// @note consider using the MemoryCpuMapped template which provides a zero cost wrapper around this void*
/// @ingroup Memory
class MemoryCpuMappedUntyped
{
	MemoryCpuMappedUntyped() = delete;
	MemoryCpuMappedUntyped(const MemoryCpuMappedUntyped&) = delete;
	MemoryCpuMappedUntyped& operator=(const MemoryCpuMappedUntyped&) = delete;
public:
	MemoryCpuMappedUntyped(MemoryVmaAllocation&&);
	MemoryCpuMappedUntyped(MemoryCpuMappedUntyped&&) noexcept;
	//MemoryCpuMappedUntyped& operator=(MemoryCpuMappedUntyped&&);
	~MemoryCpuMappedUntyped();
	//size_t size() const { return mAllocation.size(); }
	void* data() const { return mCpuLocation; }
private:
	friend class MemoryManager;
	MemoryVmaAllocation mAllocation;
	void* mCpuLocation = nullptr;
};


/// Provides a poiner to cpu mapped memory.
/// Represents a templated pointer in to a CPU mapped allocation.  Owns a MemoryVmaAllocation during its lifetime.
/// @tparam T data type we want to map to (saves on nasty pointer casting etc).
/// @ingroup Memory
template<typename T>
class MemoryCpuMapped : public MemoryCpuMappedUntyped
{
public:
	MemoryCpuMapped() = delete;
	MemoryCpuMapped(const MemoryCpuMapped&) = delete;
	MemoryCpuMapped& operator=(const MemoryCpuMapped&) = delete;
	MemoryCpuMapped& operator=(MemoryCpuMapped&& other) = delete;
	//	{
	//		return MemoryCpuMappedUntyped::operator=(std::move(other));
	//	}
	MemoryCpuMapped(MemoryCpuMapped<T>&& mapped)
		: MemoryCpuMappedUntyped(std::move(mapped))
	{
	}
	MemoryCpuMapped(MemoryCpuMappedUntyped&& mapped)
		: MemoryCpuMappedUntyped(std::move(mapped))
	{
	}
	T* data() { return static_cast<T*>(MemoryCpuMappedUntyped::data()); }
protected:
};


//
// MemoryVmaAllocation implementation
//
inline MemoryVmaAllocation::MemoryVmaAllocation(MemoryVmaAllocation&& other) noexcept
{
	vmaAllocation = other.vmaAllocation;
	other.vmaAllocation = nullptr;
}

inline MemoryVmaAllocation& MemoryVmaAllocation::operator=(MemoryVmaAllocation&& other) noexcept
{
	if (&other != this) {
		vmaAllocation = other.vmaAllocation;
		other.vmaAllocation = nullptr;
	}
	return *this;
}


//
// MemoryVmaAllocatedBuffer implementation
//
template<typename T_VKTYPE>
MemoryVmaAllocatedBuffer<T_VKTYPE>::MemoryVmaAllocatedBuffer(MemoryVmaAllocatedBuffer&& other) noexcept
	: allocation(std::move(other.allocation))
{
	buffer = other.buffer;
	other.buffer = VK_NULL_HANDLE;
}

template<typename T_VKTYPE>
MemoryVmaAllocatedBuffer<T_VKTYPE>& MemoryVmaAllocatedBuffer<T_VKTYPE>::operator=(MemoryVmaAllocatedBuffer&& other) noexcept
{
	if (this != &other) {
		allocation = std::move(other.allocation);
		buffer = other.buffer;
		other.buffer = VK_NULL_HANDLE;
	}
	return *this;
}


//
// MemoryCpuMappedUntyped implementation
//
inline MemoryCpuMappedUntyped::MemoryCpuMappedUntyped(MemoryVmaAllocation&& memoryToMap)
	: mAllocation(std::move(memoryToMap))
	, mCpuLocation(nullptr)
{
}

inline MemoryCpuMappedUntyped::MemoryCpuMappedUntyped(MemoryCpuMappedUntyped&& other) noexcept
	: mAllocation(std::move(other.mAllocation))
	, mCpuLocation(other.mCpuLocation)
{
	other.mCpuLocation = nullptr;
}

inline MemoryCpuMappedUntyped::~MemoryCpuMappedUntyped()
{
	assert(mCpuLocation == nullptr);	// should have been unmapped through the memorymanager
}
