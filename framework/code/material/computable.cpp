//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "computable.hpp"
#include "shader.hpp"
#include "shaderModule.hpp"
#include "shaderDescription.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include "system/os_common.h"
#include <cassert>
#include <utility>


ComputablePass::ComputablePass(ComputablePass&& other) noexcept
    : mMaterialPass(std::move(other.mMaterialPass))
    , mImageMemoryBarriers(std::move(other.mImageMemoryBarriers))
    , mBufferMemoryBarriers(std::move(other.mBufferMemoryBarriers))
    , mNeedsExecutionBarrier(other.mNeedsExecutionBarrier)
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

Computable::Computable(Computable&& other) noexcept
    : mVulkan(other.mVulkan)
    , mMaterial(std::move(other.mMaterial))
    , mPasses(std::move(other.mPasses))
    , mImageInputMemoryBarriers(std::move(other.mImageInputMemoryBarriers))
    , mImageOutputMemoryBarriers(std::move(other.mImageOutputMemoryBarriers))
    , mBufferOutputMemoryBarriers(std::move(other.mBufferOutputMemoryBarriers))
{
}

void ComputablePass::SetDispatchThreadCount(const std::array<uint32_t, 3> threadCount)
{
    std::array<uint32_t, 3> groupCount;
    const auto& cWorkGroupLocalSize = mMaterialPass.mShaderPass.m_shaderPassDescription.m_workGroupSettings.localSize;
    if (cWorkGroupLocalSize[0] == 0 || cWorkGroupLocalSize[1] == 0 || cWorkGroupLocalSize[2] == 0)
    {
        // Workgroup local size must be defined in the shader definition if we want to use this function
        assert(0);
        return;
    }
    groupCount[0] = (threadCount[0] + cWorkGroupLocalSize[0] - 1) / cWorkGroupLocalSize[0];
    groupCount[1] = (threadCount[1] + cWorkGroupLocalSize[1] - 1) / cWorkGroupLocalSize[1];
    groupCount[2] = (threadCount[2] + cWorkGroupLocalSize[2] - 1) / cWorkGroupLocalSize[2];

    SetDispatchGroupCount(groupCount);
}


Computable::~Computable()
{
    for (auto& pass : mPasses)
    {
        vkDestroyPipeline(mVulkan.m_VulkanDevice, pass.mPipeline, nullptr);
        pass.mPipeline = VK_NULL_HANDLE;
    }
}


enum class BindingAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite
};
/// @brief Structure holding the 'use' information for one binding
/// @tparam VK_TYPE Vulkan buffer type for this list of bindings (eg VkImage, VkBuffer)
template<typename VK_BUFFERTYPE>
struct BindingUseData
{
    typedef VK_BUFFERTYPE buffer_type;
    BindingUseData(uint32_t _passIdx, BindingAccess _access, VK_BUFFERTYPE _buffer) : passIdx(_passIdx), access(_access), buffer(_buffer) {}
    uint32_t        passIdx;
    BindingAccess   access;
    VK_BUFFERTYPE   buffer;
};


