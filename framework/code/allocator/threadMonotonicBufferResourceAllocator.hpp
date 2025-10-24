/*
* This file is based on work by Microsoft Corporation.
* Original license: Apache-2.0 WITH LLVM-exception
* Copyright (c) Microsoft Corporation.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
* Original file: "memory_resource"
*
* Modifications:
* - Add support for thread-specific memory allocation counting
* - Add memory leak guard (m_allocated_resources)
*/
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <array>
#include <functional>
#include <thread>
#include <unordered_set>
#include <memory_resource>
#include <assert.h>
#include "threadBufferResourceHelper.hpp"

namespace core
{
    class ThreadMonotonicBufferResourceAllocator : public std::pmr::memory_resource {
    public:
        explicit ThreadMonotonicBufferResourceAllocator(std::pmr::memory_resource* const _Upstream) noexcept // strengthened
            : _Resource{_Upstream} {} // initialize this resource with upstream

        ThreadMonotonicBufferResourceAllocator(const size_t _Initial_size, std::pmr::memory_resource* const _Upstream) noexcept // strengthened
            : _Next_buffer_size(_Round(_Initial_size)), _Resource{_Upstream} {
            // initialize this resource with upstream and initial allocation size
        }

        ThreadMonotonicBufferResourceAllocator(void* const _Buffer, const size_t _Buffer_size,
            std::pmr::memory_resource* const _Upstream) noexcept // strengthened
            : _Current_buffer(_Buffer), _Space_available(_Buffer_size),
              _Next_buffer_size(_Buffer_size ? _Scale(_Buffer_size) : _Min_allocation), _Resource{_Upstream} {
            // initialize this resource with upstream and initial buffer
        }

        ThreadMonotonicBufferResourceAllocator() = default;

        explicit ThreadMonotonicBufferResourceAllocator(const size_t _Initial_size) noexcept // strengthened
            : _Next_buffer_size(_Round(_Initial_size)) {} // initialize this resource with initial allocation size

        ThreadMonotonicBufferResourceAllocator(void* const _Buffer, const size_t _Buffer_size) noexcept // strengthened
            : _Current_buffer(_Buffer), _Space_available(_Buffer_size),
              _Next_buffer_size(_Buffer_size ? _Scale(_Buffer_size) : _Min_allocation) {
            // initialize this resource with initial buffer
        }

        ~ThreadMonotonicBufferResourceAllocator() noexcept override {
            release();
        }

        ThreadMonotonicBufferResourceAllocator(const ThreadMonotonicBufferResourceAllocator&)            = delete;
        ThreadMonotonicBufferResourceAllocator& operator=(const ThreadMonotonicBufferResourceAllocator&) = delete;

        void release() noexcept /* strengthened */ {

#ifndef SAAA_SHIPPING_BUILD
            // If this was hit, you are potentially leaking memory or returning locally allocated memory to
            // the callee without the callee guarding such memory
            // assert(m_allocated_resources.empty());
            assert(m_allocation_count == 0);
#endif

            if (_Chunks._Empty()) {
                // nothing to release; potentially continues to use an initial block provided at construction
                return;
            }

            _Current_buffer  = nullptr;
            _Space_available = 0;

            // unscale _Next_buffer_size so the next allocation will be the same size as the most recent allocation
            // (keep synchronized with ThreadMonotonicBufferResourceAllocator::_Scale)
            const size_t _Unscaled = (_Next_buffer_size / 3 * 2 + alignof(_Header) - 1) & _Max_allocation;
            _Next_buffer_size      = (std::max)(_Unscaled, _Min_allocation);

            StackBuffer<_Header> _Tmp{};
            std::swap(_Tmp, _Chunks);
            while (!_Tmp._Empty()) {
                const auto _Ptr = _Tmp._Pop();
                _Resource->deallocate(_Ptr->_Base_address(), _Ptr->_Size, _Ptr->_Align);
            }
        }

        [[nodiscard]] std::pmr::memory_resource* upstream_resource() const noexcept /* strengthened */ {
            // retrieve the upstream resource
            return _Resource;
        }

        void* allocate_released(const size_t _Bytes, const size_t _Align)
        {
#ifndef SAAA_SHIPPING_BUILD
            // If this was hit, the thread memory resource scope isn't available when the allocation
            // was performed, ensure the ThreadMonotonicMemoryScope is left alive (it can be used
            // recursivelly between function calls as long as the very first callee retains an active
            // scope)
            assert(s_buffer_monotonic_resource_usage_count > 0);
#endif

            // allocate from the current buffer or a new larger buffer from upstream
            if (!std::align(_Align, _Bytes, _Current_buffer, _Space_available)) {
                _Increase_capacity(_Bytes, _Align);
            }

            void* const _Result = _Current_buffer;
            _Current_buffer = static_cast<char*>(_Current_buffer) + _Bytes;
            _Space_available -= _Bytes;

            return _Result;
        }

