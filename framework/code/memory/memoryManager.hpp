//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @defgroup Memory
/// Graphics api templated manager for gpu memory buffer allocation, destruction and mapping (to cpu).
/// 

#include "memory/memory.hpp"
#include "memoryMapped.hpp"

// forward declarations

/// @brief Memeory manager top level (template) class
/// We expect the graphics api specific implementations to completely specialize this template!
/// @tparam T_GFXAPI 
template<typename T_GFXAPI>
class MemoryManager
{
    MemoryManager(const MemoryManager<T_GFXAPI>&) = delete;
    MemoryManager& operator=(const MemoryManager<T_GFXAPI>&) = delete;
public:
    MemoryManager() = delete;       // This template class must be specialized

    static_assert(sizeof(MemoryManager<T_GFXAPI>) > 1);   // Ensure this class template is specialized (and not used as-is)
};
