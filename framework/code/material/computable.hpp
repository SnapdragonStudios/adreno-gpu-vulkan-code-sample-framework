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
#include "vulkan/vulkan.h"
#include "material/material.hpp"

// Forward Declarations
class Shader;
class Vulkan;


/// Encapsulates a 'computable' pass, contains the materialpass, pipeline, etc).
/// Similar to a DrawablePass but without the vertex buffer
/// @ingroup Material
class ComputablePass
{
    ComputablePass(const ComputablePass&) = delete;
    ComputablePass& operator=(const ComputablePass&) = delete;
public:
    ComputablePass(const MaterialPass& materialPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, std::vector<VkImageMemoryBarrier> imageMemoryBarriers, std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers, bool needsExecutionBarrier)
        : mMaterialPass(materialPass)
        , mPipeline(pipeline)
        , mPipelineLayout(pipelineLayout)
        , mImageMemoryBarriers(std::move(imageMemoryBarriers))
        , mBufferMemoryBarriers(std::move(bufferMemoryBarriers))
        , mNeedsExecutionBarrier(needsExecutionBarrier)
    {}
    ComputablePass(ComputablePass&&) noexcept;
    ~ComputablePass();

    const std::vector<VkDescriptorSet>& GetVkDescriptorSets() const { return mMaterialPass.GetVkDescriptorSets(); }
    const auto& GetVkImageMemoryBarriers() const { return mImageMemoryBarriers; }
    const auto& GetVkBufferMemoryBarriers() const { return mBufferMemoryBarriers; }
    const bool NeedsBarrier() const { return mNeedsExecutionBarrier || (!mImageMemoryBarriers.empty()) || (!mBufferMemoryBarriers.empty()); };   ///< @return true if there needs to be a barrier before executing this compute pass

    /// number of workgroup dispatches to execute (value after the local workgroup sizes are accounted for)
    void SetDispatchGroupCount(std::array<uint32_t, 3> count) { mDispatchGroupCount = count; }
    const auto& GetDispatchGroupCount() const { return mDispatchGroupCount; }
    /// number of global workgroup threads to run (value before the local workgroup sizes are accounted for).  Requires "WorkGroup" : { "LocalSize": {x,y,z} } in the shader definition json.
    void SetDispatchThreadCount(std::array<uint32_t, 3> count);

    const MaterialPass&             mMaterialPass;

    VkPipeline                      mPipeline = VK_NULL_HANDLE; // Owned by us
    VkPipelineLayout                mPipelineLayout;            // Owned by ShaderPass or MaterialPass

protected:
    std::vector<VkImageMemoryBarrier> mImageMemoryBarriers;     ///< Image barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::vector<VkBufferMemoryBarrier> mBufferMemoryBarriers;   ///< Buffer barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::array<uint32_t, 3>         mDispatchGroupCount{ 1u,1u,1u };
    bool                            mNeedsExecutionBarrier = false;///< Denotes if we need an execution barrier for ENTRY to this pass (even if there are no image or buffer barriers).
};


/// Encapsulates a 'computable' object, contains the Material, computable passes.
/// Similar to a Drawable but without the vertex buffer
class Computable
{
    Computable(const Computable&) = delete;
    Computable& operator=(const Computable&) = delete;
public:
    Computable(Vulkan& vulkan, Material&&);
    ~Computable();
    Computable(Computable&&) noexcept;

    bool Init();
    const auto& GetPasses() const { return mPasses; }
    const auto& GetImageInputMemoryBarriers() const { return mImageInputMemoryBarriers; }
    const auto& GetImageOutputMemoryBarriers() const { return mImageOutputMemoryBarriers; }
    const auto& GetBufferOutputMemoryBarriers() const { return mBufferOutputMemoryBarriers; }

    /// number of workgroup dispatches to execute (value after the local workgroup sizes are accounted for)
    void SetDispatchGroupCount(uint32_t passIdx, const std::array<uint32_t, 3>& groupCount);
    /// number of global workgroup threads to run (value before the local workgroup sizes are accounted for).  Requires "WorkGroup" : { "LocalSize": {x,y,z} } in the shader definition json.
    void SetDispatchThreadCount(uint32_t passIdx, const std::array<uint32_t, 3>& threadCount);

    /// Add all the commands to the cmdBuffer needed to 'dispatch' the given pass.
    /// Will add necissary barriers before the dispatch, along with binding the appropriate descriptor sets needed by the pass.
    void DispatchPass(VkCommandBuffer cmdBuffer, const ComputablePass& computablePass, uint32_t bufferIdx) const;

protected:
    Material                            mMaterial;
    Vulkan&                             mVulkan;
    std::vector<ComputablePass>         mPasses;
    std::vector<VkImageMemoryBarrier>   mImageInputMemoryBarriers;      // barriers for ENTRY to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::vector<VkImageMemoryBarrier>   mImageOutputMemoryBarriers;     // barriers for EXIT to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::vector<VkBufferMemoryBarrier>  mBufferOutputMemoryBarriers;    // barriers for EXIT to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
};
