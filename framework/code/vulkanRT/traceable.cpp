//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "traceable.hpp"
#include "vulkanRT.hpp"
#include "material/vulkan/material.hpp"
#include "material/shader.hpp"
#include "material/vulkan/shaderModule.hpp"
#include "material/shaderDescription.hpp"
#include "texture/vulkan/texture.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/commandBuffer.hpp"
//#include "vulkan/TextureFuncts.h"
#include "system/os_common.h"
#include <cassert>
#include <utility>


TraceablePass::TraceablePass(TraceablePass&& other) noexcept
    : mMaterialPass(std::move(other.mMaterialPass))
    , mShaderBindingTable(std::move(other.mShaderBindingTable))
    //, mImageMemoryBarriers(std::move(other.mImageMemoryBarriers))
    //, mBufferMemoryBarriers(std::move(other.mBufferMemoryBarriers))
    //, mNeedsExecutionBarrier(other.mNeedsExecutionBarrier)
    , mRayThreadCount(other.mRayThreadCount)
{
    mPipeline = other.mPipeline;
    other.mPipeline = VK_NULL_HANDLE;
    mPipelineLayout = other.mPipelineLayout;
    other.mPipelineLayout = VK_NULL_HANDLE;
    mShaderBindingTableAddresses = other.mShaderBindingTableAddresses;
    other.mShaderBindingTableAddresses = {};
}

TraceablePass::~TraceablePass()
{
    assert(mPipeline == VK_NULL_HANDLE);
}

Traceable::Traceable(VulkanRT& vulkanRt, Material<Vulkan>&& material)
    : mVulkanRt(vulkanRt)
    , mMaterial(std::move(material))
{
}

Traceable::~Traceable()
{
    for (auto& pass : mPasses)
    {
        mVulkanRt.DestroyRTPipeline(pass.mPipeline);
        pass.mPipeline = VK_NULL_HANDLE;
        ReleaseUniformBuffer(&mVulkanRt.GetVulkan(), &pass.mShaderBindingTable);
    }
}


enum class BindingAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite
};
/// @brief Structure holding the 'use' information for one binding
/// @tparam VK_TYPE Vulkan buffer type for this list of bindings (eg VkImage, VkImage)
template<typename VK_BUFFERTYPE>
struct BindingUseData
{
    typedef VK_BUFFERTYPE buffer_type;
    BindingUseData(uint32_t _passIdx, BindingAccess _access, VK_BUFFERTYPE _buffer) : passIdx(_passIdx), access(_access), buffer(_buffer) {}
    uint32_t        passIdx;
    BindingAccess   access;
    VK_BUFFERTYPE   buffer;
};


