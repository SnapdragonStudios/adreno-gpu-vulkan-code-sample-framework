//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "drawableDx12.hpp"
#include "dx12/dx12.hpp"
#include "dx12/commandList.hpp"
#include "dx12/descriptorHeapManager.hpp"
#include "dx12/renderPass.hpp"
#include "material.hpp"
#include "pipeline.hpp"
#include "shader.hpp"
#include "../specializationConstants.hpp"
#include "material/shaderDescription.hpp"
#include "shaderModule.hpp"
#include "system/os_common.h"
#include "mesh/meshHelper.hpp"
#include "mesh/instanceGenerator.hpp"
#include <cassert>
#include <utility>

DrawablePass<Dx12>::~DrawablePass()
{
}

const DrawablePass<Dx12>* Drawable<Dx12>::GetDrawablePass(const std::string& passName) const
{
    auto it = mPassNameToIndex.find(passName);
    if (it != mPassNameToIndex.end())
    {
        return &mPasses[it->second];
    }
    return nullptr;
}


Drawable<Dx12>::~Drawable()
{
}

bool Drawable<Dx12>::ReInit(std::span<const RenderPass> renderPasses, std::span<const Pipeline> pipelines, std::span<const char*const> passNames, uint32_t passMask, std::span<const Msaa> passMultisample, std::span<const uint32_t> subpasses)
{
    assert( passMultisample.empty() || passMultisample.size() == passNames.size() );
    assert( renderPasses.size() == passNames.size() );

    mPassMask = passMask;
    mPassNameToIndex.clear();
    mPasses.clear();

    const auto& shader = mMaterial.GetShader();
    mPasses.reserve(shader.GetShaderPasses().size());
    for (uint32_t passIdx = 0; passIdx < sizeof(passMask) * 8; ++passIdx)
    {
        if (passMask & (1 << passIdx))
        {
            // LOGI("Creating Mesh Object PipelineState and Pipeline for pass... %s", passNames[passIdx]);
            auto* pMaterialPass = const_cast<MaterialPass<Dx12>*>(mMaterial.GetMaterialPass(passNames[passIdx]));
            if (!pMaterialPass)
            {
                LOGE("  Pass does not exist in shader");
                continue;
            }
            assert(pMaterialPass);
            const auto& shaderPass = pMaterialPass->GetShaderPass();
            PipelineRasterizationState<Dx12> rasterizationState;
            SpecializationConstants<Dx12> specializationConstants;

            Pipeline pipeline = pipelines[passIdx] 
                                    ? pipelines[passIdx].Copy()
                                    : CreatePipeline( mGfxApi,
                                                      shaderPass.m_shaderPassDescription,
                                                      shaderPass.GetPipelineLayout(),
                                                      shaderPass.GetPipelineVertexInputState(),
                                                      rasterizationState,
                                                      specializationConstants,
                                                      shaderPass.m_shaders,
                                                      renderPasses[passIdx],
                                                      subpasses.empty() ? 0 : subpasses[passIdx],
                                                      passMultisample.empty() ? Msaa::Samples1 : passMultisample[passIdx] );

            // Common to all pipelines
            mPassNameToIndex.try_emplace(passNames[passIdx], (uint32_t)mPasses.size());   // add the lookup (in to mPasses)

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
            DrawablePassVertexBuffers passVertexBuffers;
            passVertexBufferLookup.reserve(shader.m_shaderDescription->m_vertexFormats.size());
            passVertexBuffers.views.reserve(shader.m_shaderDescription->m_vertexFormats.size());
            for (uint32_t formatBindingIdx : shaderPass.m_shaderPassDescription.m_vertexFormatBindings)
            {
                const int bufferIdx = tmp[formatBindingIdx];
                passVertexBufferLookup.push_back(bufferIdx);
                if (bufferIdx >= 0)
                {
                    // Vertex rate data (ie the mesh)
                    passVertexBuffers.views.push_back(mMeshObject.m_VertexBuffers[bufferIdx].GetVertexBufferView());

                    // Double check the mesh is supplying the data we expect in the shader, may mismatch if the mesh was not built using the vertex format (eg Mesh<Dx12>::CreateScreenSpaceMesh)
                    // We could dive even deeper, for now check the span and the number of attributes match
                    //assert(mMeshObject.m_VertexBuffers[bufferIdx].GetAttributes().size() == shader.m_shaderDescription->m_vertexFormats[formatBindingIdx].elements.size());
                    assert(mMeshObject.m_VertexBuffers[bufferIdx].GetSpan() == shader.m_shaderDescription->m_vertexFormats[formatBindingIdx].span);
                }
                else
                {
                    // Instance rate data (ie the instance buffer)
                    assert(bufferIdx == -1);
                    assert(mVertexInstanceBuffer.has_value());
                    passVertexBuffers.views.push_back(mVertexInstanceBuffer->GetVertexBufferView());
                }
            }

            // Index buffer is optional
            D3D12_INDEX_BUFFER_VIEW indexBuffer = {};
            size_t indexCount = 0;
            if (mMeshObject.m_IndexBuffer)
            {
                indexBuffer = mMeshObject.m_IndexBuffer->GetIndexBufferView();
                indexCount = mMeshObject.m_IndexBuffer->GetNumIndices();
            }

            // Indirect Draw buffer is optional
//            VkBuffer drawIndirectBuffer = mDrawIndirectBuffer.has_value() ? mDrawIndirectBuffer->GetVkBuffer() : VK_NULL_HANDLE;
//            uint32_t drawIndirectCount = mDrawIndirectBuffer.has_value() ? (uint32_t)mDrawIndirectBuffer->GetNumDraws() : 0;
//            uint32_t drawIndirectOffset = mDrawIndirectBuffer.has_value() ? mDrawIndirectBuffer->GetBufferOffset() : 0;
//            // Indirect Draw Count (count buffer) set to be the beginning of the drawIndirectBuffer IF there is an offset in the mDrawIndirectBuffer.
//            VkBuffer drawIndirectCountBuffer = drawIndirectOffset>0 ? drawIndirectBuffer : VK_NULL_HANDLE;
//
//            // Pipeline layout may come from the shaderPass or (if that fails) from the materialPass (if it was created late because of 'dynamic' descriptor set layout).
//            VkPipelineLayout pipelineLayout = shaderPass.GetPipelineLayout().GetVkPipelineLayout();
//            if (pipelineLayout == VK_NULL_HANDLE)
//                pipelineLayout = pMaterialPass->GetPipelineLayout().GetVkPipelineLayout();

            // add the DrawablePass
            DrawablePass& pass = mPasses.emplace_back(  *pMaterialPass,
                                                        std::move( pipeline ),
                                                        pMaterialPass->GetRootData(),
                                                        pMaterialPass->GetDescriptorTables(),
                                                        std::move( passVertexBuffers ),
                                                        indexBuffer,
                                                        (uint32_t)mMeshObject.m_NumVertices,
                                                        (uint32_t)indexCount,
                                                        passIdx
                                                      );
        }
    }
    return true;
}

