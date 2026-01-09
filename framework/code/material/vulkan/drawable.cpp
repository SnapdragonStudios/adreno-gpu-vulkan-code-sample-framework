//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "drawable.hpp"
#include "vulkan/vulkan.hpp"
#include "material.hpp"
#include "shader.hpp"
#include "../shaderDescription.hpp"
#include "vulkan/commandBuffer.hpp"
#include "vulkan/extensionLib.hpp"

namespace
{
    inline VkSampleCountFlagBits EnumToVk(Msaa msaa)
    {
        return (VkSampleCountFlagBits)msaa;
    }
}

DrawablePass<Vulkan>::~DrawablePass()
{
}

template<>
Drawable<Vulkan>::~Drawable()
{
}

template<>
bool Drawable<Vulkan>::ReInit( std::span<const RenderContext> renderContexts, uint32_t passMask )
{
    mPassMask = passMask;
    mPassNameToIndex.clear();
    mPasses.clear();

    const auto& shader = mMaterial.GetShader();
    mPasses.reserve(shader.GetShaderPasses().size());

    // we allow either no pass names or a single empty passName to denote that we want to just process the passes in order, otherwise lookup by name
//    const bool lookupPassNames = !passNames.empty() && (passNames.size() > 1 || (passNames[0] != nullptr && passNames[0][0] != '\0'));

    for (uint32_t passIdx = 0; passIdx < sizeof(passMask) * 8 && passMask != 0 && passIdx < renderContexts.size(); ++passIdx)
    {
        bool passMaskSet = ((passMask & 1) != 0);
        passMask >>= 1;
        if (passMaskSet)
        {
            const auto& renderContext = renderContexts[passIdx];
            // LOGI("Creating Mesh Object PipelineState and Pipeline for pass... %s", renderContext.name.c_str());
            auto* pMaterialPass = renderContext.name.empty() ? mMaterial.GetMaterialPass(passIdx) : mMaterial.GetMaterialPass(renderContext.name);
            if (!pMaterialPass)
            {
                LOGE("  Pass %s does not exist in shader", renderContext.name.c_str());
                continue;
            }
            assert(pMaterialPass);
            const auto& shaderPass = pMaterialPass->GetShaderPass();

            mPassNameToIndex.try_emplace(renderContext.name, (uint32_t)mPasses.size());   // add the lookup (in to mPasses)

            // Build the passVertexBufferLookup so at runtime we can easily populate the vkBuffer array with the vertex and instance buffers in the order specified per pass by m_vertexFormatBindings.
            // Could do this in a single loop; currently split into 2 so we can potentially add more flexibility in where we get the VKBuffers from (TODO)
            std::vector<int> tmp;
            tmp.reserve(shader.m_shaderDescription->m_vertexFormats.size());
            int numVertexRateFormats = 0, numInstanceRateFormats = 0;
            for (const auto& vertexFormat : shader.m_shaderDescription->m_vertexFormats)
            {
                switch (vertexFormat.inputRate) {
                case VertexFormat::eInputRate::Vertex:
                    tmp.push_back(numVertexRateFormats++);
                    break;
                case VertexFormat::eInputRate::Instance:
                    tmp.push_back(--numInstanceRateFormats);
                    break;
                }
            }

            std::vector<uint32_t> passVertexBufferLookup; // order of the vkBuffers for this pass (index is in to the vertex array if positive, or in to the instance array if negative (-1 is the 'first')
            std::vector<VkBuffer> passVertexBuffers;
            passVertexBufferLookup.reserve(shader.m_shaderDescription->m_vertexFormats.size());
            passVertexBuffers.reserve(shader.m_shaderDescription->m_vertexFormats.size());
            for (uint32_t formatBindingIdx : shaderPass.m_shaderPassDescription.m_vertexFormatBindings)
            {
                const int bufferIdx = tmp[formatBindingIdx];
                passVertexBufferLookup.push_back(bufferIdx);
                if (bufferIdx >= 0)
                {
                    // Vertex rate data (ie the mesh)
                    passVertexBuffers.push_back(mMeshObject.m_VertexBuffers[bufferIdx].GetVkBuffer());

                    // Double check the mesh is supplying the data we expect in the shader, may mismatch if the mesh was not built using the vertex format (eg Mesh<Vulkan>::CreateScreenSpaceMesh)
                    // We could dive even deeper, for now check the span and the number of attributes match
                    assert(mMeshObject.m_VertexBuffers[bufferIdx].GetAttributes().size() == shader.m_shaderDescription->m_vertexFormats[formatBindingIdx].elements.size());
                    assert(mMeshObject.m_VertexBuffers[bufferIdx].GetSpan() == shader.m_shaderDescription->m_vertexFormats[formatBindingIdx].span);
                }
                else
                {
                    // Instance rate data (ie the instance buffer)
                    assert(bufferIdx == -1);
                    assert(mVertexInstanceBuffer.has_value());
                    passVertexBuffers.push_back(mVertexInstanceBuffer->GetVkBuffer());
                }
            }
            std::vector<VkDeviceSize> passVertexBufferOffsets(passVertexBuffers.size(), 0);

            // Index buffer is optional
            VkBuffer indexBuffer = VK_NULL_HANDLE;
            VkIndexType indexBufferType = VK_INDEX_TYPE_MAX_ENUM;
            size_t indexCount = 0;
            if (mMeshObject.m_IndexBuffer)
            {
                indexBuffer = mMeshObject.m_IndexBuffer->GetVkBuffer();
                indexBufferType = mMeshObject.m_IndexBuffer->GetVkIndexType();
                indexCount = mMeshObject.m_IndexBuffer->GetNumIndices();
            }

            // Indirect Draw buffer is optional
            VkBuffer drawIndirectBuffer = mDrawIndirectBuffer.has_value() ? mDrawIndirectBuffer->GetVkBuffer() : VK_NULL_HANDLE;
            uint32_t drawIndirectCount = mDrawIndirectBuffer.has_value() ? (uint32_t)mDrawIndirectBuffer->GetNumDraws() : 0;
            uint32_t drawIndirectOffset = mDrawIndirectBuffer.has_value() ? mDrawIndirectBuffer->GetBufferOffset() : 0;
            // Indirect Draw Count (count buffer) set to be the beginning of the drawIndirectBuffer IF there is an offset in the mDrawIndirectBuffer.
            VkBuffer drawIndirectCountBuffer = drawIndirectOffset>0 ? drawIndirectBuffer : VK_NULL_HANDLE;

            Pipeline pipeline;

            // Pipeline (and layout) may come from the shaderPass or (if that fails) from the materialPass (if it was created late because of 'dynamic' descriptor set layout).
            bool materialSpecificPipeline = !shaderPass.GetPipelineLayout();
            const PipelineLayout<Vulkan>& pipelineLayout = materialSpecificPipeline ? pMaterialPass->GetPipelineLayout() : shaderPass.GetPipelineLayout();
            if (!materialSpecificPipeline)
            {
                // We (probably) have a valid pipeline we can (re)use
                pipeline = !renderContext.IsDynamic() ? renderContext.GetOverridePipeline() : Pipeline();
            }
            if (!pipeline)
            {
                PipelineRasterizationState<Vulkan> pipelineRasterizationState { shaderPass.m_shaderPassDescription };
                pipeline = CreatePipeline( mGfxApi, shaderPass.m_shaderPassDescription, pipelineLayout, shaderPass.GetPipelineVertexInputState(), pipelineRasterizationState, pMaterialPass->GetSpecializationConstants(), shaderPass.m_shaders, renderContext, renderContext.msaa);
            }

            // add the DrawablePass
            DrawablePass& pass = mPasses.emplace_back( *pMaterialPass,
                                                        std::move(pipeline),
                                                        pipelineLayout.GetVkPipelineLayout(),
                                                        pMaterialPass->GetVkDescriptorSets(),
                                                        shaderPass.GetPipelineVertexInputState(),
                                                        DrawablePassVertexBuffers { .mVertexBuffers = std::move( passVertexBuffers ),
                                                                                    .mVertexBufferOffsets = std::move( passVertexBufferOffsets ) },
                                                        indexBuffer,
                                                        indexBufferType,
                                                        drawIndirectBuffer,
                                                        drawIndirectCountBuffer,
                                                        (uint32_t)mMeshObject.m_NumVertices,
                                                        (uint32_t)indexCount,
                                                        (uint32_t)drawIndirectCount,
                                                        (uint32_t)drawIndirectOffset,
                                                        passIdx
                                                      );
        }
    }
    return true;
}

