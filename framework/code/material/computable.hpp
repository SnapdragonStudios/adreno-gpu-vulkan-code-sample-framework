// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
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
    ComputablePass(const MaterialPass& materialPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, std::vector<VkImageMemoryBarrier> imageMemoryBarriers)
        : mMaterialPass(materialPass)
        , mPipeline(pipeline)
        , mPipelineLayout(pipelineLayout)
        , mImageMemoryBarriers(std::move(imageMemoryBarriers))
    {}
    ComputablePass(ComputablePass&&) noexcept;
    ~ComputablePass();

    const std::vector<VkDescriptorSet>& GetVkDescriptorSets() const { return mMaterialPass.GetVkDescriptorSets(); }
    const auto& GetVkImageMemoryBarriers() const { return mImageMemoryBarriers; }

    void SetDispatchGroupCount(std::array<uint32_t, 3> count) { mDispatchGroupCount = count; }
    const auto& GetDispatchGroupCount() const { return mDispatchGroupCount; }

    const MaterialPass&             mMaterialPass;

    VkPipeline                      mPipeline = VK_NULL_HANDLE; // Owned by us
    VkPipelineLayout                mPipelineLayout;    // Owned by shader

protected:
    std::vector<VkImageMemoryBarrier> mImageMemoryBarriers;     // barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::array<uint32_t, 3>         mDispatchGroupCount{ 1u,1u,1u };
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

    bool Init();
    const auto& GetPasses() const { return mPasses; }

    void SetDispatchGroupCount(uint32_t passIdx, const std::array<uint32_t, 3>& groupCount);    ///TODO: can we data-drive this?
protected:
    Material        mMaterial;
    Vulkan&         mVulkan;
    std::vector<ComputablePass>     mPasses;
};
