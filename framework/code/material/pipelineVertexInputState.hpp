//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations
class ShaderDescription;
class ShaderPassDescription;

class PipelineVertexInputStateBase {
    PipelineVertexInputStateBase( const PipelineVertexInputStateBase& ) = delete;
    PipelineVertexInputStateBase& operator=( const PipelineVertexInputStateBase& ) = delete;
public:
    PipelineVertexInputStateBase() noexcept = default;
};


/// Helper for making VkPipelineVertexInputStateCreateInfo.
/// @ingroup Material
template<typename T_GFXAPI>
class PipelineVertexInputState : public PipelineVertexInputStateBase
{
public:
    PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind /*indices of buffers (in shaderDescription.m_vertexFormats) to bind in this input state*/) noexcept;
    static_assert(sizeof(PipelineVertexInputState<T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};


class PipelineRasterizationStateBase
{
    PipelineRasterizationStateBase( const PipelineRasterizationStateBase& ) = delete;
    PipelineRasterizationStateBase& operator=( const PipelineRasterizationStateBase& ) = delete;
public:
    PipelineRasterizationStateBase() noexcept = default;
};

/// Helper for making VkPipelineRasterizationStateCreateInfo.
/// @ingroup Material
template<typename T_GFXAPI>
class PipelineRasterizationState : public PipelineRasterizationStateBase
{
public:
    PipelineRasterizationState() noexcept
    {}
    PipelineRasterizationState( PipelineRasterizationState<T_GFXAPI>&& ) noexcept
    {}
    PipelineRasterizationState( const ShaderPassDescription& shaderPassDescription ) noexcept {}

    //ok to use the base class (eg for Dx12) when there is no rasterization state
    //static_assert(sizeof( PipelineRasterizationState<T_GFXAPI> ) >= 1);   // Ensure this class template is specialized (and not used as-is)
};
