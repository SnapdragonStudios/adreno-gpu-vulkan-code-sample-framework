//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once


#include "../drawableLoader.hpp"

#include "../drawable.hpp"
#include "vulkan/vulkan.hpp"
#include "memory/vulkan/drawIndirectBufferObject.hpp"
#include "memory/vulkan/indexBufferObject.hpp"
#include "memory/vulkan/vertexBufferObject.hpp"
#include "pipeline.hpp"
#include "pipelineVertexInputState.hpp"
#include "vulkan/renderContext.hpp"


// Forward Declarations
class MaterialBase;
template<typename T_GFXAPI> class IndexBuffer;
template<typename T_GFXAPI> class Material;
template<typename T_GFXAPI> class MaterialPass;
template<typename T_GFXAPI> class Shader;
template<typename T_GFXAPI> class RenderContext;

class Vulkan;
struct MeshInstance;


/// Collection of VkBuffers for vertex buffer stream(s) (including instance buffers).
/// @ingroup Material
template<>
struct DrawablePassVertexBuffers<Vulkan> : public DrawablePassVertexBuffersBase
{
    std::vector<VkBuffer>       mVertexBuffers;
    std::vector<VkDeviceSize>   mVertexBufferOffsets;
};

/// Encapsulates a drawable pass.  Specialized for Vulkan.
/// Owns the Vulkan pipeline, descriptor sets required by that pipeline.  References vertex/index buffers etc from the parent Drawable.
/// Users are expected to use Drawable (which contains a vector of DrawablePasses and is more 'user friendly').
/// @ingroup Material
template<>
class DrawablePass<Vulkan> : public DrawablePassBase
{
    DrawablePass(const DrawablePass<Vulkan>&) = delete;
    DrawablePass<Vulkan>& operator=(const DrawablePass<Vulkan>&) = delete;
    DrawablePass& operator=( DrawablePass<Vulkan>&& ) noexcept = delete;
public:
    DrawablePass( DrawablePass<Vulkan>&& ) noexcept = default;
    DrawablePass() = delete;
    ~DrawablePass();
    DrawablePass(const MaterialPass<Vulkan>&   MaterialPass,
                Pipeline<Vulkan>                Pipeline,
                VkPipelineLayout                PipelineLayout,
                std::vector<VkDescriptorSet>    DescriptorSet,
                const PipelineVertexInputState<Vulkan>& PipelineVertexInputState,
                DrawablePassVertexBuffers<Vulkan> VertexBuffers,
                VkBuffer                        IndexBuffer,
                VkIndexType                     IndexBufferType,
                VkBuffer                        DrawIndirectBuffer,
                VkBuffer                        DrawIndirectCountBuffer,
                uint32_t                        NumVertices,
                uint32_t                        NumIndices,
                uint32_t                        NumDrawIndirect,
                uint32_t                        DrawIndirectOffset,
                uint32_t                        PassIdx
    ) : mMaterialPass( MaterialPass ), mPipeline( std::move(Pipeline) ), mPipelineLayout( PipelineLayout ), mDescriptorSet( DescriptorSet ), mPipelineVertexInputState( PipelineVertexInputState ), mVertexBuffers( VertexBuffers ), mIndexBuffer( IndexBuffer ), mIndexBufferType( IndexBufferType ), mDrawIndirectBuffer( DrawIndirectBuffer ), mDrawIndirectCountBuffer( DrawIndirectCountBuffer ), mNumVertices( NumVertices ),
            mNumIndices( NumIndices), mNumDrawIndirect( NumDrawIndirect),  mDrawIndirectOffset( DrawIndirectOffset), mPassIdx( PassIdx)
    {
    }

    const MaterialPass<Vulkan>&     mMaterialPass;
    Pipeline<Vulkan>                mPipeline;                  // Owned by us
    VkPipelineLayout                mPipelineLayout;            // Owned by shader
    std::vector<VkDescriptorSet>    mDescriptorSet;             // one per NUM_VULKAN_BUFFERS (double/triple buffering)
    const PipelineVertexInputState<Vulkan>& mPipelineVertexInputState;  // contains vertex binding and attribute descriptions
    DrawablePassVertexBuffers<Vulkan> mVertexBuffers;             // contains vkbuffer/offsets; one per mVertexBuffersLookup entry.  May be vertex rate or instance rate.
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


class PipelineRasterizationInfo {
    // contents of D3D12_GRAPHICS_PIPELINE_STATE_DESC or VkPipelineRasterizationStateCreateInfo
};