template<>
bool Drawable<Vulkan>::ReInitMeshShader(std::span<const RenderContext> renderContexts, uint32_t passMask)
{
    mPassMask = passMask;
    mPassNameToIndex.clear();
    mPasses.clear();

    const auto& shader = mMaterial.GetShader();
    mPasses.reserve( shader.GetShaderPasses().size() );
    for (uint32_t passIdx = 0; passIdx < sizeof( passMask ) * 8 && passMask != 0; ++passIdx)
    {
        bool passMaskSet = ((passMask & 1) != 0);
        passMask >>= 1;
        if (passMaskSet)
        {
            const auto& renderContext = renderContexts[passIdx];
            // LOGI("Creating Mesh Object PipelineState and Pipeline for pass... %s", passNames[passIdx]);
            auto* pMaterialPass = renderContext.name.empty() ? mMaterial.GetMaterialPass(passIdx) : mMaterial.GetMaterialPass(renderContext.name);
            if (!pMaterialPass)
            {
                LOGE("  Pass %s does not exist in shader", renderContext.name.c_str());
                continue;
            }
            assert(pMaterialPass);
            assert( pMaterialPass );
            const auto& shaderPass = pMaterialPass->GetShaderPass();

            mPassNameToIndex.try_emplace( renderContext.name, (uint32_t)mPasses.size() );   // add the lookup (in to mPasses)

            VkBuffer indexBuffer = VK_NULL_HANDLE;
            VkIndexType indexBufferType = VK_INDEX_TYPE_MAX_ENUM;

            // Indirect Draw buffer is optional
            VkBuffer drawIndirectBuffer = mDrawIndirectBuffer.has_value() ? mDrawIndirectBuffer->GetVkBuffer() : VK_NULL_HANDLE;
            uint32_t drawIndirectCount = mDrawIndirectBuffer.has_value() ? (uint32_t)mDrawIndirectBuffer->GetNumDraws() : 0;
            uint32_t drawIndirectOffset = mDrawIndirectBuffer.has_value() ? mDrawIndirectBuffer->GetBufferOffset() : 0;
            // Indirect Draw Count (count buffer) set to be the beginning of the drawIndirectBuffer IF there is an offset in the mDrawIndirectBuffer.
            VkBuffer drawIndirectCountBuffer = drawIndirectOffset > 0 ? drawIndirectBuffer : VK_NULL_HANDLE;

            Pipeline pipeline;

            // Pipeline (and layout) may come from the shaderPass or (if that fails) from the materialPass (if it was created late because of 'dynamic' descriptor set layout).
            bool materialSpecificPipeline = !shaderPass.GetPipelineLayout();
            const PipelineLayout<Vulkan>& pipelineLayout = materialSpecificPipeline ? pMaterialPass->GetPipelineLayout() : shaderPass.GetPipelineLayout();
            if (!renderContext.IsDynamic())
            {
                if (!materialSpecificPipeline && !renderContext.IsDynamic())
                {
                    // We (probably) have a valid pipeline we can (re)use
                    pipeline = renderContext.GetOverridePipeline();
                }
                if (!pipeline)
                {
                    PipelineRasterizationState<Vulkan> pipelineRasterizationState{shaderPass.m_shaderPassDescription};
                    pipeline = CreatePipeline( mGfxApi, shaderPass.m_shaderPassDescription, pipelineLayout, shaderPass.GetPipelineVertexInputState(), pipelineRasterizationState, pMaterialPass->GetSpecializationConstants(), shaderPass.m_shaders, renderContext, renderContext.msaa);
                }
            }

            // add the DrawablePass
            DrawablePass& pass = mPasses.emplace_back( *pMaterialPass,
                                                       std::move(pipeline),
                                                       pipelineLayout.GetVkPipelineLayout(),
                                                       pMaterialPass->GetVkDescriptorSets(),
                                                       shaderPass.GetPipelineVertexInputState(),
                                                       DrawablePassVertexBuffers { }, //{ .mVertexBuffers = std::move( passVertexBuffers ), .mVertexBufferOffsets = std::move( passVertexBufferOffsets ) },
                                                       VK_NULL_HANDLE, //indexBuffer,
                                                       indexBufferType,
                                                       drawIndirectBuffer,
                                                       drawIndirectCountBuffer,
                                                       0, //(uint32_t)mMeshObject.m_NumVertices,
                                                       0, //(uint32_t)indexCount,
                                                       (uint32_t)drawIndirectCount,
                                                       (uint32_t)drawIndirectOffset,
                                                       passIdx
            );
        }
    }
    return true;
}

