//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations


/// Simple wrapper around VkRenderPass or DirectX render target data used by BeginRenderPass and CreateGraphicsPipelineState.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// This template class expected to be specialized (if this template throws compiler errors then the code is not using the specialization classes which is an issue!)
/// @ingroup Material
template<typename T_GFXAPI>
class RenderPass
{
    RenderPass& operator=(const RenderPass<T_GFXAPI>&) = delete;
    RenderPass(const RenderPass<T_GFXAPI>&) = delete;
public:
    RenderPass() noexcept = delete;
    RenderPass(RenderPass<T_GFXAPI>&&) noexcept = delete;
    ~RenderPass() = delete;

    static_assert(sizeof(RenderPass<T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};

