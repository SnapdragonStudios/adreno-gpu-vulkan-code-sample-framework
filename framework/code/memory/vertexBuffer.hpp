//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

///
/// Vertex buffer template (platform agnostic)
/// @group memory
/// 

#include "buffer.hpp"
#include <vector>

// forward declarations
class VertexFormat;

/// Buffer containing vertex data.
/// @ingroup Memory
template<typename T_GFXAPI>
class VertexBuffer : public Buffer<T_GFXAPI>
{
    ~VertexBuffer() noexcept = delete;                                          // Ensure this class template is specialized (and not used as-is)
    static_assert(sizeof(VertexBuffer<T_GFXAPI>) != sizeof(Buffer<T_GFXAPI>));  // Ensure this class template is specialized (and not used as-is)
};