void Drawable<Dx12>::DrawPass(CommandList& cmdList, const DrawablePass& drawablePass, uint32_t bufferIdx, const std::span<DrawablePassVertexBuffers> vertexBufferOverrides) const
{
    ID3D12GraphicsCommandList* dxCmdList = cmdList.Get();
    const auto& shaderPass = drawablePass.mMaterialPass.GetShaderPass();

    // Set the pipeline for this material
    dxCmdList->SetPipelineState( drawablePass.mPipeline.GetPipeline() );

    mGfxApi.SetDescriptorHeaps(dxCmdList);

    dxCmdList->SetGraphicsRootSignature( shaderPass.GetPipelineLayout().GetRootSignature() );

    // Bind everything the shader needs
    {
        // Set the root parameters!
        const auto& rootDescriptorTable = drawablePass.mDescriptorTables.begin();
        const auto& rootLayout = shaderPass.GetDescriptorSetLayouts()[0].GetLayout<true>();
        uint32_t descriptorTableIdx = 0;  // root is table 0
        for (size_t i = 0; i < rootLayout.root.size(); ++i)
        {
                switch (rootLayout.root[i].ParameterType) {
                case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                    dxCmdList->SetGraphicsRootDescriptorTable(i, drawablePass.mDescriptorTables[++descriptorTableIdx].GetGpuHandle());
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                    dxCmdList->SetGraphicsRoot32BitConstants(i, 0, nullptr, 0);
                    assert(0 && "unhandled root parameter");
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_CBV:
                    dxCmdList->SetGraphicsRootConstantBufferView( i, drawablePass.mRootItems[i].gpuAddress );
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_SRV:
                    dxCmdList->SetGraphicsRootShaderResourceView(i, 0);
                    assert(0 && "unhandled root parameter");
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_UAV:
                    dxCmdList->SetGraphicsRootUnorderedAccessView(i, 0);
                    assert(0 && "unhandled root parameter");
                    break;
                }
        }
    }

    const auto& vertexBuffers = vertexBufferOverrides.empty() ? drawablePass.mVertexBuffers : vertexBufferOverrides[bufferIdx % vertexBufferOverrides.size()];

    if (!vertexBuffers.views.empty())
    {
        // Bind mesh vertex/instance buffer(s)
        dxCmdList->IASetVertexBuffers( 0, (UINT) vertexBuffers.views.size(), vertexBuffers.views.data() );
        dxCmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    }
    if (!vertexBufferOverrides.empty())
    {
        assert(vertexBufferOverrides.empty() || vertexBuffers.views.size() == drawablePass.mVertexBuffers.views.size());
    }

    if (drawablePass.mIndexBuffer.Format != DXGI_FORMAT_UNKNOWN)
    {
        // Bind index buffer data
        dxCmdList->IASetIndexBuffer(&drawablePass.mIndexBuffer);

//        if (drawablePass.mDrawIndirectBuffer != VK_NULL_HANDLE)
//        {
//            if (drawablePass.mDrawIndirectCountBuffer != VK_NULL_HANDLE)
//            {
//                // Draw the mesh using draw indirect cont buffer (VkDrawIndexedIndirectCount command)
//                const auto* drawIndirectCountExt = mVulkan.GetExtension<ExtensionHelper::Ext_VK_KHR_draw_indirect_count>();
//                assert( drawIndirectCountExt != nullptr && drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR != nullptr);
//                drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR(cmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mDrawIndirectCountBuffer, 0, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndexedIndirectCommand));
//                assert(0);//not yet implemented
//            }
//            else
//                // Draw the mesh using draw indirect buffer (VkDrawIndexedIndirectCommand)
//                vkCmdDrawIndexedIndirect(cmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndexedIndirectCommand));
//        }
//        else
        {
            // Everything is set up, draw the mesh
            dxCmdList->DrawIndexedInstanced(drawablePass.mNumIndices, GetInstances() ? (uint32_t)GetInstances()->GetNumVertices() : 1, 0, 0, 0);
        }
    }
    else
    {
//        if (drawablePass.mDrawIndirectBuffer != VK_NULL_HANDLE)
//        {
//            if (drawablePass.mDrawIndirectCountBuffer != VK_NULL_HANDLE)
//            {
//                // Draw the mesh using draw indirect buffer (VkDrawIndirectCommand - no index buffer)
//                const auto* drawIndirectCountExt = mVulkan.GetExtension<ExtensionHelper::Ext_VK_KHR_draw_indirect_count>();
//                assert( drawIndirectCountExt != nullptr && drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR != nullptr );
//                drawIndirectCountExt->m_vkCmdDrawIndexedIndirectCountKHR(cmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mDrawIndirectCountBuffer, 0, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndirectCommand));
//            }
//            else
//                // Draw the mesh using draw indirect buffer (VkDrawIndirectCommand - no index buffer)
//                vkCmdDrawIndirect(cmdBuffer, drawablePass.mDrawIndirectBuffer, drawablePass.mDrawIndirectOffset, drawablePass.mNumDrawIndirect, sizeof(VkDrawIndirectCommand));
//        }
//        else
        {
            // Draw the mesh without index buffer
            dxCmdList->DrawInstanced(drawablePass.mNumVertices, GetInstances() ? (uint32_t)GetInstances()->GetNumVertices() : 1, 0, 0);
        }
    }
}

