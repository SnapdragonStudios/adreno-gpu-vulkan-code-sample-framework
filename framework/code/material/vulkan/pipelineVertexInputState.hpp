//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "material/pipelineVertexInputState.hpp"

// Forward declarations
class ShaderDescription;
class Vulkan;


/// Helper for making VkPipelineVertexInputStateCreateInfo.
/// @ingroup Material
template<>
class PipelineVertexInputState<Vulkan>
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

