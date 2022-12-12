//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "bloom-image-processing.hpp"

// framework
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "camera/cameraData.hpp"
#include "camera/cameraGltfLoader.hpp"
#include "helper/postProcessSMAA.hpp"
#include "helper/postProcessStandard.hpp"
#include "main/applicationEntrypoint.hpp"
#include "memory/memoryManager.hpp"
#include "memory/bufferObject.hpp"
#include "memory/indexBufferObject.hpp"
#include "memory/vertexBufferObject.hpp"
#include "system/math_common.hpp"
#include "system/config.h"

// vulkan
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_support.hpp"
#include "vulkan/TextureFuncts.h"

VAR(bool, gUseExtension, true, kVariableNonpersistent);
VAR(unsigned, gBlurFilterSize, 7, kVariableNonpersistent);

#if OS_ANDROID
#define SHADERFILE(f) "./Shaders/" f
#define TEXTUREFILE(f) "./Textures/" f
#else
#define SHADERFILE(f) "./Media/Shaders/" f
#define TEXTUREFILE(f) "./Media/Textures/" f
#endif // OS_ANDROID

const char* BloomImageprocessing::ShaderSets[ShaderPair_Count][3] =
{
    {SHADERFILE("VertexShader.vert.spv"), SHADERFILE("Downsample.frag.spv"), SHADERFILE("Downsample-Ext.frag.spv")},// ShaderPair_Downsample,
    {SHADERFILE("VertexShader.vert.spv"), SHADERFILE("BlurBase-Horizontal.frag.spv"), SHADERFILE("BlurBase-Horizontal-Ext.frag.spv")},// ShaderPair_Blur_Horizontal,
    {SHADERFILE("VertexShader.vert.spv"), SHADERFILE("BlurBase-Vertical.frag.spv"), SHADERFILE("BlurBase-Vertical-Ext.frag.spv")},// ShaderPair_Blur_Vertical,
    {SHADERFILE("VertexShader.vert.spv"), SHADERFILE("Display.frag.spv"), SHADERFILE("Display.frag.spv")},// ShaderPair_Display,
};

///
/// @brief Implementation of the Application entrypoint (called by the framework)
/// @return Pointer to Application (derived from @FrameworkApplicationBase).
/// Creates the Application class.  Ownership is passed to the calling (framework) function.
/// 
FrameworkApplicationBase* Application_ConstructApplication()
{
    return new BloomImageprocessing();
}

BloomImageprocessing::BloomImageprocessing() : ApplicationHelperBase()
{
    m_bUseExtension = gUseExtension;
    gBlurFilterSize = mini(gBlurFilterSize, 25);

    memset(m_weightTextureViews, 0, sizeof(m_weightTextureViews));
}

BloomImageprocessing::~BloomImageprocessing()
{
}

void  BloomImageprocessing::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& config)
{
    ApplicationHelperBase::PreInitializeSetVulkanConfiguration(config);
    config.RequiredExtension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
    // config.RequiredExtension("VK_KHR_format_feature_flags2");
    if (m_bUseExtension)
    {
        config.RequiredExtension<Ext_VK_QCOM_image_processing>();
    }
    else
    {
        config.OptionalExtension<Ext_VK_QCOM_image_processing>();
    }
}

