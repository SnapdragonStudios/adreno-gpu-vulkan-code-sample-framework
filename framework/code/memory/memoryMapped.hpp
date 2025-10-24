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

// Forward declarations
template<class T_GFXAPI> class MemoryManager;
template<class T_GFXAPI> class MemoryAllocation;


/// Wraps the VMA or D3D12MA allocation handle.  Represents an allocation out of 'gpu' memory.  Cannot be copied (only moved) - single owner.
/// Passed around between whomever is 'owner' of the allocation (when mapping etc)
/// Expected for this class to be specialized for each graphics api.
/// @ingroup Memory
template<typename T_GFXAPI>
class MemoryAllocation final
{
public:
    MemoryAllocation() noexcept {}       // This class is expected to be specialized!
    ~MemoryAllocation() = delete;        // This class is expected to be specialized!
protected:
    static_assert(sizeof( MemoryAllocation<T_GFXAPI> ) >= 1);   // Ensure this class template is specialized (and not used as-is).  Maybe the correct #include is not being pulled in!
};


/// Represents a memory block allocated on the gfx device and that has an associated buffer/resource handle alongside the allocation
/// @tparam T_BUFFERTYPE underlying buffer/resource type - eg VkImage or VkBuffer on Vulkan
/// @ingroup Memory
template<typename T_GFXAPI, typename T_BUFFERTYPE /*VkImage or VkBuffer or ID3D12Resource*/>
class MemoryAllocatedBuffer
{
    MemoryAllocatedBuffer(const MemoryAllocatedBuffer<T_GFXAPI, T_BUFFERTYPE>&) = delete;
    MemoryAllocatedBuffer& operator=(const MemoryAllocatedBuffer<T_GFXAPI, T_BUFFERTYPE>&) = delete;
public:
    friend class MemoryManager<T_GFXAPI>;
    // Restrict MemoryAllocatedBuffer to not be duplicated and not accidentally deleted (leaking memory).
    MemoryAllocatedBuffer(MemoryAllocatedBuffer<T_GFXAPI, T_BUFFERTYPE>&& other) noexcept;
    MemoryAllocatedBuffer& operator=(MemoryAllocatedBuffer<T_GFXAPI, T_BUFFERTYPE>&& other) noexcept;
    MemoryAllocatedBuffer() noexcept {}
    explicit operator bool() const { return static_cast<bool>(allocation); }
private:
    MemoryAllocation<T_GFXAPI> allocation;
};


/// Handle for a memory block that is mapped for cpu use.
/// Holds ownership of the allocated memory block while it has cpu access.
/// User is expected to call UnMap to move owership of the memory block back away from the cpu guard.
/// @note consider using the MemoryCpuMapped template which provides a zero cost wrapper around this void*
/// @ingroup Memory
template<typename T_GFXAPI>
class MemoryCpuMappedUntyped
{
    MemoryCpuMappedUntyped() = delete;
    MemoryCpuMappedUntyped(const MemoryCpuMappedUntyped&) = delete;
    MemoryCpuMappedUntyped& operator=(const MemoryCpuMappedUntyped&) = delete;
public:
    MemoryCpuMappedUntyped(MemoryAllocation<T_GFXAPI>&&) noexcept;
    MemoryCpuMappedUntyped(MemoryCpuMappedUntyped<T_GFXAPI>&&) noexcept;
    ~MemoryCpuMappedUntyped();
    //size_t size() const { return mAllocation.size(); }
    void* data() const { return mCpuLocation; }
private:
    friend class MemoryManager<T_GFXAPI>;
    MemoryAllocation<T_GFXAPI> mAllocation;
    void* mCpuLocation = nullptr;
};

template<typename T_GFXAPI>
MemoryCpuMappedUntyped< T_GFXAPI>::MemoryCpuMappedUntyped(MemoryAllocation<T_GFXAPI>&& memoryToMap) noexcept
    : mAllocation(std::move(memoryToMap))
    , mCpuLocation(nullptr)
{
}

template<typename T_GFXAPI>
MemoryCpuMappedUntyped<T_GFXAPI>::MemoryCpuMappedUntyped(MemoryCpuMappedUntyped<T_GFXAPI>&& other) noexcept
    : mAllocation(std::move(other.mAllocation))
    , mCpuLocation(other.mCpuLocation)
{
    other.mCpuLocation = nullptr;
}

//
// MemoryCpuMappedUntyped implementation
//
template<typename T_GFXAPI>
MemoryCpuMappedUntyped<T_GFXAPI>::~MemoryCpuMappedUntyped()
{
    assert(mCpuLocation == nullptr);	// should have been unmapped through the memorymanager
}

/// Provides a poiner to cpu mapped memory.
/// Represents a templated pointer in to a CPU mapped allocation.  Owns a MemoryAllocation during its lifetime.
/// @tparam T data type we want to map to (saves on nasty pointer casting etc).
/// @ingroup Memory
template<typename T_GFXAPI, typename T>
class MemoryCpuMapped : public MemoryCpuMappedUntyped<T_GFXAPI>
{
public:
    MemoryCpuMapped() = delete;
    MemoryCpuMapped(const MemoryCpuMapped&) = delete;
    MemoryCpuMapped& operator=(const MemoryCpuMapped&) = delete;
    MemoryCpuMapped& operator=(MemoryCpuMapped&& other) = delete;
    MemoryCpuMapped(MemoryCpuMapped<T_GFXAPI, T>&& mapped)
        : MemoryCpuMappedUntyped<T_GFXAPI>(std::move(mapped))
    {
    }
    MemoryCpuMapped(MemoryCpuMappedUntyped<T_GFXAPI>&& mapped)
        : MemoryCpuMappedUntyped<T_GFXAPI>(std::move(mapped))
    {
    }
    T* data() { return static_cast<T*>(MemoryCpuMappedUntyped<T_GFXAPI>::data()); }
protected:
};
