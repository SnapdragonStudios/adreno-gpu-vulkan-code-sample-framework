//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <array>
#include <functional>
#include <unordered_set>
#include <memory_resource>

#include "threadManagedBufferResourceAllocator.hpp"
#include "threadMonotonicBufferResourceAllocator.hpp"

namespace core
{
    inline std::span<std::byte> GetThreadMonotonicLocalBufferResourceBlob()
    {
        static thread_local std::array<std::byte, 2024> buffer;
        return buffer;
    }
    
    inline std::span<std::byte> GetThreadManagedLocalBufferResourceBlob()
    {
        static thread_local std::array<std::byte, 2024> buffer;
        return buffer;
    }

    thread_local ThreadMonotonicBufferResourceAllocator s_monotonic_buffer_resource(GetThreadMonotonicLocalBufferResourceBlob().data(), GetThreadMonotonicLocalBufferResourceBlob().size());
    thread_local ThreadManagedBufferResourceAllocator   s_managed_buffer_resource(GetThreadManagedLocalBufferResourceBlob().data(), GetThreadManagedLocalBufferResourceBlob().size());
}

namespace core
{
////////////////////////////////////////////////////////////////////////////////
// Class name: ThreadMonotonicMemoryScope
////////////////////////////////////////////////////////////////////////////////
class ThreadMonotonicMemoryScope
{
public:

    ThreadMonotonicMemoryScope()
    {
        s_buffer_monotonic_resource_usage_count++;
    }

    ~ThreadMonotonicMemoryScope()
    {
        s_buffer_monotonic_resource_usage_count--;
        if (s_buffer_monotonic_resource_usage_count == 0)
        {
            s_monotonic_buffer_resource.release();
        }
    }

    inline ThreadMonotonicBufferResourceAllocator& GetResource()
    {
        return s_monotonic_buffer_resource;
    }

    inline operator ThreadMonotonicBufferResourceAllocator& ()
    {
        return s_monotonic_buffer_resource;
    }
    
    inline operator ThreadMonotonicBufferResourceAllocator* ()
    {
        return &s_monotonic_buffer_resource;
    }

    template<typename _Ty>
    inline operator std::pmr::polymorphic_allocator<_Ty>()
    {
        return std::pmr::polymorphic_allocator<_Ty>{ &s_monotonic_buffer_resource };
    }

    /*
    * Allocates an object with lifetime tied to the monotonic buffer resource, returning a reference to it
    */
    template<typename _Ty, typename... Args>
    std::unique_ptr<_Ty, std::function<void(_Ty*)>> AllocateUniquePtr(Args&&... args)
    {
        auto& memory_resource = GetResource();
        _Ty* ptr = memory_resource.allocate(sizeof(_Ty));

        ::new((void*)ptr) _Ty(std::forward<Args>(args)...);

        auto deleter = [](_Ty* p, std::pmr::memory_resource& alloc)
        {
            p->~_Ty();
            alloc.deallocate(p, sizeof(_Ty));
        };

        return { ptr, std::bind(deleter, std::placeholders::_1, memory_resource) };
    }

    /*
    * Allocates an object with lifetime tied to the monotonic buffer resource, returning a reference to it
    * No destructor is called when it goes out of scope, for unique_ptr behavior use the AllocateUniquePtr()
    */
    template<typename _Ty, typename... Args>
    _Ty& AllocateObject(Args&&... args)
    {
        auto& memory_resource = GetResource();
        auto allocation = memory_resource.allocate_released(sizeof(_Ty), alignof(_Ty));
        _Ty* ptr = static_cast<_Ty*>(allocation);
        ::new((void*)ptr) _Ty(std::forward<Args>(args)...);

        return *ptr;
    }

    /*
    * Allocates one or more objects with lifetime tied to the monotonic buffer resource, returning a reference 
    * to it
    * No destructor is called when they go out of scope
    */
    template<typename _Ty, typename... Args>
    std::span<_Ty> AllocateObjects(std::size_t count, Args&&... args)
    {
        auto& memory_resource = GetResource();
        _Ty* ptr = static_cast<_Ty*>(memory_resource.allocate_released(sizeof(_Ty) * count, alignof(_Ty)));

        for (std::size_t i = 0; i < count; ++i)
        {
            ::new((void*)&ptr[i]) _Ty(std::forward<Args>(args)...);
        }
        
        return std::span(ptr, count);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Class name: ThreadAutomaticMonotonicMemoryResource
////////////////////////////////////////////////////////////////////////////////
class ThreadAutomaticMonotonicMemoryResource
{
public:

    ThreadAutomaticMonotonicMemoryResource() = default;
    ~ThreadAutomaticMonotonicMemoryResource() = default;

    inline std::pmr::memory_resource& GetResource()
    {
        return s_managed_buffer_resource;
    }

    inline operator std::pmr::memory_resource& ()
    {
        return s_managed_buffer_resource;
    }

    inline operator std::pmr::memory_resource* ()
    {
        return &s_managed_buffer_resource;
    }

    template<typename _Ty>
    inline operator std::pmr::polymorphic_allocator<_Ty>()
    {
        return std::pmr::polymorphic_allocator<_Ty>{ &GetResource() };
    }

    /*
    * Allocates an object with lifetime tied to the monotonic buffer resource, returning a reference to it
    */
    template<typename _Ty, typename... Args>
    std::unique_ptr<_Ty, std::function<void(_Ty*)>> AllocateUniquePtr(Args&&... args)
    {
        auto& memory_resource = GetResource();
        _Ty* ptr = memory_resource.allocate(sizeof(_Ty));

        ::new((void*)ptr) _Ty(std::forward<Args>(args)...);

        auto deleter = [](_Ty* p, std::pmr::memory_resource& alloc)
        {
            p->~_Ty();
            alloc.deallocate(p, sizeof(_Ty));
        };

        return { ptr, std::bind(deleter, std::placeholders::_1, memory_resource) };
    }
};
}