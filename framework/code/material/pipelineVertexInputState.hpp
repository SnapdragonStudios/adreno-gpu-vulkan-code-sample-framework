//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations
class ShaderDescription;


/// Helper for making VkPipelineVertexInputStateCreateInfo.
/// @ingroup Material
template<typename T_GFXAPI>
class PipelineVertexInputState
{
    PipelineVertexInputState(const PipelineVertexInputState<T_GFXAPI>&) = delete;
    PipelineVertexInputState<T_GFXAPI>& operator=(const PipelineVertexInputState<T_GFXAPI>&) = delete;
public:
    PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind /*indices of buffers (in shaderDescription.m_vertexFormats) to bind in this input state*/) noexcept;
    PipelineVertexInputState(PipelineVertexInputState<T_GFXAPI>&&) noexcept;

    static_assert(sizeof(PipelineVertexInputState<T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};