    protected:
        void* do_allocate(const size_t _Bytes, const size_t _Align) override {

#ifndef SAAA_SHIPPING_BUILD
            // If this was hit, the thread memory resource scope isn't available when the allocation
            // was performed, ensure the ThreadMonotonicMemoryScope is left alive (it can be used
            // recursivelly between function calls as long as the very first callee retains an active
            // scope)
            assert(s_buffer_monotonic_resource_usage_count > 0);
#endif

            // allocate from the current buffer or a new larger buffer from upstream
            if (!std::align(_Align, _Bytes, _Current_buffer, _Space_available)) {
                _Increase_capacity(_Bytes, _Align);
            }

            void* const _Result = _Current_buffer;
            _Current_buffer     = static_cast<char*>(_Current_buffer) + _Bytes;
            _Space_available -= _Bytes;

#ifndef SAAA_SHIPPING_BUILD
            // m_allocated_resources.emplace(_Result);
            m_allocation_count++;
#endif

            return _Result;
        }

        void do_deallocate(void* data, size_t, size_t) override 
        {
#ifndef SAAA_SHIPPING_BUILD
            // m_allocated_resources.erase(data);
            m_allocation_count--;
#endif
        }

        bool do_is_equal(const memory_resource& _That) const noexcept override {
            return this == &_That;
        }

    private:
        struct _Header : StackBufferEntryLink<> { // track the size and alignment of an allocation from upstream
            size_t _Size;
            size_t _Align;

            _Header(const size_t _Size_, const size_t _Align_) : _Size{_Size_}, _Align{_Align_} {}

            void* _Base_address() const { // header is stored at the end of the allocated memory block
                return const_cast<char*>(reinterpret_cast<const char*>(this + 1) - _Size);
            }
        };

        static constexpr size_t _Min_allocation = 2 * sizeof(_Header);
        static constexpr size_t _Max_allocation = 0 - alignof(_Header);

        static constexpr size_t _Round(const size_t _Size) noexcept {
            // return the smallest multiple of alignof(_Header) greater than _Size,
            // clamped to the range [_Min_allocation, _Max_allocation]
            if (_Size < _Min_allocation) {
                return _Min_allocation;
            }

            if (_Size >= _Max_allocation) {
                return _Max_allocation;
            }

            // Since _Max_allocation == -alignof(_Header), _Size < _Max_allocation implies that
            // (_Size + alignof(_Header) - 1) does not overflow.
            return (_Size + alignof(_Header) - 1) & _Max_allocation;
        }

        static constexpr size_t _Scale(const size_t _Size) noexcept {
            // scale _Size by 1.5, rounding up to a multiple of alignof(_Header), saturating to _Max_allocation
            // (keep synchronized with ThreadMonotonicBufferResourceAllocator::release)
#pragma warning(push)
#pragma warning(disable : 26450) // TRANSITION, VSO-1828677
            constexpr auto _Max_size = (_Max_allocation - alignof(_Header) + 1) / 3 * 2;
#pragma warning(pop)
            if (_Size >= _Max_size) {
                return _Max_allocation;
            }

            return (_Size + (_Size + 1) / 2 + alignof(_Header) - 1) & _Max_allocation;
        }

        void _Increase_capacity(const size_t _Bytes, const size_t _Align) { // obtain a new buffer from upstream
            if (_Bytes > _Max_allocation - sizeof(_Header)) {
                throw;
            }

            size_t _New_size = _Next_buffer_size;
            if (_New_size < _Bytes + sizeof(_Header)) {
                _New_size = (_Bytes + sizeof(_Header) + alignof(_Header) - 1) & _Max_allocation;
            }

            const size_t _New_align = (std::max)(alignof(_Header), _Align);

            void* _New_buffer = _Resource->allocate(_New_size, _New_align);
            CheckMemoryResourceAlignment(_New_buffer, _New_align);

            _Current_buffer  = _New_buffer;
            _Space_available = _New_size - sizeof(_Header);
            _New_buffer      = static_cast<char*>(_New_buffer) + _Space_available;
            _Chunks._Push(::new (_New_buffer) _Header{_New_size, _New_align});

            _Next_buffer_size = _Scale(_New_size);
        }

        void* _Current_buffer    = nullptr; // current memory block to parcel out to callers
        size_t _Space_available  = 0; // space remaining in current block
        size_t _Next_buffer_size = _Min_allocation; // size of next block to allocate from upstream
        StackBuffer<_Header> _Chunks{}; // list of memory blocks allocated from upstream
        std::pmr::memory_resource* _Resource = std::pmr::get_default_resource(); // upstream resource from which to allocate

#ifndef SAAA_SHIPPING_BUILD
        // std::unordered_set<void*> m_allocated_resources;
        uint32_t                  m_allocation_count = 0;
#endif
    };
}