template<>
void Drawable<Vulkan>::DrawPass(CommandList& cmdBuffer, const DrawablePass& drawablePass, uint32_t bufferIdx, const std::span<DrawablePassVertexBuffers> vertexBufferOverrides) const
{
    VkCommandBuffer vkCmdBuffer = cmdBuffer;

    // Bind the pipeline for this material
    vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, drawablePass.mPipeline.GetVkPipeline());

    // Bind everything the shader needs
    if (!drawablePass.mDescriptorSet.empty())
    {
        VkDescriptorSet vkDescriptorSet = drawablePass.mDescriptorSet.size() >= 1 ? drawablePass.mDescriptorSet[bufferIdx] : drawablePass.mDescriptorSet[0];
        vkCmdBindDescriptorSets(vkCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            drawablePass.mPipelineLayout,
            0,
            1,
            &vkDescriptorSet,
            0,
            NULL);
    }

    const auto& shaderPassDescription = drawablePass.mMaterialPass.mShaderPass.m_shaderPassDescription;
    if (shaderPassDescription.m_meshName.empty())
    {
        //
        // Traditional vert (and maybe frag) shader rasterization pipeline
        //
        const auto& vertexBuffers = vertexBufferOverrides.empty() ? drawablePass.mVertexBuffers : vertexBufferOverrides[bufferIdx % vertexBufferOverrides.size()];

        if (!vertexBuffers.mVertexBuffers.empty())
        {
            // Bind mesh vertex/instance buffer(s)
            vkCmdBindVertexBuffers( vkCmdBuffer,
                                    0,
                                    (uint32_t)vertexBuffers.mVertexBuffers.size(),
                                    vertexBuffers.mVertexBuffers.data(),
                                    vertexBuffers.mVertexBufferOffsets.data() );
        }
        if (!vertexBufferOverrides.empty())
        {
            assert( vertexBuffers.mVertexBuffers.size() == vertexBuffers.mVertexBufferOffsets.size() );
            assert( vertexBufferOverrides.empty() || vertexBuffers.mVertexBuffers.size() == drawablePass.mVertexBuffers.mVertexBuffers.size() );
        }

        if (drawablePass.mIndexBuffer != VK_NULL_HANDLE)
        {
            assert( drawablePass.mIndexBufferType != VK_INDEX_TYPE_MAX_ENUM );

            // Bind index buffer data
            vkCmdBindIndexBuffer(vkCmdBuffer,
                drawablePass.mIndexBuffer,
                0,
                drawablePass.mIndexBufferType);

            if (drawablePass.mDrawIndirectBuffer != VK_NULL_HANDLE)
            {
                if (drawablePass.mDrawIndirectCountBuffer != VK_NULL_HANDLE)
                {
                    // Draw the mesh using draw indirect cont buffer (VkDrawIndexedIndirectCount command)
                    const auto* drawIndirectCountExt = mGfxApi.GetExtension<ExtensionLib::Ext_VK_KHR_draw_indirect_count>();
                    assert( drawIndirectCountExt != nullptr && drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR != nullptr);
                    drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR(vkCmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mDrawIndirectCountBuffer, 0, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndexedIndirectCommand));
                }
                else
                    // Draw the mesh using draw indirect buffer (VkDrawIndexedIndirectCommand)
                    vkCmdDrawIndexedIndirect(vkCmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndexedIndirectCommand));
            }
            else
            {
                // Everything is set up, draw the mesh
                vkCmdDrawIndexed(vkCmdBuffer, drawablePass.mNumIndices, GetInstances() ? (uint32_t)GetInstances()->GetNumVertices() : 1, 0, 0, 0);
            }
        }
        else
        {
            if (drawablePass.mDrawIndirectBuffer != VK_NULL_HANDLE)
            {
                if (drawablePass.mDrawIndirectCountBuffer != VK_NULL_HANDLE)
                {
                    // Draw the mesh using draw indirect buffer (VkDrawIndirectCommand - no index buffer)
                    const auto* drawIndirectCountExt = mGfxApi.GetExtension<ExtensionLib::Ext_VK_KHR_draw_indirect_count>();
                    assert( drawIndirectCountExt != nullptr && drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR != nullptr );
                    drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR(vkCmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mDrawIndirectCountBuffer, 0, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndirectCommand));
                }
                else
                    // Draw the mesh using draw indirect buffer (VkDrawIndirectCommand - no index buffer)
                    vkCmdDrawIndirect(vkCmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndirectCommand));
            }
            else
            {
                // Draw the mesh without index buffer
                vkCmdDraw(vkCmdBuffer, drawablePass.mNumVertices, GetInstances() ? (uint32_t)GetInstances()->GetNumVertices() : 1, 0, 0);
            }
        }
    }
    else
    {
        //
        // Mesh shader pipeline
        //
        auto* meshExtension = mGfxApi.GetExtension<ExtensionLib::Ext_VK_KHR_mesh_shader>();
        if (!meshExtension || meshExtension->Status != VulkanExtensionStatus::eLoaded)
            assert( 0 && "mesh shader extension not loader or supported" );
        else if (!shaderPassDescription.m_taskName.empty())
        {
            // Task, mesh (and frag) shader
            meshExtension->m_vkCmdDrawMeshTasksEXT( vkCmdBuffer, mDispatchGroupCount[0], mDispatchGroupCount[1], mDispatchGroupCount[2] );
        }
        else
        {
            // Mesh (and frag) only, no task shader
            if (drawablePass.mDrawIndirectBuffer != VK_NULL_HANDLE)
            {
                if (drawablePass.mDrawIndirectCountBuffer != VK_NULL_HANDLE)
                {
                    // Draw the mesh using Mesh indirect count buffer (vkCmdDrawMeshTasksIndirectCountEXT command)
                    assert(meshExtension->m_vkCmdDrawMeshTasksIndirectCountEXT != nullptr);
                    meshExtension->m_vkCmdDrawMeshTasksIndirectCountEXT(
                        vkCmdBuffer, 
                        drawablePass.mDrawIndirectBuffer, 
                        drawablePass.mDrawIndirectOffset, 
                        drawablePass.mDrawIndirectCountBuffer, 
                        0, 
                        drawablePass.mNumDrawIndirect, 
                        sizeof(VkDrawMeshTasksIndirectCommandEXT));
                }
                else
                {
                    // Draw the mesh using Mesh indirect buffer (vkCmdDrawMeshTasksIndirectEXT)
                    assert(meshExtension->m_vkCmdDrawMeshTasksIndirectEXT!= nullptr);
                    meshExtension->m_vkCmdDrawMeshTasksIndirectEXT(
                        vkCmdBuffer,
                        drawablePass.mDrawIndirectBuffer, 
                        drawablePass.mDrawIndirectOffset, 
                        drawablePass.mNumDrawIndirect, 
                        sizeof(VkDrawMeshTasksIndirectCommandEXT));
                }
            }
        }
    }
}