bool Traceable::Init()
{
    LOGI("Creating Traceable");

    const auto& materialPasses = mMaterial.GetMaterialPasses();
    mPasses.reserve(materialPasses.size() );

    //
    // Previous pass buffer usage data (for all passes before the current one).
    //
    std::vector<BindingUseData<VkImage>>            prevImageUsages;
    std::vector<BindingUseData<VkBufferAndOffset>>  prevBufferUsages;
    prevImageUsages.reserve(64);
    prevBufferUsages.reserve(64);

    // Lambda helper!  Calls emitFn when the buffer passed to 'currentUsage' was last written to by a previous pass (ie it is in prevPassUsages and was not read from in a pass between current use and the previous write).
    // Returns true if we need an execution barrier emitting.
    // We want to buffer/image barrier on Write then Read patterns, and executiuon barrier on Read then Write patterns.
    const auto& emitBarrier = [](const auto& currentUsage, const auto& prevPassUsages, const auto& emitFn) -> bool {

        // scan backwards looking for prior uses of this buffer/image...
        int priorUseIdx = (int)prevPassUsages.size();
        while (priorUseIdx-- > 0)
        {
            const auto& priorUsage = prevPassUsages[priorUseIdx];
            if (currentUsage.buffer == priorUsage.buffer)
            {
                // if we found a prior use determine what kind of barrier we need to insert.
                const auto& priorUsage = prevPassUsages[priorUseIdx];
                if (priorUsage.access == BindingAccess::ReadOnly)
                {
                    if (currentUsage.access == BindingAccess::ReadOnly)
                    {
                        // Read followed by Read.
                        // do nothing... .  Doesnt even need an execution barrier.
                        // HOWEVER DONT EARLY OUT BECAUSE WE MAY ALSO BIND AS A WRITE (albeit on a different frame - we need to sort that out, happens if we read the 'previous frame' as a texture and write the 'current frame' as an image in the same shader pass)
                    }
                    else
                    {
                        // Read followed by Write or ReadWrite.
                        // just need an execution barrier in this pass
                        return true;
                    }
                }
                else // priorUsage.access != BindingAccess::ReadOnly
                {
                    // Write followed by something (read, write, readwrite).
                    // emit barrier.
                    emitFn(currentUsage.buffer);
                    return true;
                }
            }
        }
        return false;
    };


    for (uint32_t materialPassIdx = 0; materialPassIdx < (uint32_t) materialPasses.size(); ++materialPassIdx)
    {
        const auto& materialPass = materialPasses[materialPassIdx];
        const auto& shaderPass = materialPass.GetShaderPass();
        assert(std::holds_alternative<RayTracingShaderModules<Vulkan>>(shaderPass.m_shaders.m_modules));

        // Usually pipeline layout will be stored with the shader but if the descriptor set layout is 'dynamic' (and stored in the materialPass) the pipeline layout will also be in the materialPass.
        VkPipelineLayout pipelineLayout = shaderPass.GetPipelineLayout().GetVkPipelineLayout();
        if (pipelineLayout == VK_NULL_HANDLE)
            pipelineLayout = materialPass.GetPipelineLayout().GetVkPipelineLayout();

        VkPipeline pipeline = VK_NULL_HANDLE;
        Uniform<Vulkan> shaderBindingTable;
        std::array<VkStridedDeviceAddressRegionKHR, 4> shaderBindingTableAddresses{};
        const auto& shaderModules = shaderPass.m_shaders.Get<RayTracingShaderModules<Vulkan>>();
        LOGI("CreateTracablePipeline: %s\n", shaderPass.m_shaderPassDescription.m_rayGenerationName.c_str());
        
        VkShaderModule rayClosestHit = shaderModules.rayClosestHit ? shaderModules.rayClosestHit->GetVkShaderModule() : VK_NULL_HANDLE;
        VkShaderModule rayAnyHit = shaderModules.rayAnyHit ? shaderModules.rayAnyHit->GetVkShaderModule() : VK_NULL_HANDLE;
        VkShaderModule rayMiss = shaderModules.rayMiss ? shaderModules.rayMiss->GetVkShaderModule() : VK_NULL_HANDLE;
        auto* pSpecializationConstants = materialPass.GetSpecializationConstants().GetVkSpecializationInfo();

        if (!mVulkanRt.CreateRTPipeline(VK_NULL_HANDLE, pipelineLayout, shaderModules.rayGeneration.GetVkShaderModule(), rayClosestHit, rayAnyHit, rayMiss, pSpecializationConstants, shaderPass.m_shaderPassDescription.m_rayTracingSettings.maxRayRecursionDepth, pipeline, shaderBindingTable, shaderBindingTableAddresses))
        {
            // Error
            return false;
        }

        //
        // Build any memory barriers between passes
        //
        bool passNeedsExecutionBarrier = false; // Set if the pass needs (at least) an execution barrier.  Adds barriers for WAR (write after read) use cases.
        std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
        std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;

        // Barriers for Image bindings
        for (const auto& passImageBindings : materialPass.GetImageBindings())
        {
            for (const auto& passImageBinding : passImageBindings.first) // image binding can be an array of bindings
            {
                const auto passUsage = BindingUseData(materialPassIdx, passImageBindings.second.setBinding.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite, passImageBinding.image);

                passNeedsExecutionBarrier |= emitBarrier(passUsage, prevImageUsages, [&imageMemoryBarriers](VkImage image) {
                    imageMemoryBarriers.push_back(VkImageMemoryBarrier{
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                        VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                        VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                        VK_IMAGE_LAYOUT_GENERAL,    //newLayout;
                        VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                        VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                        image,                      //image;
                        { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                            0, //baseMipLevel;
                            1, //mipLevelCount;
                            0, //baseLayer;
                            1, //layerCount;
                        }//subresourceRange;
                        });
                    });
            }
        }

        // Barriers for texture bindings
        for (const auto& passTextureBindings : materialPass.GetTextureBindings())
        {
            for (const auto& texture : passTextureBindings.first) // texture binding can be an array of bindings
            {
                const auto& textureVulkan = apiCast<Vulkan>(texture);
                const auto passUsage = BindingUseData(materialPassIdx, BindingAccess::ReadOnly/*always readonly*/, textureVulkan->GetVkImage());

                passNeedsExecutionBarrier |= emitBarrier(passUsage, prevImageUsages,
                    [&imageMemoryBarriers](VkImage image) {
                        imageMemoryBarriers.push_back(VkImageMemoryBarrier{
                            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                            nullptr,
                            VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                            VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                            VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                            VK_IMAGE_LAYOUT_GENERAL,    //newLayout;
                            VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                            VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                            image,                      //image;
                            { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                                0, //baseMipLevel;
                                1, //mipLevelCount;
                                0, //baseLayer;
                                1, //layerCount;
                            }//subresourceRange;
                        });
                    });
            }
        }

        // Barriers for buffer bindings
        for (const auto& passBufferBindings : materialPass.GetBufferBindings())
        {
            for (const auto& passBufferBinding : passBufferBindings.first) // buffer binding can be an array of bindings
            {
                const auto passUsage = BindingUseData(materialPassIdx, passBufferBindings.second.setBinding.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite, passBufferBinding);

                passNeedsExecutionBarrier |= emitBarrier(passUsage, prevBufferUsages,
                    [&](const VkBufferAndOffset& buffer) {
                        //LOGI("Pass %d: Buffer Barrier: %s", materialPassIdx, materialPass.mShaderPass.m_shaderPassDescription.m_sets[0].m_descriptorTypes[tmp].names[0].c_str());
                        bufferMemoryBarriers.push_back(VkBufferMemoryBarrier{
                            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                            nullptr,
                            VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                            VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                            VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                            VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                            buffer.buffer(),            //buffer;
                            buffer.offset(),            //offset
                            VK_WHOLE_SIZE               //size
                        });
                    });
            }
        }

        mPasses.push_back( TraceablePass{ materialPass, pipeline, pipelineLayout, std::move(shaderBindingTable), std::move(shaderBindingTableAddresses) /*, std::move(imageMemoryBarriers), std::move(bufferMemoryBarriers), passNeedsExecutionBarrier*/ } );

        //
        // Store out the arrays of buffers used by this pass
        //

/*        for (const auto& imageBindings : materialPass.GetImageBindings())
        {
            BindingAccess access = imageBindings.second.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite;
            for (const auto& imageBinding : imageBindings.first)
            {
                prevImageUsages.push_back({ materialPassIdx, access, imageBinding.image });
            }
        }
        for (const auto& textureBinding : materialPass.GetTextureBindings())
        {
            for (const auto& texInfo : textureBinding.first)
            {
                prevImageUsages.push_back({ materialPassIdx, BindingAccess::ReadOnly, texInfo->GetVkImage() });
            }
        }
        for (const auto& bufferBinding : materialPass.GetBufferBindings())
        {
            LOGI("Buffer Binding: %s", materialPass.mShaderPass.m_shaderPassDescription.m_sets[0].m_descriptorTypes[bufferBinding.second.index].names[0].c_str());

            for (const VkBuffer& buffer : bufferBinding.first)
            {
                prevBufferUsages.push_back({ materialPassIdx, bufferBinding.second.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite, buffer });
            }
        }
*/
    }

    // Create the final set of barriers.
    // Any bindings that (may) write and were not used as a (read only) input to a subsequent ComputePass will be assumed as being outputs from the Traceable (and will get memory barriers built)

    // Helper to scan through passUsages and call emitFn on any bindings that have a write as their last access type (ie things that are being output from the Traceable and haven't already been barriered).
    // Mangles/clears passUsages.
/*    const auto& emitOutputBarrier = [](auto&& passUsages, const auto& emitFn) -> void
    {
        while( !passUsages.empty() )
        {
            typename std::remove_reference_t<decltype(passUsages)>::value_type::buffer_type currentBuffer = VK_NULL_HANDLE;
            for (auto revIt = passUsages.rbegin(); revIt != passUsages.rend(); )
            {
                if (currentBuffer == VK_NULL_HANDLE)
                {
                    if (revIt->access == BindingAccess::ReadOnly)
                    {
                        // last use of this binding was a readonly - no need for any output barriers
                        currentBuffer = revIt->buffer;
                    }
                    else
                    {
                        // last use of this binding is write or read-write
                        currentBuffer = revIt->buffer;
                        // pushback
                        emitFn(revIt->buffer);
                    }
                }
                if (currentBuffer == revIt->buffer)
                {
                    // Once we have found a candidate buffer remove all prior references to it (and itself).
                    revIt = std::reverse_iterator(passUsages.erase((++revIt).base()));
                }
                else
                {
                    ++revIt;
                }
            }
        }
    };

    emitOutputBarrier(prevBufferUsages,
        [this](VkBuffer buffer) -> void {
            mBufferOutputMemoryBarriers.push_back(VkBufferMemoryBarrier{
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                buffer,                     //buffer;
                0,                          //offset
                VK_WHOLE_SIZE               //size
                });
        });

    emitOutputBarrier(prevImageUsages,
        [this](VkImage image) -> void {
            mImageOutputMemoryBarriers.push_back(VkImageMemoryBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,//newLayout;
                VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                image,                      //image;
                { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                  0, //baseMipLevel;
                  1, //mipLevelCount;
                  0, //baseLayer;
                  1, //layerCount;
                }//subresourceRange;
                });
        });
*/
    for (uint32_t whichBuffer = 0; whichBuffer < mMaterial.GetNumFrameBuffers(); ++whichBuffer)
    {
        mMaterial.UpdateDescriptorSets(whichBuffer);
    }
    return true;
}

