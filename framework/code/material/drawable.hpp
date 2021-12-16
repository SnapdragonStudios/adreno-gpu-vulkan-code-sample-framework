// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <optional>
#include "vulkan/vulkan.h"
#include "vulkan/MeshObject.h"
#include "material/material.hpp"
#include "memory/vertexBufferObject.hpp"
#include "pipelineVertexInputState.hpp"

// Forward Declarations
class Material;
class Shader;
class Vulkan;


/// Encapsulates a drawable pass.
/// Owns the Vulkan pipeline, descriptor sets required by that pipeline.  References vertex/index buffers etc from the parent Drawable.
/// Users are expected to use Drawable (which contains a vector of DrawablePasses and is more 'user friendly').
/// @ingroup Material
class DrawablePass
{
    DrawablePass(const DrawablePass&) = delete;
    DrawablePass& operator=(const DrawablePass&) = delete;
public:
    DrawablePass(DrawablePass&&) = default;
    DrawablePass() = delete;
    ~DrawablePass();
    const MaterialPass&             mMaterialPass;
    VkPipeline                      mPipeline = VK_NULL_HANDLE; // Owned by us
    VkPipelineLayout                mPipelineLayout;            // Owned by shader
    std::vector<VkDescriptorSet>    mDescriptorSet;             // one per NUM_VULKAN_BUFFERS (double/triple buffering)
    const PipelineVertexInputState& mPipelineVertexInputState;  // contains vertex binding and attribute descriptions
    std::vector<VkBuffer>           mVertexBuffers;             // one per mVertexBuffersLookup entry.  May be vertex rate or instance rate.
    std::vector<VkDeviceSize>       mVertexBufferOffsets;       // offsets in to the above mVertexBuffers
    VkBuffer                        mIndexBuffer = VK_NULL_HANDLE;
    VkIndexType                     mIndexBufferType = VK_INDEX_TYPE_MAX_ENUM;
    uint32_t                        mNumVertices;
    uint32_t                        mNumIndices;
    uint32_t                        mPassIdx;                   // index of the bit in Drawable::m_passMask
};


/// Encapsulates a drawable object, owns the material (multiple passes, descriptor sets, etc) and the mesh (vertex list).
/// @ingroup Material
class Drawable
{
    Drawable(const Drawable&) = delete;
    Drawable& operator=(const Drawable&) = delete;
public:
    Drawable(Vulkan& vulkan, Material&& material);
    Drawable(Drawable&&) noexcept;
    ~Drawable();

    /// Initialize this Drawable with the given mesh and (single) render pass.
    bool Init(VkRenderPass vkRenderPass, const char* passNames, MeshObject meshObject, std::optional<VertexBufferObject> vertexInstanceBuffer = std::nullopt, const VkSampleCountFlagBits* const passMultisample = nullptr, const uint32_t* const subpasses = nullptr);
    /// Initialize this Drawable with the given mesh and render passes.
    bool Init( tcb::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, MeshObject meshObject, std::optional<VertexBufferObject> vertexInstanceBuffer = std::nullopt, tcb::span<const VkSampleCountFlagBits> passMultisample = {}, tcb::span<const uint32_t> subpasses = {} );
    /// Re-initialize the drawable.  Used internally by Init but can be used by the user when the render pass has been modified.
    bool ReInit( VkRenderPass vkRenderPass, const char* passNames, const VkSampleCountFlagBits* const passMultisample, const uint32_t* const subpasses );
    /// Re-initialize the drawable.  Used internally by Init but can be used by the user when the render passes have been modified.
    bool ReInit(tcb::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, tcb::span<const VkSampleCountFlagBits> passMultisample, tcb::span<const uint32_t> subpasses);

    /// Issues the Vulkan commands needed to draw this DrawablePass.
    /// Binds the pipeline, descriptor sets, vertex buffers, index buffers, and issues the appropriate vkCmdDraw*
    void DrawPass(VkCommandBuffer cmdBuffer, const DrawablePass& drawablePass, uint32_t bufferIdx) const;

    const DrawablePass* GetDrawablePass(const std::string& passName) const;
    const auto& GetDrawablePasses() const { return mPasses; };
    uint32_t GetPassMask() const { return mPassMask; }
    const auto& GetMaterial() const { return mMaterial; }
    const auto& GetInstances() const { return mVertexInstanceBuffer; }

protected:
    Material                        mMaterial;
    MeshObject                      mMeshObject;

    std::vector<DrawablePass>       mPasses;
    std::map<std::string, uint32_t> mPassNameToIndex;   // Index in to mpasses  ///TODO: allow for generation of a list of these - so each pass can iterate through the passes easily
    uint32_t                        mPassMask = 0;

    std::optional<VertexBufferObject> mVertexInstanceBuffer;

    Vulkan&                         mVulkan;
};


/// Wrapper class for LoadDrawables (user entry point for loading a drawable mesh object).
/// @ingroup Material
class DrawableLoader
{
    DrawableLoader(const DrawableLoader&) = delete;
    DrawableLoader& operator=(const DrawableLoader&) = delete;
public:
    /// @brief Load a mesh object and create the @Drawable(s) for rendering it.
    /// This is the recommended way of loading meshes in to the Framework Material system.
    /// @param numRenderPasses number of passes in vkRenderPasses and renderPassNames
    /// @param vkRenderPasses array of Vulkan render passes that we may want to create DrawablePasses for (can be duplicated if we have subpasses)
    /// @param renderPassNames names of the render passes, expected to match the names inside the Material 
    /// @param meshFilename name of the mesh filename to load (via assetManager)
    /// @param materialLoader user supplied function that returns a Material for each mesh material (@ref MeshObjectIntermediate::MaterialDef) that the loaded mesh returns.  Meshes can have multiple materials (possibly hundreds).
    /// @param drawables output vector of Drawable
    /// @param renderPassMultisample optional multisample flags (if zero size assume no multisampling)
    /// @param useInstancing pass true if drawable loader should try to find duplicated instances of meshes (same MaterialDef, same vertex uv sets, vertex positions onlly differing by rotation and translation).  Can take a little time to process.
    /// @param RenderPassSubpasses subpass indices for each render pass (0 for first subpass of if there are no subpasses).  If empty treat everything as using subpass 0
    /// @return true on success
    static bool LoadDrawables(Vulkan& vulkan, AssetManager& assetManager, tcb::span<VkRenderPass> vkRenderPasses, const char* const* renderPassNames, const std::string& meshFilename, const std::function<std::optional<Material>(const MeshObjectIntermediate::MaterialDef&)>& materialLoader, std::vector<Drawable>& drawables, tcb::span<const VkSampleCountFlagBits> renderPassMultisample, bool useInstancing, tcb::span<const uint32_t> renderPassSubpasses);
};