//-----------------------------------------------------------------------------
bool BloomImageprocessing::Initialize(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    if (!FrameworkApplicationBase::Initialize(windowHandle))
    {
        return false;
    }

    Vulkan* pVulkan = m_vulkan.get();

    memset(m_passes, 0, sizeof(m_passes));

    //--------------------------------------------------------------------------
    // Setup Render Targets
    {
        const VkFormat bloomFormat[] = { k_RtFormat };

        for (uint32_t ii = 0; ii < Pass_Count; ++ii)
        {

            uint32_t w = GlobalPassInfo[ii].renderWidth;
            uint32_t h = GlobalPassInfo[ii].renderHeight;

            if (w == 0)
            {
                w = pVulkan->m_SurfaceWidth;
            }

            if (h == 0)
            {
                h = pVulkan->m_SurfaceHeight;
            }

            char rtNames[256];
            snprintf(rtNames, 256, "Bloom RT %d %dx%d", ii, w, h);

            if (!m_IntermediateRts[ii].Initialize(pVulkan, w, h, bloomFormat, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT, rtNames))
            {
                LOGE("Unable to create main render target");
            }
        }
    }
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    // Setup Shaders
    for (uint32_t ss = 0; ss < ShaderPair_Count; ++ss)
    {
        LoadShader(pVulkan, *m_AssetManager, &m_shaders[ss], ShaderSets[ss][0], m_bUseExtension ? ShaderSets[ss][2] : ShaderSets[ss][1]);
    }
    //--------------------------------------------------------------------------
    
    //--------------------------------------------------------------------------
    // Setup Weight Array
    float* pRawTexData = new float[gBlurFilterSize];
    float16* pHalfTexData = new float16[gBlurFilterSize];
    {
        BuildWeightArray(gBlurFilterSize, pRawTexData);
        for (uint32_t ii = 0; ii < gBlurFilterSize; ++ii)
        {
            pHalfTexData[ii] = float16(pRawTexData[ii]);
        }
    }
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    // Setup Textures
    if (m_bUseExtension)
    {
        for (uint32_t ii = 0; ii < NumWeightImages; ++ii)
        {
            VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM;
            const VkFormat weightFormat = VK_FORMAT_R16_SFLOAT;

            VkImageViewSampleWeightCreateInfoQCOM weightViewInfo = {};
            weightViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_SAMPLE_WEIGHT_CREATE_INFO_QCOM;
            weightViewInfo.pNext = NULL;
            weightViewInfo.numPhases = 1;

            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.pNext = &weightViewInfo;
            viewInfo.flags = 0;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (ii == 1) // H
            {
                m_weightTextures[ii] = LoadTextureFromBuffer(pVulkan, pHalfTexData, gBlurFilterSize * sizeof(float16), gBlurFilterSize, 1, 1, weightFormat, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, usageFlags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                weightViewInfo.filterCenter.x = gBlurFilterSize / 2;
                weightViewInfo.filterCenter.y = 0;
                weightViewInfo.filterSize.width = gBlurFilterSize;
                weightViewInfo.filterSize.height = 1;
            }
            else // V
            {
                m_weightTextures[ii] = LoadTextureFromBuffer(pVulkan, pHalfTexData, gBlurFilterSize * sizeof(float16), 1, gBlurFilterSize, 1, weightFormat, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, usageFlags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                weightViewInfo.filterCenter.x = 0;
                weightViewInfo.filterCenter.y = gBlurFilterSize / 2;
                weightViewInfo.filterSize.width = 1;
                weightViewInfo.filterSize.height = gBlurFilterSize;
            }

            viewInfo.image = m_weightTextures[ii].GetVkImage();
            viewInfo.format = m_weightTextures[ii].Format;

            if (!CheckVkError("vkCreateImageView()", vkCreateImageView(pVulkan->m_VulkanDevice, &viewInfo, NULL, &m_weightTextureViews[ii])))
            {
                return false;
            }
        }
    }

    if (pVulkan->IsTextureFormatSupported(VK_FORMAT_ASTC_4x4_UNORM_BLOCK))
    {
        m_sourceTexture = LoadKTXTexture(pVulkan, *m_AssetManager, TEXTUREFILE("Bloom-Source-Texture-astc.ktx"), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    }
    else
    {
        m_sourceTexture = LoadKTXTexture(pVulkan, *m_AssetManager, TEXTUREFILE("Bloom-Source-Texture.ktx"), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    }
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    // Setup UBOs
    for (uint32_t ii = 0; ii < Pass_Count; ++ii)
    {
        const ConstPassInfo* pPassInfo = &GlobalPassInfo[ii];

        size_t dataSize = 0;
        float* pUboData = NULL;

        switch (pPassInfo->shaderPair)
        {
        case ShaderPair_Downsample:
            break;
        case ShaderPair_Blur_Horizontal:
        case ShaderPair_Blur_Vertical:
        {
            dataSize = 32 * sizeof(float);
            pUboData = (float*)calloc(32, sizeof(float));

            pUboData[0] = (float)gBlurFilterSize;

            for (uint32_t index = 0; index < gBlurFilterSize; index++)
            {
                pUboData[index + 1] = pRawTexData[index];
            }
            break;
        }
        case ShaderPair_Display:
            break;
        }

        if (pUboData != NULL)
        {
            CreateUniformBuffer(pVulkan, &m_uniforms[ii], dataSize, pUboData);
            free(pUboData);
        }
    }
    //--------------------------------------------------------------------------

    delete[] pRawTexData;
    delete[] pHalfTexData;

    //--------------------------------------------------------------------------
    // Setup Passes
    {
        for (uint32_t pp = 0; pp < Pass_Count; ++pp)
        {
            m_passes[pp] = new PassInfo(pVulkan, &GlobalPassInfo[pp]);

            m_passes[pp]->pRt = &m_IntermediateRts[pp];
            m_passes[pp]->pUniform = NULL;
            m_passes[pp]->renderArea.width = m_passes[pp]->pPassInfo->renderWidth;
            m_passes[pp]->renderArea.height = m_passes[pp]->pPassInfo->renderHeight;
            m_passes[pp]->weightViewIndex = -1;
            m_passes[pp]->pShaderInfo = &m_shaders[m_passes[pp]->pPassInfo->shaderPairId];

            switch (m_passes[pp]->pPassInfo->shaderPair)
            {
            case ShaderPair_Downsample:
                m_passes[pp]->vInputViews.push_back(m_sourceTexture.GetVkImageView());
                break;
            case ShaderPair_Blur_Horizontal:
            {
                m_passes[pp]->pUniform = &m_uniforms[pp];
                m_passes[pp]->vInputViews.push_back(m_IntermediateRts[pp - 1][0].m_ColorAttachments[0].GetVkImageView());

                if (m_bUseExtension)
                {
                    m_passes[pp]->vInputViews.push_back(m_weightTextureViews[0]);
                    m_passes[pp]->weightViewIndex = m_passes[pp]->vInputViews.size() - 1;
                }
                break;
            }
            case ShaderPair_Blur_Vertical:
                m_passes[pp]->pUniform = &m_uniforms[pp];
                m_passes[pp]->vInputViews.push_back(m_IntermediateRts[pp - 1][0].m_ColorAttachments[0].GetVkImageView());
                if (m_bUseExtension)
                {
                    m_passes[pp]->vInputViews.push_back(m_weightTextureViews[1]);
                    m_passes[pp]->weightViewIndex = m_passes[pp]->vInputViews.size() - 1;
                }
                break;
            case ShaderPair_Display:
                m_passes[pp]->pRt = NULL; // &m_IntermediateRts[Pass_Display];
                m_passes[pp]->renderArea.width = pVulkan->m_SurfaceWidth; // k_FullImageWidth;
                m_passes[pp]->renderArea.height = pVulkan->m_SurfaceHeight; // k_FullImageHeight;
                m_passes[pp]->vInputViews.push_back(m_sourceTexture.GetVkImageView());
                m_passes[pp]->vInputViews.push_back((*m_passes[pp -1]->pRt)[0].m_ColorAttachments[0].GetVkImageView());
                break;
            }
        }

    }
    //--------------------------------------------------------------------------

    for (uint32_t pp = 0; pp < Pass_Count; ++pp)
    {
        FinalizePass(m_passes[pp]);
    }

    //--------------------------------------------------------------------------
    // Setup Command Buffers
    for (uint32_t ii = 0; ii < pVulkan->m_SwapchainImageCount; ++ii)
    {
        BuildCmdBuffer(ii);
    }
    //--------------------------------------------------------------------------

    return true;
}

//-----------------------------------------------------------------------------
void BloomImageprocessing::Destroy()
//-----------------------------------------------------------------------------
{
    vkDeviceWaitIdle(m_vulkan.get()->m_VulkanDevice);

    // Textures
    for (uint32_t ii = 0; ii < NumWeightImages; ++ii)
    {
        ReleaseTexture(m_vulkan.get(), &m_weightTextures[ii]);
        vkDestroyImageView(m_vulkan.get()->m_VulkanDevice, m_weightTextureViews[ii], NULL);
    }
    ReleaseTexture(m_vulkan.get(), &m_sourceTexture);

    // Shaders
    for (uint32_t ss = 0; ss < ShaderPair_Count; ++ss)
    {
        ReleaseShader(m_vulkan.get(), &m_shaders[ss]);
    }

    // Uniform Buffers
    for (uint32_t ii = 0; ii < Pass_Count; ++ii)
    {
        ReleaseUniformBuffer(m_vulkan.get(), &m_uniforms[ii]);
    }

    for (uint32_t pp = 0; pp < Pass_Count; ++pp)
    {
        delete m_passes[pp];
    }

    ApplicationHelperBase::Destroy();
}

void BloomImageprocessing::Render(float fltDiffTime)
{
    // Grab the vulkan wrapper
    Vulkan* pVulkan = m_vulkan.get();

    // Obtain the next swap chain image for the next frame.
    auto currentBuffer = pVulkan->SetNextBackBuffer();

    m_commandBuffers[currentBuffer.idx].QueueSubmit(currentBuffer, pVulkan->m_RenderCompleteSemaphore);

    // Queue is loaded up, tell the driver to start processing
    pVulkan->PresentQueue(pVulkan->m_RenderCompleteSemaphore, currentBuffer.swapchainPresentIdx);

}

void BloomImageprocessing::FinalizePass(PassInfo* pPass)
{
    Vulkan* pVulkan = m_vulkan.get();

    if (pPass->pRt == NULL)
    {
        pPass->renderpass = pVulkan->m_SwapchainRenderPass;
    }
    else
    {
        pPass->renderpass = pPass->pRt->m_RenderPass;
        pPass->fbo = pPass->pRt->m_RenderTargets[0].m_FrameBuffer;
    }

    // Descriptor Set Layouyt
    VkDescriptorSetLayoutCreateInfo dsLayoutInfo = {};
    dsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsLayoutInfo.pNext = NULL;
    dsLayoutInfo.flags = 0x0;

    std::vector<VkDescriptorSetLayoutBinding> vBindings;
    vBindings.reserve(10);
    if (pPass->pUniform != NULL)
    {
        vBindings.push_back(VkDescriptorSetLayoutBinding());
        vBindings.back().binding = 0;
        vBindings.back().descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vBindings.back().descriptorCount = 1;
        vBindings.back().stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        vBindings.back().pImmutableSamplers = NULL;
    }

    for (uint32_t ii = 0; ii < pPass->vInputViews.size(); ++ii)
    {
        bool isWeightImage = (pPass->weightViewIndex >= 0) && (ii >= pPass->weightViewIndex);

        vBindings.push_back(VkDescriptorSetLayoutBinding());
        vBindings.back().binding = vBindings.size() - 1;
        vBindings.back().descriptorType = isWeightImage ? VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        vBindings.back().descriptorCount = 1;
        vBindings.back().stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        vBindings.back().pImmutableSamplers = NULL;

        vBindings.push_back(VkDescriptorSetLayoutBinding());
        vBindings.back().binding = vBindings.size() - 1;
        vBindings.back().descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        vBindings.back().descriptorCount = 1;
        vBindings.back().stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        vBindings.back().pImmutableSamplers = NULL;
    }

    dsLayoutInfo.bindingCount = vBindings.size();
    dsLayoutInfo.pBindings = vBindings.data();
    if (!CheckVkError("vkCreateDescriptorSetLayout()", vkCreateDescriptorSetLayout(pVulkan->m_VulkanDevice, &dsLayoutInfo, NULL, &pPass->dsLayout)))
    {
        return;
    }

    // Pipeline Layout
    VkPipelineLayoutCreateInfo PipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    PipelineLayoutInfo.pNext = NULL;
    PipelineLayoutInfo.setLayoutCount = 1;
    PipelineLayoutInfo.pSetLayouts = &pPass->dsLayout;

    if (!CheckVkError("vkCreatePipelineLayout()", vkCreatePipelineLayout(pVulkan->m_VulkanDevice, &PipelineLayoutInfo, NULL, &pPass->pLayout)))
    {
        return;
    }

    // Descriptor Pool
    std::vector< VkDescriptorPoolSize> vPoolSizes;
    vPoolSizes.reserve(4);
    for (auto dsIt = vBindings.begin(); dsIt != vBindings.end(); dsIt++)
    {
        vPoolSizes.push_back(VkDescriptorPoolSize());
        vPoolSizes.back().type = (*dsIt).descriptorType;
        vPoolSizes.back().descriptorCount = (*dsIt).descriptorCount;
    }
    VkDescriptorPoolCreateInfo PoolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    PoolInfo.pNext = NULL;
    PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;     // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to allow them to be returned
    PoolInfo.maxSets = 1;  // Since descriptor sets come out of this pool we need more than one
    PoolInfo.poolSizeCount = vPoolSizes.size();
    PoolInfo.pPoolSizes = vPoolSizes.data();

    if (!CheckVkError("vkCreateDescriptorPool()", vkCreateDescriptorPool(pVulkan->m_VulkanDevice, &PoolInfo, NULL, &pPass->dsPool)))
    {
        return;
    }

    // Descriptor Set
    VkDescriptorSetAllocateInfo DescSetInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    DescSetInfo.pNext = NULL;
    DescSetInfo.descriptorPool = pPass->dsPool;
    DescSetInfo.descriptorSetCount = 1;
    DescSetInfo.pSetLayouts = &pPass->dsLayout;

    if (!CheckVkError("vkAllocateDescriptorSets()", vkAllocateDescriptorSets(pVulkan->m_VulkanDevice, &DescSetInfo, &pPass->ds)))
    {
        return;
    }

    VkDescriptorImageInfo vImageInfos[8] = {};
    VkDescriptorBufferInfo vBufferInfos[4] = {};
    VkWriteDescriptorSet vWriteInfos[12] = {};

    if (pPass->pUniform != NULL)
    {
        vBufferInfos[0] = pPass->pUniform->bufferInfo;
    }
    for (uint32_t tt = 0; tt < pPass->vInputViews.size(); ++tt)
    {
        VkImageView view = pPass->vInputViews[tt];

        bool isWeightImage = ((pPass->weightViewIndex >= 0) && (tt >= pPass->weightViewIndex));

        vImageInfos[tt * 2 + 0].imageView = view;
        vImageInfos[tt * 2 + 0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkSamplerReductionModeCreateInfo redInfo = {};
        redInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        redInfo.pNext = NULL;
        redInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

        VkSamplerCreateInfo SamplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        SamplerInfo.flags = isWeightImage ? VK_SAMPLER_CREATE_IMAGE_PROCESSING_BIT_QCOM : 0;
        SamplerInfo.pNext = &redInfo;
        SamplerInfo.magFilter = isWeightImage ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        SamplerInfo.minFilter = isWeightImage ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.mipLodBias = 0.f;
        SamplerInfo.anisotropyEnable = VK_FALSE;
        SamplerInfo.maxAnisotropy = 1.0f;
        SamplerInfo.compareEnable = VK_FALSE;
        SamplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        SamplerInfo.minLod = 0.0f;
        SamplerInfo.maxLod = 0.0f;
        SamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;   // VkBorderColor
        SamplerInfo.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler;

        if (!CheckVkError("vkCreateSampler()", vkCreateSampler(pVulkan->m_VulkanDevice, &SamplerInfo, NULL, &sampler)))
        {
            return;
        }

        pPass->samplers.push_back(sampler);
        vImageInfos[tt * 2 + 1].sampler = sampler;
    }

    uint32_t writeIndex = 0;

    uint32_t baseBinding = 0;
    if (pPass->pUniform != NULL)
    {
        
        vWriteInfos[writeIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vWriteInfos[writeIndex].pNext = NULL;
        vWriteInfos[writeIndex].dstSet = pPass->ds;
        vWriteInfos[writeIndex].dstBinding = 0;
        vWriteInfos[writeIndex].dstArrayElement = 0;
        vWriteInfos[writeIndex].descriptorCount = 1;
        vWriteInfos[writeIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vWriteInfos[writeIndex].pBufferInfo = &vBufferInfos[0];

        writeIndex++;
        baseBinding++;
    }

    for (uint32_t tt = 0; tt < pPass->vInputViews.size(); ++tt)
    {
        VkImageView view = pPass->vInputViews[tt];
        bool isWeightImage = (pPass->weightViewIndex >= 0) && (tt >= pPass->weightViewIndex);

        vWriteInfos[writeIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vWriteInfos[writeIndex].pNext = NULL;
        vWriteInfos[writeIndex].dstSet = pPass->ds;
        vWriteInfos[writeIndex].dstBinding = baseBinding + tt * 2 + 0;
        vWriteInfos[writeIndex].dstArrayElement = 0;
        vWriteInfos[writeIndex].descriptorCount = 1;
        vWriteInfos[writeIndex].descriptorType = isWeightImage ? VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        vWriteInfos[writeIndex].pImageInfo = &vImageInfos[tt * 2 + 0];

        writeIndex++;

        vWriteInfos[writeIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vWriteInfos[writeIndex].pNext = NULL;
        vWriteInfos[writeIndex].dstSet = pPass->ds;
        vWriteInfos[writeIndex].dstBinding = baseBinding + tt * 2 + 1;
        vWriteInfos[writeIndex].dstArrayElement = 0;
        vWriteInfos[writeIndex].descriptorCount = 1;
        vWriteInfos[writeIndex].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        vWriteInfos[writeIndex].pImageInfo = &vImageInfos[tt * 2 + 1];

        writeIndex++;
    }

    vkUpdateDescriptorSets(pVulkan->m_VulkanDevice, writeIndex, vWriteInfos, 0, NULL);

    // Pipeline
    VkPipelineVertexInputStateCreateInfo visci = {};
    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Common to all pipelines
    // State for rasterization, such as polygon fill mode is defined.
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    // For the cube, we don't write or check depth
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;


    rs.cullMode = VK_CULL_MODE_NONE;
    rs.depthBiasEnable = VK_FALSE;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    m_vulkan->CreatePipeline(VK_NULL_HANDLE,
        &visci,
        pPass->pLayout,
        pPass->renderpass,
        0/*subpass*/,
        &rs,
        &ds,
        nullptr,
        nullptr,
        {},
        nullptr,
        nullptr,
        pPass->pShaderInfo->VertShaderModule.GetVkShaderModule(),
        pPass->pShaderInfo->FragShaderModule.GetVkShaderModule(),
        false,
        VK_NULL_HANDLE,
        & pPass->pipeline);

}

void BloomImageprocessing::DrawPass(Wrap_VkCommandBuffer* cmd, const PassInfo* pPass, uint32_t idx)
{
    Vulkan* pVulkan = m_vulkan.get();

    // When starting the render pass, we can set clear values.
    VkClearColorValue clear_color = {};
    clear_color.float32[0] = 0.0f;
    clear_color.float32[1] = 0.0f;
    clear_color.float32[2] = 0.0f;
    clear_color.float32[3] = 1.0f;

    VkViewport Viewport = {};
    Viewport.width = (float)pPass->renderArea.width;
    Viewport.height = (float)pPass->renderArea.height;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;
    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = pPass->renderArea.width;
    Scissor.extent.height = pPass->renderArea.height;

    cmd->BeginRenderPass(Scissor, 0.0f, 1.0f, { &clear_color,1 }, 1, pPass->pRt == NULL ? true : false, pPass->renderpass, pPass->pRt == NULL ? true : false, pPass->pRt == NULL ? pVulkan->m_pSwapchainFrameBuffers[idx] : pPass->fbo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd->m_VkCommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(cmd->m_VkCommandBuffer, 0, 1, &Scissor);

    // Bind the pipeline for this material
    vkCmdBindPipeline(cmd->m_VkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pPass->pipeline);

    // Bind everything the shader needs
    vkCmdBindDescriptorSets(cmd->m_VkCommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pPass->pLayout,
        0,
        1,
        &pPass->ds,
        0,
        NULL);

    vkCmdDraw(cmd->m_VkCommandBuffer, 6, 1, 0, 0);

    cmd->EndRenderPass();
}

void BloomImageprocessing::BuildCmdBuffer(uint32_t idx)
{
    Wrap_VkCommandBuffer* pCmdBuf = &m_commandBuffers[idx];

    Vulkan* pVulkan = m_vulkan.get();

    if (!pCmdBuf->Initialize(pVulkan))
    {
        LOGE("Failed to initialize command buffer %d", idx);
        return;
    }

    pCmdBuf->Reset();
    pCmdBuf->Begin();

    for (uint32_t pp = 0; pp < Pass_Count; ++pp)
    {
        if (pp > 0)
        {
            VkMemoryBarrier memBarrier = {};
            memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memBarrier.pNext = NULL;
            memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

            vkCmdPipelineBarrier(pCmdBuf->m_VkCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0x0, 1, &memBarrier, 0, NULL, 0, NULL);
        }

        DrawPass(pCmdBuf, m_passes[pp], idx);
    }

    pCmdBuf->End();


}

void BloomImageprocessing::BuildWeightArray(uint32_t weightSize, float* pWeightArray)
{
    uint32_t tri = weightSize + 3;
    uint64_t fact_tri = Factorial(tri);

    double* pTriVals = new double[tri+1];
    double triValSum = 0.0;

    for (uint32_t ii = 0; ii < tri + 1; ++ii)
    {
        pTriVals[ii] = (double)fact_tri / ((double)Factorial(ii) * (double)Factorial(tri - ii));
        triValSum += pTriVals[ii];
    }

    for (uint32_t ii = 0; ii < weightSize; ++ii)
    {
        pWeightArray[ii] = (float)(pTriVals[ii + 2] / triValSum);
    }

    delete[] pTriVals;
}
