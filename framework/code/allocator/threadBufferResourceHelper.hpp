/*
* This file is based on work by Microsoft Corporation.
* Original license: Apache-2.0 WITH LLVM-exception
* Copyright (c) Microsoft Corporation.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
* Original file: "memory_resource"
*
* Modifications:
* - Add support for thread-specific memory allocation counting
*/
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <array>
#include <functional>
#include <unordered_set>
#include <memory_resource>

namespace core
{
    thread_local uint32_t s_buffer_monotonic_resource_usage_count = 0;

    inline void CheckMemoryResourceAlignment(void* const _Ptr, const size_t _Align) noexcept
    {
#if defined(_STL_ASSERT)
        _STL_ASSERT((reinterpret_cast<uintptr_t>(_Ptr) & (_Align - 1)) == 0,
            "Upstream resource did not respect alignment requirement.");
#endif
        (void)_Ptr;
        (void)_Align;
    }

    //////////////////
    // STACK BUFFER //
    //////////////////
    // Supporting stack buffer for buffer resource types

    template <class _Tag = void>
    struct StackBufferEntryLink // base class for intrusive singly-linked structures
    { 
        StackBufferEntryLink* _Next;
    };

    template <class _Ty, class _Tag = void>
    struct StackBuffer { // intrusive stack of _Ty, which must derive from StackBufferEntryLink<_Tag>
        using _Link_type = StackBufferEntryLink<_Tag>;

#if defined(_STL_INTERNAL_STATIC_ASSERT)
        _STL_INTERNAL_STATIC_ASSERT(is_base_of_v<_Link_type, _Ty>);
#endif

        constexpr StackBuffer() noexcept = default;
        constexpr StackBuffer(StackBuffer&& _That) noexcept : _Head{_That._Head} {
            _That._Head = nullptr;
        }
        constexpr StackBuffer& operator=(StackBuffer&& _That) noexcept {
            _Head       = _That._Head;
            _That._Head = nullptr;
            return *this;
        }

        static constexpr _Link_type* _As_link(_Ty* const _Ptr) noexcept {
            return static_cast<_Link_type*>(_Ptr);
        }

        static constexpr _Ty* _As_item(_Link_type* const _Ptr) noexcept {
            return static_cast<_Ty*>(_Ptr);
        }

        constexpr bool _Empty() const noexcept {
            return _Head == nullptr;
        }

        constexpr _Ty* _Top() const noexcept {
            return _As_item(_Head);
        }

        constexpr void _Push(_Ty* const _Item) noexcept {
            const auto _Ptr = _As_link(_Item);
            _Ptr->_Next     = _Head;
            _Head           = _Ptr;
        }

        constexpr _Ty* _Pop() noexcept { // pre: _Head != nullptr
            const auto _Result = _Head;
            _Head              = _Head->_Next;
            return _As_item(_Result);
        }

        constexpr void _Remove(_Ty* const _Item) noexcept {
            const auto _Ptr = _As_link(_Item);
            for (_Link_type** _Pnext = &_Head; *_Pnext; _Pnext = &(*_Pnext)->_Next) {
                if (*_Pnext == _Ptr) {
                    *_Pnext = _Ptr->_Next;
                    break;
                }
            }
        }

        _Link_type* _Head = nullptr;
    };

    //////////////////
    //////////////////
    //////////////////
}