bool Computable::Init()
{
    LOGI("Creating Computable");

    const auto& materialPasses = mMaterial.GetMaterialPasses();
    mPasses.reserve(materialPasses.size() );

    //
    // Previour pass buffer usage data (for all passes before the current one).
    //
    struct ImageUsage {
        VkImage image;
        uint32_t numMips;
        uint32_t firstMip;
        ImageUsage( VkImage _image, uint32_t _numMips, uint32_t _firstMip ) : image( _image ), numMips( _numMips ), firstMip( _firstMip ) {};
        ImageUsage( const ImageInfo& other ) : image( other.image ), numMips( other.imageViewNumMips ), firstMip( other.imageViewFirstMip ) {}
        bool operator==( const ImageUsage& other ) const {
            return image == other.image && ((firstMip < other.firstMip + other.numMips) && (firstMip + numMips > other.firstMip));
        }
    };
    std::vector<BindingUseData<ImageUsage>>  prevImageUsages;
    std::vector<BindingUseData<VkBuffer>>    prevBufferUsages;
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
        const auto& shaderPass = materialPass.mShaderPass;
        assert(std::holds_alternative<ComputeShaderModule>(shaderPass.m_shaders.m_modules));

        // Usually pipeline layout will be stored with the shader but if the descriptor set layout is 'dynamic' (and stored in the materialPass) the pipeline layout will also be in the materialPass.
        VkPipelineLayout pipelineLayout = shaderPass.GetPipelineLayout().GetVkPipelineLayout();
        if (pipelineLayout == VK_NULL_HANDLE)
            pipelineLayout = materialPass.GetPipelineLayout().GetVkPipelineLayout();

        VkPipeline pipeline;
        const ShaderModule& shaderModule = shaderPass.m_shaders.Get<ComputeShaderModule>();
        LOGI("CreateComputePipeline: %s\n", shaderPass.m_shaderPassDescription.m_computeName.c_str());

        if (!mVulkan.CreateComputePipeline(VK_NULL_HANDLE,
            pipelineLayout,
            shaderModule.GetVkShaderModule(),
            &pipeline))
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
                const auto passUsage = BindingUseData( materialPassIdx, passImageBindings.second.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite, ImageUsage(passImageBinding) );

                passNeedsExecutionBarrier |= emitBarrier(passUsage, prevImageUsages, [&imageMemoryBarriers](const ImageUsage& imageUsage) {
                    imageMemoryBarriers.push_back(VkImageMemoryBarrier{
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                        VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                        VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                        VK_IMAGE_LAYOUT_GENERAL,    //newLayout;
                        VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                        VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                        imageUsage.image,           //image;
                        { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                            imageUsage.firstMip,    //baseMipLevel;
                            imageUsage.numMips,     //mipLevelCount;
                            0,                      //baseLayer;
                            1,                      //layerCount;
                        }//subresourceRange;
                        });
                    });
            }
        }

        // Barriers for texture bindings
        for (const auto& passTextureBindings : materialPass.GetTextureBindings())
        {
            for (const auto& passTextureBinding : passTextureBindings.first) // texture binding can be an array of bindings
            {
                const auto passUsage = BindingUseData(materialPassIdx, BindingAccess::ReadOnly/*always readonly*/, ImageUsage(*passTextureBinding));

                passNeedsExecutionBarrier |= emitBarrier(passUsage, prevImageUsages,
                    [&imageMemoryBarriers](const ImageUsage& imageUsage) {
                        imageMemoryBarriers.push_back(VkImageMemoryBarrier{
                            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                            nullptr,
                            VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                            VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                            VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,    //newLayout;
                            VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                            VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                            imageUsage.image,           //image;
                            { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                                imageUsage.firstMip,    //baseMipLevel;
                                imageUsage.numMips,     //mipLevelCount;
                                0,                      //baseLayer;
                                1,                      //layerCount;
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
                const auto passUsage = BindingUseData(materialPassIdx, passBufferBindings.second.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite, passBufferBinding);

                passNeedsExecutionBarrier |= emitBarrier(passUsage, prevBufferUsages,
                    [&](VkBuffer buffer) {
                        //LOGI("Pass %d: Buffer Barrier: %s", materialPassIdx, materialPass.mShaderPass.m_shaderPassDescription.m_sets[0].m_descriptorTypes[tmp].names[0].c_str());
                        bufferMemoryBarriers.push_back(VkBufferMemoryBarrier{
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
            }
        }

        mPasses.push_back( ComputablePass{ materialPass, pipeline, pipelineLayout, std::move(imageMemoryBarriers), std::move(bufferMemoryBarriers), passNeedsExecutionBarrier } );

        //
        // Store out the arrays of buffers used by this pass
        //

        for (const auto& imageBindings : materialPass.GetImageBindings())
        {
            BindingAccess access = imageBindings.second.isReadOnly ? BindingAccess::ReadOnly : BindingAccess::ReadWrite;
            for (const auto& imageBinding : imageBindings.first)
            {
                prevImageUsages.push_back( { materialPassIdx, access, ImageUsage{imageBinding.image, imageBinding.imageViewNumMips, imageBinding.imageViewFirstMip} } );
            }
        }
        for (const auto& textureBinding : materialPass.GetTextureBindings())
        {
            for (const auto& texInfo : textureBinding.first)
            {
                prevImageUsages.push_back( { materialPassIdx, BindingAccess::ReadOnly, {texInfo->GetVkImage(), texInfo->MipLevels, texInfo->FirstMip} } );
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
    }

    // Create the final set of barriers.
    // Any bindings that (may) write and were not used as a (read only) input to a subsequent ComputePass will be assumed as being outputs from the Computable (and will get memory barriers built)

    // Helper to scan through passUsages and call emitFn on any bindings that have a write as their last access type (ie things that are being output from the Computable and haven't already been barriered).
    // Mangles/clears passUsages.
    const auto& emitOutputBarrier = [](auto&& passUsages, const auto& emitFn) -> void
    {
        while( !passUsages.empty() )
        {
            std::optional<typename std::remove_reference_t<decltype(passUsages)>::value_type::buffer_type> currentBuffer {};
            for (auto revIt = passUsages.rbegin(); revIt != passUsages.rend(); )
            {
                if (!currentBuffer)
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
        [this](const ImageUsage& image) -> void {
            mImageOutputMemoryBarriers.push_back(VkImageMemoryBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT, //srcAccessMask
                VK_ACCESS_SHADER_READ_BIT,  //dstAccessMask
                VK_IMAGE_LAYOUT_GENERAL,    //oldLayout;
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,//newLayout;
                VK_QUEUE_FAMILY_IGNORED,    //srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,    //dstQueueFamilyIndex;
                image.image,                //image;
                { VK_IMAGE_ASPECT_COLOR_BIT,//aspect;
                  image.firstMip,           //baseMipLevel;
                  image.numMips,            //mipLevelCount;
                  0,                        //baseLayer;
                  1,                        //layerCount;
                }//subresourceRange;
                });
        });

    for (uint32_t whichBuffer = 0; whichBuffer < mMaterial.GetNumFrameBuffers(); ++whichBuffer)
    {
        mMaterial.UpdateDescriptorSets(whichBuffer);
    }
    return true;
}

void Computable::SetDispatchGroupCount(uint32_t passIdx, const std::array<uint32_t, 3>& groupCount)
{
    mPasses[passIdx].SetDispatchGroupCount(groupCount);
}

void Computable::SetDispatchThreadCount(uint32_t passIdx, const std::array<uint32_t, 3>& threadCount)
{
    mPasses[passIdx].SetDispatchThreadCount(threadCount);
}

void Computable::DispatchPass(VkCommandBuffer cmdBuffer, const ComputablePass& computablePass, uint32_t bufferIdx) const
{
    // Add image barriers (if needed)
    if (computablePass.NeedsBarrier())
    {
        const auto& imageMemoryBarriers = computablePass.GetVkImageMemoryBarriers();
        const auto& bufferMemoryBarriers = computablePass.GetVkBufferMemoryBarriers();

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

    // Bind the pipeline for this material
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computablePass.mPipeline);

    // Bind everything the shader needs
    const auto& descriptorSets = computablePass.GetVkDescriptorSets();
    VkDescriptorSet descriptorSet = descriptorSets.size() >= 1 ? descriptorSets[bufferIdx] : descriptorSets[0];
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computablePass.mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Dispatch the compute task
    vkCmdDispatch(cmdBuffer, computablePass.GetDispatchGroupCount()[0], computablePass.GetDispatchGroupCount()[1], computablePass.GetDispatchGroupCount()[2]);
}
