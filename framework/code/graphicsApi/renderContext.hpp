//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations


/// Simple wrapper for a rendering context (RenderContext and Pipeline or a dynamic rendering state)
/// This template class expected to be specialized (if this template throws compiler errors then the code is not using the specialization classes which is an issue!)
/// @ingroup Material
template<typename T_GFXAPI>
class RenderContext
{
    RenderContext& operator=(const RenderContext<T_GFXAPI>&) = delete;
    RenderContext(const RenderContext<T_GFXAPI>&) = delete;
public:
    RenderContext() noexcept = delete;
    RenderContext(RenderContext<T_GFXAPI>&&) noexcept = delete;
    ~RenderContext() = delete;

    static_assert(sizeof(RenderContext<T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};

