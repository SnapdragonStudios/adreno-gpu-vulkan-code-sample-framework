//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations
class ShaderDescription;


/// Helper for making VkPipelineVertexInputStateCreateInfo.
/// @ingroup Material
class PipelineVertexInputState
{
    PipelineVertexInputState(const PipelineVertexInputState&) = delete;
    PipelineVertexInputState& operator=(const PipelineVertexInputState&) = delete;
public:
    PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind /*indices of buffers (in shaderDescription.m_vertexFormats) to bind in this input state*/ );
    PipelineVertexInputState(PipelineVertexInputState&&) = default;
    const auto& GetVkPipelineVertexInputStateCreateInfo() const { return mVisci; }
protected:
    VkPipelineVertexInputStateCreateInfo mVisci;
    std::vector<VkVertexInputBindingDescription> mBindings;
    std::vector<VkVertexInputAttributeDescription> mAttributes;
};

