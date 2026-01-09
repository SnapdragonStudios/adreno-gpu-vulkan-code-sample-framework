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
#include "../memoryMapped.hpp"

// forward declarations
class Dx12;
struct ID3D12Resource;
namespace D3D12MA {
    class Allocation;
};

/// Wraps the D3D12MA Allocation handle.  Represents an allocation out of 'dx12' memory.  Cannot be copied (only moved) - single owner.
/// Passed around between whomever is 'owner' of the allocation (when mapping etc)
/// @ingroup Memory
template<>
class MemoryAllocation<Dx12> final
{
    friend class MemoryManager<Dx12>;
    friend class MemoryDx12AllocatedBuffer;
public:
	MemoryAllocation(const MemoryAllocation&) = delete;
	MemoryAllocation& operator=(const MemoryAllocation&) = delete;
	MemoryAllocation() {}
	MemoryAllocation(MemoryAllocation&& other) noexcept;
	MemoryAllocation& operator=(MemoryAllocation&& other) noexcept;
	~MemoryAllocation() { assert(allocation == nullptr); }	// protect accidental deletion (leak)
	explicit operator bool() const { return allocation != nullptr; }
private:
	//void clear() { allocation = nullptr; }
	D3D12MA::Allocation* allocation = nullptr;
};


/// Represents a memory resource allocated by D3D12MA
/// @ingroup Memory
template<typename T_RESOURCETYPE>
class MemoryAllocatedBuffer<Dx12, T_RESOURCETYPE> final
{
    MemoryAllocatedBuffer(const MemoryAllocatedBuffer<Dx12, T_RESOURCETYPE>&) = delete;
    MemoryAllocatedBuffer& operator=(const MemoryAllocatedBuffer<Dx12, T_RESOURCETYPE>&) = delete;
public:
	friend class MemoryManager<Dx12>;
	// Restrict MemoryAllocatedBuffer to not be duplicated and not accidentally deleted (leaking memory).
    MemoryAllocatedBuffer(MemoryAllocatedBuffer<Dx12, T_RESOURCETYPE>&& other) noexcept;
    MemoryAllocatedBuffer& operator=(MemoryAllocatedBuffer<Dx12, T_RESOURCETYPE>&& other) noexcept;
    MemoryAllocatedBuffer() noexcept {}
    ~MemoryAllocatedBuffer() { assert(resource == nullptr); }
    auto* GetResource() const { return resource; }
    auto* GetResource() { return resource; }
    explicit operator bool() const { return static_cast<bool>(allocation); }
private:
	MemoryAllocation<Dx12> allocation;
    static_assert( std::is_pointer_v<T_RESOURCETYPE> ); // expecting T_RESOURCETYPE is a pointer, eg ID3D12Resource*
    T_RESOURCETYPE/*ID3D12Resource* */ resource = nullptr;
};


//
// MemoryAllocation implementation
//
inline MemoryAllocation<Dx12>::MemoryAllocation(MemoryAllocation<Dx12>&& other) noexcept
{
	allocation = other.allocation;
	other.allocation = nullptr;
}

inline MemoryAllocation<Dx12>& MemoryAllocation<Dx12>::operator=(MemoryAllocation<Dx12>&& other) noexcept
{
	if (&other != this) {
        assert(allocation==nullptr);
        allocation = other.allocation;
		other.allocation = nullptr;
	}
	return *this;
}


//
// MemoryDx12AllocatedBuffer implementation
//
template<typename T>
inline MemoryAllocatedBuffer<Dx12, T>::MemoryAllocatedBuffer(MemoryAllocatedBuffer<Dx12, T>&& other) noexcept
	: allocation(std::move(other.allocation))
{
	resource = other.resource;
	other.resource = 0;
}

template<typename T>
inline MemoryAllocatedBuffer<Dx12, T>& MemoryAllocatedBuffer<Dx12, T>::operator=(MemoryAllocatedBuffer<Dx12, T>&& other) noexcept
{
	if (this != &other) {
		allocation = std::move(other.allocation);
        assert(resource==nullptr);
		resource = other.resource;
		other.resource = 0;
	}
	return *this;
}


#if 0
//
// MemoryCpuMappedUntyped implementation
//
inline MemoryCpuMappedUntyped<Dx12>::MemoryCpuMappedUntyped(MemoryAllocation<Dx12>&& memoryToMap)
	: mAllocation(std::move(memoryToMap))
	, mCpuLocation(nullptr)
{
}

inline MemoryCpuMappedUntyped<Dx12>::MemoryCpuMappedUntyped(MemoryCpuMappedUntyped<Dx12>&& other) noexcept
	: mAllocation(std::move(other.mAllocation))
	, mCpuLocation(other.mCpuLocation)
{
	other.mCpuLocation = nullptr;
}

inline MemoryCpuMappedUntyped<Dx12>::~MemoryCpuMappedUntyped()
{
	assert(mCpuLocation == nullptr);	// should have been unmapped through the memorymanager
}
#endif // 0
