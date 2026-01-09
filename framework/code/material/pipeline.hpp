//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations
class ShaderPassDescription;
template<typename T_GFXAPI> class SpecializationConstants;
template<typename T_GFXAPI> class PipelineLayout;
template<typename T_GFXAPI> class PipelineRasterizationState;
template<typename T_GFXAPI> class PipelineVertexInputState;
template<typename T_GFXAPI> class ShaderModules;
template<typename T_GFXAPI> class RenderPass;
template<typename T_GFXAPI> class RenderContext;
enum class Msaa;

/// @brief Settings for vertex input assembly
struct PipelineInputAssemblySettings
{
    enum PrimitiveTopology {
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP = 2,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN = 5,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 6,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 7,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 8,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9,
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST = 10,
    } PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool primitiveRestartEnable = false;
};


/// Simple wrapper around VkPipeline or ID3D12PipelineState.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// This template class expected to be specialized (if this template throws compiler errors then the code is not using the specialization classes which is an issue!)
/// @ingroup Material
template<typename T_GFXAPI>
class Pipeline
{
    Pipeline& operator=(const Pipeline<T_GFXAPI>&) = delete;
    Pipeline(const Pipeline<T_GFXAPI>&) = delete;
public:
    Pipeline() noexcept = delete;
    Pipeline(Pipeline<T_GFXAPI>&&) noexcept = delete;
    ~Pipeline() = delete;

    static_assert(sizeof( Pipeline<T_GFXAPI> ) >= 1, "Must use the specialized version of this class.  Your are likely missing #include \"material/<GFXAPI>/pipeline.hpp\"");
};

template<typename T_GFXAPI>
Pipeline<T_GFXAPI> CreatePipeline( T_GFXAPI&,
                                   const ShaderPassDescription& shaderPassDescription,
                                   const PipelineLayout<T_GFXAPI>& pipelineLayout,
                                   const PipelineVertexInputState<T_GFXAPI>& pipelineVertexInputState,
                                   const PipelineRasterizationState<T_GFXAPI>& pipelineRasterizationState,
                                   const SpecializationConstants<T_GFXAPI>& specializationConstants,
                                   const ShaderModules<T_GFXAPI>& shaderModules,
                                   const RenderContext<T_GFXAPI>& renderContext,
                                   Msaa msaa )
{
    static_assert(sizeof( T_GFXAPI ) != sizeof( T_GFXAPI ), "Must use the specialized version of this function.  Your are likely missing #include \"material/<GFXAPI>/pipeline.hpp\"");
    assert( 0 && "Expecting CreatePipeline (per graphics api) to be used" );
    return {};
}