const std::string& Traceable::GetPassName( uint32_t passIdx ) const
{
    const auto& passNames = mMaterial.m_shader.GetShaderPassIndicesToNames();
    assert( passIdx <= passNames.size() );
    return passNames[passIdx];
}

void Traceable::SetRayThreadCount(uint32_t passIdx, const std::array<uint32_t, 3>& threadCount)
{
    mPasses[passIdx].SetRayThreadCount(threadCount);
}

void Traceable::Dispatch(CommandListVulkan* cmdBuffers, uint32_t numCmdBuffers, uint32_t startDescriptorSetIdx) const
{
    assert(cmdBuffers != nullptr);

    for (uint32_t whichBuffer = 0; whichBuffer < numCmdBuffers; ++whichBuffer)
    {
        for (const auto& traceablePass : GetPasses())
        {
            DispatchPass(*cmdBuffers, traceablePass, (whichBuffer + startDescriptorSetIdx) % (uint32_t)traceablePass.GetVkDescriptorSets().size());
        }
        ++cmdBuffers;
    }
}

void Traceable::DispatchPass(VkCommandBuffer cmdBuffer, const TraceablePass& traceablePass, uint32_t bufferIdx) const
{
    // Add image barriers (if needed)
/*    if (traceablePass.NeedsBarrier())
    {
        const auto& imageMemoryBarriers = traceablePass.GetVkImageMemoryBarriers();
        const auto& bufferMemoryBarriers = traceablePass.GetVkBufferMemoryBarriers();

        // Barrier on memory, with correct layouts set.
        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // srcMask,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // dstMask,
            0,
            0, nullptr,
            (uint32_t)bufferMemoryBarriers.size(),
            bufferMemoryBarriers.empty() ? nullptr : bufferMemoryBarriers.data(),
            (uint32_t)imageMemoryBarriers.size(),
            imageMemoryBarriers.empty() ? nullptr : imageMemoryBarriers.data());
    }
*/
    // Bind the pipeline for this material
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, traceablePass.mPipeline);

    // Bind everything the shaders need
    const auto& descriptorSets = traceablePass.GetVkDescriptorSets();
    VkDescriptorSet descriptorSet = descriptorSets.size() >= 1 ? descriptorSets[bufferIdx] : descriptorSets[0];
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, traceablePass.mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Dispatch the compute task
    mVulkanRt.vkCmdTraceRaysKHR(cmdBuffer, &traceablePass.mShaderBindingTableAddresses[0], &traceablePass.mShaderBindingTableAddresses[1], &traceablePass.mShaderBindingTableAddresses[2], &traceablePass.mShaderBindingTableAddresses[3], traceablePass.GetRayThreadCount()[0], traceablePass.GetRayThreadCount()[1], traceablePass.GetRayThreadCount()[2]);
}
