//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "pipelineVertexInputState.hpp"
#include "material/shaderDescription.hpp"
#include "vertexDescription.hpp"


PipelineVertexInputState<Vulkan>::PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind) noexcept
    : mVisci{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO }
{
    mBindings.reserve(buffersToBind.size());
    uint32_t location = 0;
    for (uint32_t b = 0; b < buffersToBind.size(); ++b)
    {
        const auto& vertexFormat = shaderDescription.m_vertexFormats[buffersToBind[b]];
        auto& binding = mBindings.emplace_back();
        binding.binding = b;
        binding.stride = vertexFormat.span;
        binding.inputRate = vertexFormat.inputRate == VertexFormat::eInputRate::Vertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;

        size_t attributeLocationOffset = mAttributes.size();
        VertexDescription v(vertexFormat, binding.binding);
        mAttributes.insert(mAttributes.end(), v.GetVertexInputAttributeDescriptions().begin(), v.GetVertexInputAttributeDescriptions().end());
        // The location is unique across all attributes (not local to the binding); make this so.
        for (size_t a = attributeLocationOffset; a < mAttributes.size(); ++a)
            mAttributes[a].location += (uint32_t)attributeLocationOffset;
    }
    mVisci.vertexBindingDescriptionCount = (uint32_t)mBindings.size();
    mVisci.pVertexBindingDescriptions = mBindings.data();
    mVisci.vertexAttributeDescriptionCount = (uint32_t)mAttributes.size();
    mVisci.pVertexAttributeDescriptions = mAttributes.data();
}

PipelineVertexInputState<Vulkan>::PipelineVertexInputState(PipelineVertexInputState<Vulkan>&& other) noexcept
    : mVisci(other.mVisci)
    , mBindings(std::move(other.mBindings))
    , mAttributes(std::move(other.mAttributes))
{
    mVisci = other.mVisci;
    other.mVisci = {};
}


PipelineRasterizationState<Vulkan>::PipelineRasterizationState( const ShaderPassDescription& shaderPassDescription ) noexcept
    : mPipelineRasterizationStateCreateInfo()
{
    const auto& fixedFunctionSettings = shaderPassDescription.m_fixedFunctionSettings;

    //// State for rasterization, such as polygon fill mode is defined.
    mPipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    mPipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    mPipelineRasterizationStateCreateInfo.cullMode = (fixedFunctionSettings.cullBackFace ? VK_CULL_MODE_BACK_BIT : 0) | (fixedFunctionSettings.cullFrontFace ? VK_CULL_MODE_FRONT_BIT : 0);
    mPipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    mPipelineRasterizationStateCreateInfo.depthClampEnable = fixedFunctionSettings.depthClampEnable ? VK_TRUE : VK_FALSE;
    mPipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    mPipelineRasterizationStateCreateInfo.depthBiasEnable = fixedFunctionSettings.depthBiasEnable ? VK_TRUE : VK_FALSE;
    if (fixedFunctionSettings.depthBiasEnable)
    {
        mPipelineRasterizationStateCreateInfo.depthBiasConstantFactor = fixedFunctionSettings.depthBiasConstant;
        mPipelineRasterizationStateCreateInfo.depthBiasClamp = fixedFunctionSettings.depthBiasClamp;
        mPipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = fixedFunctionSettings.depthBiasSlope;
    }
    mPipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
}
