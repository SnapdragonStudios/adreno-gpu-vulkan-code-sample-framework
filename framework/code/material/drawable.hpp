//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <optional>
#include "vulkan/vulkan.hpp"
#include "mesh/mesh.hpp"
#include "material/material.hpp"
#include "memory/vulkan/indexBufferObject.hpp"
#include "memory/vulkan/vertexBufferObject.hpp"
#include "memory/vulkan/drawIndirectBufferObject.hpp"
#include "mesh/meshIntermediate.hpp"
#include "vulkan/pipelineVertexInputState.hpp"

// Forward Declarations
class Material;
template<typename T_GFXAPI> class ShaderT;
class Vulkan;
struct MeshInstance;


/// Collection of VkBuffers for vertex buffer stream(s) (including instance buffers).
/// @ingroup Material
struct DrawablePassVertexBuffers
{
    std::vector<VkBuffer>       mVertexBuffers;
    std::vector<VkDeviceSize>   mVertexBufferOffsets;
};

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
    DrawablePass(const MaterialPass& MaterialPass,
                VkPipeline                      Pipeline,
                VkPipelineLayout                PipelineLayout,
                std::vector<VkDescriptorSet>    DescriptorSet,
                const PipelineVertexInputState<Vulkan>& PipelineVertexInputState,
                DrawablePassVertexBuffers       VertexBuffers,
                VkBuffer                        IndexBuffer,
                VkIndexType                     IndexBufferType,
                VkBuffer                        DrawIndirectBuffer,
                VkBuffer                        DrawIndirectCountBuffer,
                uint32_t                        NumVertices,
                uint32_t                        NumIndices,
                uint32_t                        NumDrawIndirect,
                uint32_t                        DrawIndirectOffset,
                uint32_t                        PassIdx
    ) : mMaterialPass( MaterialPass ), mPipeline( Pipeline ), mPipelineLayout( PipelineLayout ), mDescriptorSet( DescriptorSet ), mPipelineVertexInputState( PipelineVertexInputState ), mVertexBuffers( VertexBuffers ), mIndexBuffer( IndexBuffer ), mIndexBufferType( IndexBufferType ), mDrawIndirectBuffer( DrawIndirectBuffer ), mDrawIndirectCountBuffer( DrawIndirectCountBuffer ), mNumVertices( NumVertices ),
            mNumIndices( NumIndices), mNumDrawIndirect( NumDrawIndirect),  mDrawIndirectOffset( DrawIndirectOffset), mPassIdx( PassIdx)
    {
    }

    const MaterialPass&             mMaterialPass;
    VkPipeline                      mPipeline = VK_NULL_HANDLE; // Owned by us
    VkPipelineLayout                mPipelineLayout;            // Owned by shader
    std::vector<VkDescriptorSet>    mDescriptorSet;             // one per NUM_VULKAN_BUFFERS (double/triple buffering)
    const PipelineVertexInputState<Vulkan>& mPipelineVertexInputState;  // contains vertex binding and attribute descriptions
    DrawablePassVertexBuffers       mVertexBuffers;             // contains vkbuffer/offsets; one per mVertexBuffersLookup entry.  May be vertex rate or instance rate.
    VkBuffer                        mIndexBuffer = VK_NULL_HANDLE;
    VkIndexType                     mIndexBufferType = VK_INDEX_TYPE_MAX_ENUM;
    VkBuffer                        mDrawIndirectBuffer = VK_NULL_HANDLE;
    VkBuffer                        mDrawIndirectCountBuffer = VK_NULL_HANDLE;
    uint32_t                        mNumVertices;
    uint32_t                        mNumIndices;
    uint32_t                        mNumDrawIndirect;
    uint32_t                        mDrawIndirectOffset;        // if non zero offset mDrawIndirectBuffer by this
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

    /// Initialize this Drawable with the given mesh and render passes.
    bool Init(std::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, std::optional<DrawIndirectBufferObject> indirectDrawBuffer = std::nullopt, std::span<const VkSampleCountFlagBits> passMultisample = {}, std::span<const uint32_t> subpasses = {}, int nodeId = -1);

    /// Initialize this Drawable with the given mesh and (single) render pass.
    bool Init(VkRenderPass vkRenderPass, const char* passNames, Mesh<Vulkan> meshObject, std::optional<VertexBufferObject> vertexInstanceBuffer = std::nullopt, std::optional<DrawIndirectBufferObject> indirectDrawBuffer = std::nullopt, const VkSampleCountFlagBits* const passMultisample = nullptr, const uint32_t* const subpasses = nullptr, int nodeId = -1);
    /// Initialize this Drawable with the given mesh and render passes.
    bool Init( std::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, Mesh<Vulkan> meshObject, std::optional<VertexBufferObject> vertexInstanceBuffer = std::nullopt, std::optional<DrawIndirectBufferObject> indirectDrawBuffer = std::nullopt, std::span<const VkSampleCountFlagBits> passMultisample = {}, std::span<const uint32_t> subpasses = {}, int nodeId = -1);
    /// Initialize this Drawable with the given mesh, (single) render pass, and raster information (triangles, lines, points, etc.).
    bool Init(VkRenderPass vkRenderPass, const char* passName, Mesh<Vulkan> meshObject, VkPipelineInputAssemblyStateCreateInfo  ia_custom, VkPipelineRasterizationStateCreateInfo  rs_custom);

    bool InitMeshShader( std::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, std::optional<DrawIndirectBufferObject> indirectDrawBuffer = std::nullopt, std::span<const VkSampleCountFlagBits> passMultisample = {}, std::span<const uint32_t> subpasses = {}, int nodeId = -1);

    /// Re-initialize the drawable.  Used internally by Init but can be used by the user when the render pass has been modified.
    bool ReInit( VkRenderPass vkRenderPass, const char* passNames, const VkSampleCountFlagBits* const passMultisample, const uint32_t* const subpasses );
    /// Re-initialize the drawable.  Used internally by Init but can be used by the user when the render pass has been modified.
    bool ReInit( VkRenderPass vkRenderPass, const char* passNames, const VkSampleCountFlagBits* const passMultisample, const uint32_t* const subpasses, VkPipelineInputAssemblyStateCreateInfo  ia_custom, VkPipelineRasterizationStateCreateInfo  rs_custom);
    /// Re-initialize the drawable.  Used internally by Init but can be used by the user when the render passes have been modified.
    bool ReInit(std::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, std::span<const VkSampleCountFlagBits> passMultisample, std::span<const uint32_t> subpasses);
    /// Re-initialize the drawable.  Used internally by Init but can be used by the user when the render passes have been modified.
    bool ReInit(std::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, std::span<const VkSampleCountFlagBits> passMultisample, std::span<const uint32_t> subpasses, VkPipelineInputAssemblyStateCreateInfo ia_custom, VkPipelineRasterizationStateCreateInfo rs_custom);


    bool ReInitMeshShader(std::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, std::span<const VkSampleCountFlagBits> passMultisample, std::span<const uint32_t> subpasses);

    /// Issues the Vulkan commands needed to draw this DrawablePass.
    /// Binds the pipeline, descriptor sets, vertex buffers, index buffers, and issues the appropriate vkCmdDraw*
    /// @param vertexBindingsOverride allows user to replace @DrawablePass::mVertexBuffers with their own.  Span is for each 'bufferIdx' (can be size()==1 if all buffers bind the same).  DrawablePassVertexBuffers contains multiple buffers so all  mesh and instance streams are overridden.
    void DrawPass(VkCommandBuffer cmdBuffer, const DrawablePass& drawablePass, uint32_t bufferIdx, const std::span<DrawablePassVertexBuffers> vertexBuffersOverride = {}) const;

    const DrawablePass* GetDrawablePass(const std::string& passName) const;
    const auto& GetDrawablePasses() const { return mPasses; };
    uint32_t GetPassMask() const { return mPassMask; }
    const auto& GetMaterial() const { return mMaterial; }
    const auto& GetMeshObject() const { return mMeshObject; }
    const auto& GetInstances() const { return mVertexInstanceBuffer; }
    const auto& GetDrawIndirectBuffer() const { return mDrawIndirectBuffer; }
    const int GetNodeId() const { return mNodeId; }

    /// number of workgroup dispatches to execute (value after the local workgroup sizes are accounted for)
    void SetDispatchGroupCount(std::array<uint32_t, 3> count) { mDispatchGroupCount = count; }
    const auto& GetDispatchGroupCount() const { return mDispatchGroupCount; }

    // Need non const access to mMeshObject but don't want to break anyone using GetMeshObject()
