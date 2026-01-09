//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "vulkan/vulkan.hpp"
#include "vulkan/renderContext.hpp"
#include "pipeline.hpp"
#include "pipelineLayout.hpp"
#include "pipelineVertexInputState.hpp"
#include "shader.hpp"
#include "shaderModule.hpp"
#include "specializationConstants.hpp"
#include "../shaderDescription.hpp"

// Forward declarations
class Vulkan;


Pipeline<Vulkan>::Pipeline() noexcept
    : mPipeline{}
{
}

Pipeline<Vulkan>::Pipeline(VkDevice device, VkPipeline pipeline) noexcept
    : mPipeline( device, pipeline )
{
}

Pipeline<Vulkan>::~Pipeline()
{}

static VkBlendFactor EnumToVk( ShaderPassDescription::BlendFactor bf)
{
    switch( bf )
    {
    case ShaderPassDescription::BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case ShaderPassDescription::BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case ShaderPassDescription::BlendFactor::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case ShaderPassDescription::BlendFactor::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case ShaderPassDescription::BlendFactor::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case ShaderPassDescription::BlendFactor::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    }
    assert( 0 );
    return VK_BLEND_FACTOR_ZERO;
}

template<>
Pipeline<Vulkan> CreatePipeline( Vulkan& vulkan,
                                 const ShaderPassDescription& shaderPassDescription,
                                 const PipelineLayout<Vulkan>& pipelineLayout,
                                 const PipelineVertexInputState<Vulkan>& pipelineVertexInputState,
                                 const PipelineRasterizationState<Vulkan>& pipelineRasterizationState,
                                 const SpecializationConstants<Vulkan>& specializationConstants,
                                 const ShaderModules<Vulkan>& shaderModules,
                                 const RenderContext<Vulkan>& renderingPassContext,
                                 Msaa msaa)
{
    //// State for rasterization, such as polygon fill mode is defined.
    const auto& fixedFunctionSettings = shaderPassDescription.m_fixedFunctionSettings;

    const auto& outputSettings = shaderPassDescription.m_outputs;

    // Setup blending/transparency
    std::vector<VkPipelineColorBlendAttachmentState> BlendStates;
    BlendStates.reserve(outputSettings.size());
    for (const auto& outputSetting : outputSettings)
    {
        VkPipelineColorBlendAttachmentState& cb = BlendStates.emplace_back(VkPipelineColorBlendAttachmentState{});
        if (outputSetting.blendEnable)
        {
            cb.blendEnable = VK_TRUE;
            cb.srcColorBlendFactor = EnumToVk(outputSetting.srcColorBlendFactor);
            cb.dstColorBlendFactor = EnumToVk(outputSetting.dstColorBlendFactor);
            cb.colorBlendOp = VK_BLEND_OP_ADD;
            cb.srcAlphaBlendFactor = EnumToVk(outputSetting.srcAlphaBlendFactor);
            cb.dstAlphaBlendFactor = EnumToVk(outputSetting.dstAlphaBlendFactor);
            cb.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        cb.colorWriteMask = outputSetting.colorWriteMask & (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    }

    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = (uint32_t)BlendStates.size();
    cb.pAttachments = BlendStates.data();

    // Setup depth testing
    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = fixedFunctionSettings.depthTestEnable ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = fixedFunctionSettings.depthWriteEnable ? VK_TRUE : VK_FALSE;
    switch( fixedFunctionSettings.depthCompareOp ) {
    case ShaderPassDescription::DepthCompareOp::LessEqual:
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
    case ShaderPassDescription::DepthCompareOp::Equal:
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
    case ShaderPassDescription::DepthCompareOp::Greater:
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
    }
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;

    // Setup (multi) sampling
    const auto& sampleShadingSettings = shaderPassDescription.m_sampleShadingSettings;
    VkSampleMask sampleMask = 0;
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = EnumToVk(msaa);
    ms.sampleShadingEnable = sampleShadingSettings.sampleShadingEnable;
    if (sampleShadingSettings.sampleShadingMask != 0)
    {
        assert(ms.rasterizationSamples <= VK_SAMPLE_COUNT_32_BIT ); // sampleMask is only 32bits currently! Easy fix if we want > 32x MSAA
        sampleMask = sampleShadingSettings.sampleShadingMask & ((1 << ms.rasterizationSamples) -1);
        ms.pSampleMask = &sampleMask;
    }

    VkPipelineSampleLocationsStateCreateInfoEXT msLocations = { VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT };
    if( sampleShadingSettings.forceCenterSample )
    {
        msLocations.sampleLocationsEnable = VK_TRUE;
        msLocations.sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
        msLocations.sampleLocationsInfo.sampleLocationsPerPixel = ms.rasterizationSamples;
        msLocations.sampleLocationsInfo.sampleLocationsCount = (uint32_t) ms.rasterizationSamples;
        msLocations.sampleLocationsInfo.sampleLocationGridSize = { 1,1 };
        std::vector<VkSampleLocationEXT> msSampleLocations( msLocations.sampleLocationsInfo.sampleLocationsCount, { 0.5f,0.5f } );
        msLocations.sampleLocationsInfo.pSampleLocations = msSampleLocations.data();
        ms.pNext = &msLocations;
    }

    VkShaderModule vkVertShader = VK_NULL_HANDLE;
    VkShaderModule vkFragShader = VK_NULL_HANDLE;
    VkShaderModule vkTaskShader = VK_NULL_HANDLE;
    VkShaderModule vkMeshShader = VK_NULL_HANDLE;
    std::visit( [&]( auto& m )
                {
                    using T = std::decay_t<decltype(m)>;
                    if constexpr (std::is_same_v<T, GraphicsShaderModules<Vulkan>>)
                    {
                        vkVertShader = m.vert.GetVkShaderModule();
                        vkFragShader = m.frag.GetVkShaderModule();
                    }
                    else if constexpr (std::is_same_v<T, GraphicsShaderModuleVertOnly<Vulkan>>)
                    {
                        vkVertShader = m.vert.GetVkShaderModule();
                    }
                    else if constexpr (std::is_same_v<T, GraphicsMeshShaderModules<Vulkan>>)
                    {
                        vkMeshShader = m.mesh.GetVkShaderModule();
                        vkFragShader = m.frag.GetVkShaderModule();
                    }
                    else if constexpr (std::is_same_v<T, GraphicsTaskMeshShaderModules<Vulkan>>)
                    {
                        vkTaskShader = m.task.GetVkShaderModule();
                        vkMeshShader = m.mesh.GetVkShaderModule();
                        vkFragShader = m.frag.GetVkShaderModule();
                    }                    
                    else
                    {
                        assert( 0 );    // unsupported shader module type (eg ComputeShaderModule)
                    }
                }, shaderModules.m_modules );

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (!vulkan.CreatePipeline( vulkan.GetPipelineCache(),
                                &pipelineVertexInputState.GetVkPipelineVertexInputStateCreateInfo(),
                                pipelineLayout.GetVkPipelineLayout(),
                                renderingPassContext,
                                &pipelineRasterizationState.mPipelineRasterizationStateCreateInfo,
                                &ds,
                                BlendStates.empty() ? nullptr : &cb,
                                &ms,
                                nullptr,//input assembly state
                                {},//dynamic states
                                nullptr/*viewport*/, nullptr/*scissor*/,
                                vkTaskShader,
                                vkMeshShader,
                                vkVertShader,
                                vkFragShader,
                                specializationConstants.GetVkSpecializationInfo(),
                                false,
                                VK_NULL_HANDLE,
                                &pipeline))
    {
        // Error
        return {};
    }
    return Pipeline<Vulkan>(vulkan.m_VulkanDevice, pipeline);
}

void ReleasePipeline(Vulkan& vulkan, Pipeline<Vulkan>& pipeline)
{
    pipeline = {};
}