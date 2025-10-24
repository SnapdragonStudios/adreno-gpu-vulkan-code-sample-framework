//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <volk/volk.h>
#include "vulkan/refHandle.hpp"
#include "pipelineVertexInputState.hpp"
#include "../pipeline.hpp"

// Forward declarations
class ShaderPassDescription;
class Vulkan;
enum class Msaa;
template<typename T_GFXAPI> class PipelineLayout;
template<typename T_GFXAPI> class ShaderModules;
template<typename T_GFXAPI> class SpecializationConstants;


/// Simple wrapper around VkPipeline.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// Specialization of Pipeline<T_GFXAPI>
/// @ingroup Material
template<>
class Pipeline<Vulkan> final
{
    Pipeline& operator=(const Pipeline<Vulkan>&) = delete;
public:
    Pipeline() noexcept;
    ~Pipeline();
    Pipeline( VkDevice device, VkPipeline pipeline ) noexcept;
    Pipeline( Pipeline<Vulkan>&& ) noexcept = default;
    Pipeline& operator=( Pipeline<Vulkan>&& ) noexcept = default;

    Pipeline Copy() const { return Pipeline{*this}; }

    operator bool() const { return mPipeline != VK_NULL_HANDLE; }

    VkPipeline GetVkPipeline() const { return mPipeline.get(); }

private:
    Pipeline( const Pipeline<Vulkan>& src ) noexcept {
        mPipeline = src.mPipeline;
    }
    RefHandle<VkPipeline> mPipeline;
};

template<>
Pipeline<Vulkan> CreatePipeline( Vulkan& vulkan,
                                 const ShaderPassDescription& shaderPassDescription,
                                 const PipelineLayout<Vulkan>& pipelineLayout,
                                 const PipelineVertexInputState<Vulkan>& pipelineVertexInputState,
                                 const PipelineRasterizationState<Vulkan>& pipelineRasterizationState,
                                 const SpecializationConstants<Vulkan>& specializationConstants,
                                 const ShaderModules<Vulkan>& shaderModules,
                                 const RenderContext<Vulkan>& renderContext,
                                 Msaa msaa );