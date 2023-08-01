//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations
class DescriptorSetLayout;


/// Simple wrapper around VkPipelineLayout or RootSignature.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// This template class expected to be specialized (if this template throws compiler errors then the code is not using the specialization classes which is an issue!)
/// @ingroup Material
template<typename T_GFXAPI>
class PipelineLayout
{
	PipelineLayout& operator=(const PipelineLayout<T_GFXAPI>&) = delete;
	PipelineLayout(const PipelineLayout<T_GFXAPI>&) = delete;
public:
    PipelineLayout() noexcept = delete;
	PipelineLayout(PipelineLayout<T_GFXAPI>&&) noexcept = delete;
	~PipelineLayout() = delete;

    static_assert(sizeof(PipelineLayout<T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};