public:
    Mesh<Vulkan>                    mMeshObject;

protected:
    Material                        mMaterial;

    std::array<uint32_t, 3>         mDispatchGroupCount{ 1u,1u,1u };

    std::vector<DrawablePass>       mPasses;
    std::map<std::string, uint32_t> mPassNameToIndex;   // Index in to mpasses  ///TODO: allow for generation of a list of these - so each pass can iterate through the passes easily
    uint32_t                        mPassMask = 0;
    int                             mNodeId = -1;       // Identifier used by application to determine what this drawable is attached to, eg for attaching to animations.  Not used by Drawable.

    std::optional<VertexBufferObject> mVertexInstanceBuffer;
    std::optional<DrawIndirectBufferObject> mDrawIndirectBuffer;

    Vulkan&                         mVulkan;
};


/// Wrapper class for LoadDrawables (user entry point for loading a drawable mesh object).
/// @ingroup Material
class DrawableLoader
{
    DrawableLoader(const DrawableLoader&) = delete;
    DrawableLoader& operator=(const DrawableLoader&) = delete;
public:
    /// @brief Flags controlling the behaviour of LoadDrawables
    enum LoaderFlags : uint32_t {
        None = 0,
        FindInstances = 0x1,    // useInstancing pass true if drawable loader should try to find duplicated instances of meshes(same MaterialDef, same vertex uv sets, vertex positions onlly differing by rotation and translation). Can take a little time to process.
        BakeTransforms = 0x2,   // bake world transform in to mesh data (and clear the m_Transform for all baked drawables)
        IgnoreHierarchy = 0x4   // Ignore the gltf node hierarchy when loading model
    };

