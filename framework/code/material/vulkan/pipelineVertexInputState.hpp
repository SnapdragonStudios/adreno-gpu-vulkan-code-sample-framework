//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <volk/volk.h>
#include "../pipelineVertexInputState.hpp"

// Forward declarations
class ShaderPassDescription;
class ShaderDescription;
class Vulkan;


/// Helper for making VkPipelineVertexInputStateCreateInfo.
/// @ingroup Material
template<>
class PipelineVertexInputState<Vulkan> : public PipelineVertexInputStateBase
{
    PipelineVertexInputState(const PipelineVertexInputState<Vulkan>&) = delete;
    PipelineVertexInputState<Vulkan>& operator=(const PipelineVertexInputState<Vulkan>&) = delete;
public:
    PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind /*indices of buffers (in shaderDescription.m_vertexFormats) to bind in this input state*/ ) noexcept;
    PipelineVertexInputState(PipelineVertexInputState<Vulkan>&&) noexcept;
    const auto& GetVkPipelineVertexInputStateCreateInfo() const { return mVisci; }
protected:
    VkPipelineVertexInputStateCreateInfo mVisci;
    std::vector<VkVertexInputBindingDescription> mBindings;
    std::vector<VkVertexInputAttributeDescription> mAttributes;
};


template<>
class PipelineRasterizationState<Vulkan> : public PipelineRasterizationStateBase
{
    PipelineRasterizationState( const PipelineRasterizationState<Vulkan>& ) = delete;
    PipelineRasterizationState<Vulkan>& operator=( const PipelineRasterizationState<Vulkan>& ) = delete;
public:
    PipelineRasterizationState( const ShaderPassDescription& shaderPassDescription ) noexcept;

    VkPipelineRasterizationStateCreateInfo mPipelineRasterizationStateCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };
};
