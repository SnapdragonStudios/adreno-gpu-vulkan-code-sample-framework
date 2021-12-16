// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "computable.hpp"
#include "shader.hpp"
#include "shaderModule.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include "system/os_common.h"
#include <cassert>
#include <utility>


ComputablePass::ComputablePass(ComputablePass&& other) noexcept
    : mMaterialPass(std::move(other.mMaterialPass))
    , mImageMemoryBarriers(std::move(other.mImageMemoryBarriers))
    , mDispatchGroupCount(other.mDispatchGroupCount)
{
    mPipeline = other.mPipeline;
    other.mPipeline = VK_NULL_HANDLE;
    mPipelineLayout = other.mPipelineLayout;
    other.mPipelineLayout = VK_NULL_HANDLE;
}

ComputablePass::~ComputablePass()
{
    assert(mPipeline == VK_NULL_HANDLE);
}

Computable::Computable(Vulkan& vulkan, Material&& material)
    : mVulkan(vulkan)
    , mMaterial(std::move(material))
{
}

Computable::~Computable()
{
    for (auto& pass : mPasses)
    {
        vkDestroyPipeline(mVulkan.m_VulkanDevice, pass.mPipeline, nullptr);
        pass.mPipeline = VK_NULL_HANDLE;
    }
}

bool Computable::Init()
{
    LOGI("Creating Computable");

    const auto& materialPasses = mMaterial.GetMaterialPasses();
    mPasses.reserve(materialPasses.size() );

    // Generate pass barriers.
    constexpr uint32_t cMaxDependancies = 8;
    std::array<VkImage, cMaxDependancies> passOutputs;  // keep a (running) list of outputs from all passes
    uint32_t passOutputCount = 0;                       // Total number of pass outputs
    uint32_t passOutputCountLastPass = 0;               // Number of pass outputs not including the current pass

    for (const auto& materialPass : materialPasses)
    {
        const auto& shaderPass = materialPass.mShaderPass;
        assert(std::holds_alternative<ComputeShaderModule>(shaderPass.m_shaders.m_modules));

        VkPipeline pipeline;
        const ShaderModule& shaderModule = shaderPass.m_shaders.Get<ComputeShaderModule>();
        if (!mVulkan.CreateComputePipeline(VK_NULL_HANDLE,
            shaderPass.GetPipelineLayout().GetVkPipelineLayout(),
            shaderModule.GetVkShaderModule(),
            &pipeline))
        {
            // Error
            return false;
        }

        //
        // Build any memory barriers between passes (shared images)
        //
        std::vector<VkImageMemoryBarrier> memoryBarriers;
        uint32_t passOutputCountLastPass = passOutputCount;
        for (const auto& imageBinding : materialPass.GetImageBindings())
        {
            bool found = false;
            for (uint32_t i = 0; i < passOutputCountLastPass; ++i)
            {
                if (passOutputs[i] == imageBinding.first.image)
                {
                    memoryBarriers.push_back(VkImageMemoryBarrier{
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                        VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                        VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                        VK_IMAGE_LAYOUT_GENERAL,    //newLayout;
                        VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                        VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                        passOutputs[i],             //image;
                        { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                          0, //baseMipLevel;
                          1, //mipLevelCount;
                          0, //baseLayer;
                          1, //layerCount;
                        }//subresourceRange;
                        });
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                // save for subsequent pass
                passOutputs[passOutputCount++] = imageBinding.first.image;
            }
        }
        //
        // Build barriers for images that were bound as (storage) images in a previous pass and now being used as a texture.
        // Assume this means we wrote to them in that previous pass and are now wanting to read from them again (as a texture).
        for (const auto& binding : materialPass.GetTextureBindings())
        {
            for (uint32_t i = 0; i < passOutputCountLastPass; ++i)
            {
                for (uint32_t buf=0; buf < (uint32_t)binding.first.size(); ++buf)
                {
                    if (passOutputs[i] == binding.first[buf]->GetVkImage())
                    {
                        memoryBarriers.push_back(VkImageMemoryBarrier{
                            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                            nullptr,
                            VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                            VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                            VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                            binding.first[buf]->GetVkImageLayout(),//newLayout;
                            VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                            VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                            passOutputs[i],             //image;
                            { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                              0, //baseMipLevel;
                              1, //mipLevelCount;
                              0, //baseLayer;
                              1, //layerCount;
                            }//subresourceRange;
                            });
                        break;
                    }
                }
            }
            // Unlike Images the Textures are considered read-only, so we dont need to remember them for a subsequent pass.
        }

        mPasses.push_back( ComputablePass{ materialPass, pipeline, materialPass.mShaderPass.GetPipelineLayout().GetVkPipelineLayout(), std::move(memoryBarriers) } );
    }

    for (uint32_t whichBuffer = 0; whichBuffer < mVulkan.m_SwapchainImageCount; ++whichBuffer)
    {
        mMaterial.UpdateDescriptorSets(whichBuffer);
    }
    return true;
}

void Computable::SetDispatchGroupCount(uint32_t passIdx, const std::array<uint32_t, 3>& groupCount)
{
    mPasses[passIdx].SetDispatchGroupCount(groupCount);
}