    /// @brief Load a mesh object and create the @Drawable(s) for rendering it.
    /// This is the recommended way of loading meshes in to the Framework Material system.
    /// @param vkRenderPasses span of Vulkan render passes that we may want to create DrawablePasses for (can be duplicated if we have subpasses)
    /// @param renderPassNames names of the render passes, expected to match the names inside the Material 
    /// @param meshFilename name of the mesh filename to load (via assetManager)
    /// @param materialLoader user supplied function that returns a Material for each mesh material (@ref MeshObjectIntermediate::MaterialDef) that the loaded mesh returns.  Meshes can have multiple materials (possibly hundreds).
    /// @param drawables output vector of @Drawable objects
    /// @param renderPassMultisample optional multisample flags (if zero size assume no multisampling)
    /// @param loaderFlags loader feature enables
    /// @param renderPassSubpasses subpass indices for each render pass (0 for first subpass of if there are no subpasses).  If empty treat everything as using subpass 0
    /// @param globalScale global scale applied to every loaded Drawable object
    /// @return true on success
    static bool LoadDrawables(Vulkan& vulkan, AssetManager& assetManager, std::span<VkRenderPass> vkRenderPasses, const char* const* renderPassNames, const std::string& meshFilename, const std::function<std::optional<Material>(const MeshObjectIntermediate::MaterialDef&)>& materialLoader, std::vector<Drawable>& drawables, std::span<const VkSampleCountFlagBits> renderPassMultisample, /*LoaderFlags*/uint32_t loaderFlags, std::span<const uint32_t> renderPassSubpasses, const glm::vec3 globalScale = glm::vec3(1.0f,1.0f,1.0f));

    /// @brief Create @Drawable(s) for rendering a given vector of @MeshObjectIntermediate objects.
    /// This is the recommended way of creating meshes in the Framework Material system and is used by the LoadDrawables function.
    /// @param intermediateMeshObjects vector of the (intermediate format) mesh objects we are going to make drawables from.  CreateDrawables takes ownership of this data.
    /// @param vkRenderPasses span of Vulkan render passes that we may want to create DrawablePasses for (can be duplicated if we have subpasses)
    /// @param renderPassNames names of the render passes, expected to match the names inside the Material 
    /// @param meshFilename name of the mesh filename to load (via assetManager)
    /// @param materialLoader user supplied function that returns a Material for each mesh material (@ref MeshObjectIntermediate::MaterialDef) that the loaded mesh returns.  Meshes can have multiple materials (possibly hundreds).
    /// @param drawables output vector of @Drawable objects
    /// @param renderPassMultisample optional multisample flags (if zero size assume no multisampling)
    /// @param loaderFlags loader feature enables
    /// @param RenderPassSubpasses subpass indices for each render pass (0 for first subpass of if there are no subpasses).  If empty treat everything as using subpass 0
    /// @return true on success
    static bool CreateDrawables(Vulkan& vulkan, std::vector<MeshObjectIntermediate>&& intermediateMeshObjects, std::span<VkRenderPass> vkRenderPasses, const char* const* renderPassNames, const std::function<std::optional<Material>(const MeshObjectIntermediate::MaterialDef&)>& materialLoader, std::vector<Drawable>& drawables, const std::span<const VkSampleCountFlagBits> renderPassMultisample, /*DrawableLoader::LoaderFlags*/uint32_t loaderFlags, const std::span<const uint32_t> renderPassSubpasses);

    /// @brief Create @Drawables() for rendering the ficen @MeshInstance objects.
    /// Identical to CreateDrawables but does not generate the MeshInstance data (is required to be already generated).
    static bool CreateDrawables(Vulkan& vulkan, std::vector<MeshInstance>&& intermediateMeshInstances, std::span<VkRenderPass> vkRenderPasses, const char* const* renderPassNames, const std::function<std::optional<Material>(const MeshObjectIntermediate::MaterialDef&)>& materialLoader, std::vector<Drawable>& drawables, const std::span<const VkSampleCountFlagBits> renderPassMultisample, /*DrawableLoader::LoaderFlags*/uint32_t loaderFlags, const std::span<const uint32_t> renderPassSubpasses);

    /// @brief Print some combined statistics about the given meshObjects.
    /// @param meshObjects span of the objects we want to gather the statistics for.
    static void PrintStatistics(const std::span<MeshObjectIntermediate> meshObjects);

    struct MeshStatistics {
        size_t totalVerts;
        glm::vec3 boundingBoxMin;
        glm::vec3 boundingBoxMax;
    };

    /// @brief Collect some combined statistics about the given meshObjects.
    /// @param meshObjects span of the objects we want to gather the statistics for.
    /// @returns @DrawableLoaderMeshStatistics with statistics for the given objects (combined).
    static MeshStatistics GatherStatistics(const std::span<MeshObjectIntermediate> meshObjects);
};

