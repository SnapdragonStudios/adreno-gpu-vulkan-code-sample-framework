//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <memory>
#include <array>
#include <vector>
#include <string>
#include <volk/volk.h>
#include "memory/vulkan/bufferObject.hpp"
#include "memory/vulkan/uniform.hpp"
#include "material/vulkan/material.hpp"

// Forward Declarations
class VulkanRT;
template<class T_GFXAPI> class CommandList;
using CommandListVulkan = CommandList<Vulkan>;


/// Encapsulates a 'traceable' pass, contains the materialpass, pipeline, etc).
/// Similar to a DrawablePass but for ray tracing
/// @ingroup Material
class TraceablePass
{
    TraceablePass(const TraceablePass&) = delete;
    TraceablePass& operator=(const TraceablePass&) = delete;
public:
    TraceablePass(const MaterialPass<Vulkan>& materialPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, Uniform<Vulkan> shaderBindingTable, std::array<VkStridedDeviceAddressRegionKHR, 4> shaderBindingTableAddresses
        //, std::vector<VkImageMemoryBarrier> imageMemoryBarriers, std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers, bool needsExecutionBarrier
        )
        : mMaterialPass(materialPass)
        , mPipeline(pipeline)
        , mPipelineLayout(pipelineLayout)
        , mShaderBindingTable(std::move(shaderBindingTable))
        , mShaderBindingTableAddresses(std::move(shaderBindingTableAddresses))
        //, mImageMemoryBarriers(std::move(imageMemoryBarriers))
        //, mBufferMemoryBarriers(std::move(bufferMemoryBarriers))
        //, mNeedsExecutionBarrier(needsExecutionBarrier)
    {}
    TraceablePass(TraceablePass&&) noexcept;
    ~TraceablePass();

    const std::vector<VkDescriptorSet>& GetVkDescriptorSets() const { return mMaterialPass.GetVkDescriptorSets(); }
    //const auto& GetVkImageMemoryBarriers() const { return mImageMemoryBarriers; }
    //const auto& GetVkBufferMemoryBarriers() const { return mBufferMemoryBarriers; }
    //const bool NeedsBarrier() const { return mNeedsExecutionBarrier || (!mImageMemoryBarriers.empty()) || (!mBufferMemoryBarriers.empty()); };   ///< @return true if there needs to be a barrier before executing this compute pass

    /// number of ray generation threads to run.
    void SetRayThreadCount(std::array<uint32_t, 3> count) { mRayThreadCount = count; }
    const auto& GetRayThreadCount() const { return mRayThreadCount; }

    const MaterialPass<Vulkan>&    mMaterialPass;

    VkPipeline                      mPipeline = VK_NULL_HANDLE; // Owned by us
    VkPipelineLayout                mPipelineLayout;            // Owned by ShaderPass or MaterialPassBase

    UniformVulkan                   mShaderBindingTable;

    std::array< VkStridedDeviceAddressRegionKHR, 4> mShaderBindingTableAddresses{};

protected:
    //std::vector<VkImageMemoryBarrier> mImageMemoryBarriers;     ///< Image barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    //std::vector<VkBufferMemoryBarrier> mBufferMemoryBarriers;   ///< Buffer barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::array<uint32_t, 3>         mRayThreadCount{ 1u,1u,1u };
    //bool                            mNeedsExecutionBarrier = false;///< Denotes if we need an execution barrier for ENTRY to this pass (even if there are no image or buffer barriers).
};


/// Encapsulates a 'traceable' object, contains the MaterialBase, traceable passes.
/// Similar to a Drawable but for ray tracing
class Traceable
{
    Traceable(const Traceable&) = delete;
    Traceable& operator=(const Traceable&) = delete;
public:
    Traceable(VulkanRT& vulkanRt, Material<Vulkan>&&);
    ~Traceable();

    bool Init();
    const auto& GetPasses() const { return mPasses; }

    const std::string& GetPassName( uint32_t passIdx ) const;

    //const auto& GetImageInputMemoryBarriers() const { return mImageInputMemoryBarriers; }
    //const auto& GetImageOutputMemoryBarriers() const { return mImageOutputMemoryBarriers; }
    //const auto& GetBufferOutputMemoryBarriers() const { return mBufferOutputMemoryBarriers; }

    /// number of ray generation threads to run
    void SetRayThreadCount(uint32_t passIdx, const std::array<uint32_t, 3>& count);

    /// @brief Add the commands to dispatch the 'traceable' to a command buffer.
    /// Potentially adds multiple vkCmdTraceRaysKHR (will make one per 'pass') and inserts appropiate memory barriers between passes.
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    /// @param startDescriptorSetIdx index of the first descriptor set to add
    void Dispatch(CommandListVulkan* cmdBuffers, uint32_t numCmdBuffers, uint32_t startDescriptorSetIdx) const;
    void Dispatch(CommandListVulkan& cmdBuffer)
    {
        Dispatch(&cmdBuffer, 1, 0);
    }

    /// Add all the commands to the cmdBuffer needed to 'dispatch' the given pass.
    /// Will add necissary barriers before the dispatch, along with binding the appropriate descriptor sets needed by the pass.
    void DispatchPass(VkCommandBuffer cmdBuffer, const TraceablePass& TraceablePass, uint32_t bufferIdx) const;

protected:
    Material<Vulkan>                   mMaterial;
    VulkanRT&                           mVulkanRt;
    std::vector<TraceablePass>          mPasses;
    //std::vector<VkImageMemoryBarrier>   mImageInputMemoryBarriers;      // barriers for ENTRY to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    //std::vector<VkImageMemoryBarrier>   mImageOutputMemoryBarriers;     // barriers for EXIT to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    //std::vector<VkBufferMemoryBarrier>  mBufferOutputMemoryBarriers;    // barriers for EXIT to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
};
