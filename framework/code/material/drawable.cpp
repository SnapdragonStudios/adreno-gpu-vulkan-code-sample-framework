// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "drawable.hpp"
#include "material.hpp"
#include "shader.hpp"
#include "shaderDescription.hpp"
#include "shaderModule.hpp"
#include "system/os_common.h"
#include "mesh/instanceGenerator.hpp"
#include <cassert>
#include <utility>

DrawablePass::~DrawablePass()
{
    assert(mPipeline == VK_NULL_HANDLE);
}

const DrawablePass* Drawable::GetDrawablePass(const std::string& passName) const
{
    auto it = mPassNameToIndex.find(passName);
    if (it != mPassNameToIndex.end())
    {
        return &mPasses[it->second];
    }
    return nullptr;
}


Drawable::Drawable(Vulkan& vulkan, Material&& material)
    : mVulkan(vulkan)
    , mMaterial(std::move(material))
{
}

Drawable::Drawable(Drawable&& other) noexcept
    : mVulkan(other.mVulkan)
    , mMaterial(std::move(other.mMaterial))
    , mMeshObject(std::move(other.mMeshObject))
    , mPasses(std::move(other.mPasses))
    , mPassNameToIndex(std::move(other.mPassNameToIndex))
    , mPassMask(other.mPassMask)
    , mVertexInstanceBuffer( std::move(other.mVertexInstanceBuffer))
{
    other.mPassMask = 0;
}

Drawable::~Drawable()
{
    for (auto& pass : mPasses)
    {
        vkDestroyPipeline(mVulkan.m_VulkanDevice, pass.mPipeline, nullptr);
        pass.mPipeline = VK_NULL_HANDLE;
    }
}

static VkBlendFactor BlendFactorToVk( ShaderPassDescription::BlendFactor bf)
{
    switch( bf )
    {
    case ShaderPassDescription::BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case ShaderPassDescription::BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case ShaderPassDescription::BlendFactor::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case ShaderPassDescription::BlendFactor::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case ShaderPassDescription::BlendFactor::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case ShaderPassDescription::BlendFactor::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    }
    assert( 0 );
    return VK_BLEND_FACTOR_ZERO;
}

bool Drawable::Init(VkRenderPass vkRenderPass, const char* passName, MeshObject meshObject, std::optional<VertexBufferObject> vertexInstanceBuffer, const VkSampleCountFlagBits* const passMultisample, const uint32_t* const subpasses)
{
    mMeshObject = std::move(meshObject);
    mVertexInstanceBuffer = std::move(vertexInstanceBuffer);
    return ReInit( vkRenderPass, passName, passMultisample, subpasses );
}

bool Drawable::Init(tcb::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, MeshObject meshObject, std::optional<VertexBufferObject> vertexInstanceBuffer, tcb::span<const VkSampleCountFlagBits> passMultisample, tcb::span<const uint32_t> subpasses)
{
    mMeshObject = std::move(meshObject);
    mVertexInstanceBuffer = std::move(vertexInstanceBuffer);
    return ReInit(vkRenderPasses, passNames, passMask, passMultisample, subpasses);
}

bool Drawable::ReInit( VkRenderPass vkRenderPass, const char* passName, const VkSampleCountFlagBits* const passMultisample, const uint32_t* const subpasses )
{
    auto multisampleSpan = passMultisample ? tcb::span<const VkSampleCountFlagBits>(passMultisample, 1) : tcb::span<const VkSampleCountFlagBits>{};
    auto subpassesSpan = subpasses ? tcb::span<const uint32_t>( subpasses, 1 ) : tcb::span<const uint32_t>{};

    return ReInit( { &vkRenderPass,1 }, &passName, 1, multisampleSpan, subpassesSpan );
}

bool Drawable::ReInit( tcb::span<VkRenderPass> vkRenderPasses, const char* const* passNames, uint32_t passMask, tcb::span<const VkSampleCountFlagBits> passMultisample, tcb::span<const uint32_t> subpasses )
{
    assert( passMultisample.empty() || passMultisample.size() == vkRenderPasses.size() );
    mPassMask = passMask;
    mPassNameToIndex.clear();
    for (auto& pass : mPasses)
    {
        vkDestroyPipeline(mVulkan.m_VulkanDevice, pass.mPipeline, nullptr);
        pass.mPipeline = VK_NULL_HANDLE;
    }
    mPasses.clear();

    const Shader& shader = mMaterial.m_shader;
    mPasses.reserve(shader.GetShaderPasses().size());
    for (uint32_t passIdx = 0; passIdx < sizeof(passMask) * 8; ++passIdx)
    {
        if (passMask & (1 << passIdx))
        {
            LOGI("Creating Mesh Object PipelineState and Pipeline for pass... %s", passNames[passIdx]);
            const MaterialPass* pMaterialPass = mMaterial.GetMaterialPass(passNames[passIdx]);
            if (!pMaterialPass)
            {
                LOGE("  Pass does not exist in shader");
                continue;
            }
            assert(pMaterialPass);
            const ShaderPass& shaderPass = pMaterialPass->mShaderPass;

            // Common to all pipelines
            // State for rasterization, such as polygon fill mode is defined.
            VkPipelineRasterizationStateCreateInfo rs {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            const auto& fixedFunctionSettings = shaderPass.m_shaderPassDescription.m_fixedFunctionSettings;
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.cullMode = (fixedFunctionSettings.cullBackFace?VK_CULL_MODE_BACK_BIT:0) | (fixedFunctionSettings.cullFrontFace ? VK_CULL_MODE_FRONT_BIT : 0);
            rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rs.depthClampEnable = VK_FALSE;
            rs.rasterizerDiscardEnable = VK_FALSE;
            rs.depthBiasEnable = fixedFunctionSettings.depthBiasEnable ? VK_TRUE : VK_FALSE;
            if (fixedFunctionSettings.depthBiasEnable)
            {
                rs.depthBiasConstantFactor = fixedFunctionSettings.depthBiasConstant;
                rs.depthBiasClamp = fixedFunctionSettings.depthBiasClamp;
                rs.depthBiasSlopeFactor = fixedFunctionSettings.depthBiasSlope;
            }
            rs.lineWidth = 1.0f;

            const auto& outputSettings = shaderPass.m_shaderPassDescription.m_outputs;

            // Setup blending/transparency
            std::vector<VkPipelineColorBlendAttachmentState> BlendStates;
            BlendStates.reserve(outputSettings.size());
            for (const auto& outputSetting : outputSettings)
            {
                VkPipelineColorBlendAttachmentState& cb = BlendStates.emplace_back(VkPipelineColorBlendAttachmentState{});
                if (outputSetting.blendEnable)
                {
                    cb.blendEnable = VK_TRUE;
                    cb.srcColorBlendFactor = BlendFactorToVk(outputSetting.srcColorBlendFactor);
                    cb.dstColorBlendFactor = BlendFactorToVk(outputSetting.dstColorBlendFactor);
                    cb.colorBlendOp = VK_BLEND_OP_ADD;
                    cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                    cb.alphaBlendOp = VK_BLEND_OP_ADD;
                }
                cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            }

            VkPipelineColorBlendStateCreateInfo cb = {};
            cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            cb.attachmentCount = (uint32_t)BlendStates.size();
            cb.pAttachments = BlendStates.data();

            // Setup depth testing
            VkPipelineDepthStencilStateCreateInfo ds = {};
            ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            ds.depthTestEnable = fixedFunctionSettings.depthTestEnable ? VK_TRUE : VK_FALSE;
            ds.depthWriteEnable = fixedFunctionSettings.depthWriteEnable ? VK_TRUE : VK_FALSE;
            switch( fixedFunctionSettings.depthCompareOp ) {
            case ShaderPassDescription::DepthCompareOp::LessEqual:
                ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                break;
            case ShaderPassDescription::DepthCompareOp::Equal:
                ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                break;
            case ShaderPassDescription::DepthCompareOp::Greater:
                ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                break;
            }
            ds.depthBoundsTestEnable = VK_FALSE;
            ds.back.failOp = VK_STENCIL_OP_KEEP;
            ds.back.passOp = VK_STENCIL_OP_KEEP;
            ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
            ds.stencilTestEnable = VK_FALSE;
            ds.front = ds.back;

            // Setup (multi) sampling
            VkSampleMask sampleMask = 0;
            VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

            ms.rasterizationSamples = passMultisample.empty() ? VK_SAMPLE_COUNT_1_BIT : passMultisample[passIdx];
            const auto& sampleShadingSettings = shaderPass.m_shaderPassDescription.m_sampleShadingSettings;
            ms.sampleShadingEnable = sampleShadingSettings.sampleShadingEnable;
            if (sampleShadingSettings.sampleShadingMask != 0)
            {
                assert(ms.rasterizationSamples <= VK_SAMPLE_COUNT_32_BIT ); // sampleMask is only 32bits currently! Easy fix if we want > 32x MSAA
                sampleMask = sampleShadingSettings.sampleShadingMask & ((1 << ms.rasterizationSamples) -1);
                ms.pSampleMask = &sampleMask;
            }

            VkPipelineSampleLocationsStateCreateInfoEXT msLocations = { VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT };
            if( sampleShadingSettings.forceCenterSample )
            {
                msLocations.sampleLocationsEnable = VK_TRUE;
                msLocations.sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
                msLocations.sampleLocationsInfo.sampleLocationsPerPixel = ms.rasterizationSamples;
                msLocations.sampleLocationsInfo.sampleLocationsCount = (uint32_t) ms.rasterizationSamples;
                msLocations.sampleLocationsInfo.sampleLocationGridSize = { 1,1 };
                std::vector<VkSampleLocationEXT> msSampleLocations( msLocations.sampleLocationsInfo.sampleLocationsCount, { 0.5f,0.5f } );
                msLocations.sampleLocationsInfo.pSampleLocations = msSampleLocations.data();
                ms.pNext = &msLocations;
            }

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

                    // Double check the mesh is supplying the data we expect in the shader, may mismatch if the mesh was not built using the vertex format (eg MeshObject::CreateScreenSpaceMesh)
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
                indexBufferType = mMeshObject.m_IndexBuffer->GetIndexType();
                indexCount = mMeshObject.m_IndexBuffer->GetNumIndices();
            }

            // add the DrawablePass
            DrawablePass& pass = mPasses.emplace_back(DrawablePass{ *pMaterialPass,
                                                                    VK_NULL_HANDLE,
                                                                    shaderPass.GetPipelineLayout().GetVkPipelineLayout(),
                                                                    pMaterialPass->GetVkDescriptorSets(),
                                                                    shaderPass.GetPipelineVertexInputState(),
                                                                    //std::move(passVertexBufferLookup),
                                                                    std::move(passVertexBuffers),
                                                                    std::move(passVertexBufferOffsets),
                                                                    indexBuffer,
                                                                    indexBufferType,
                                                                    (uint32_t)mMeshObject.m_NumVertices,
                                                                    (uint32_t)indexCount,
                                                                    passIdx
                                                      });
            VkShaderModule vkVertShader = VK_NULL_HANDLE;
            VkShaderModule vkFragShader = VK_NULL_HANDLE;

            std::visit( [&](auto& m)
            {
                using T = std::decay_t<decltype(m)>;
                if constexpr (std::is_same_v<T, GraphicsShaderModules>)
                {
                    vkVertShader = m.vert.GetVkShaderModule();
                    vkFragShader = m.frag.GetVkShaderModule();
                }
                else if constexpr (std::is_same_v<T, GraphicsShaderModuleVertOnly>)
                {
                    vkVertShader = m.vert.GetVkShaderModule();
                }
                else
                {
                    assert( 0 );    // unsupported shader module type (eg ComputeShaderModule)
                }
            }, shaderPass.m_shaders.m_modules );

            if (!mVulkan.CreatePipeline(VK_NULL_HANDLE,
                &pass.mPipelineVertexInputState.GetVkPipelineVertexInputStateCreateInfo(),
                pass.mPipelineLayout,
                vkRenderPasses[passIdx],
                subpasses.empty() ? 0 :subpasses[passIdx],
                &rs,
                &ds,
                BlendStates.empty() ? nullptr : &cb,
                &ms,
                {},//dynamic states
                nullptr, nullptr,
                vkVertShader,
                vkFragShader,
                false,
                VK_NULL_HANDLE,
                &pass.mPipeline))
            {
                // Error
                return false;
            }
        }
    }
    return true;
}

void Drawable::DrawPass(VkCommandBuffer cmdBuffer, const DrawablePass& drawablePass, uint32_t bufferIdx) const
{
    // Bind the pipeline for this material
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, drawablePass.mPipeline);

    // Bind everything the shader needs
    VkDescriptorSet vkDescriptorSet = drawablePass.mDescriptorSet.size() >= 1 ? drawablePass.mDescriptorSet[bufferIdx] : drawablePass.mDescriptorSet[0];
    vkCmdBindDescriptorSets(cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        drawablePass.mPipelineLayout,
        0,
        1,
        &vkDescriptorSet,
        0,
        NULL);

    // Bind our mesh vertex buffer
    vkCmdBindVertexBuffers(cmdBuffer,
        0,
        (uint32_t)drawablePass.mVertexBuffers.size(),
        drawablePass.mVertexBuffers.data(),
        drawablePass.mVertexBufferOffsets.data());

    if (drawablePass.mIndexBuffer != VK_NULL_HANDLE)
    {
        assert( drawablePass.mIndexBufferType != VK_INDEX_TYPE_MAX_ENUM );

        // Bind index buffer data
        vkCmdBindIndexBuffer(cmdBuffer,
            drawablePass.mIndexBuffer,
            0,
            drawablePass.mIndexBufferType);

        // Everything is set up, draw the mesh
        vkCmdDrawIndexed(cmdBuffer, drawablePass.mNumIndices, GetInstances() ? (uint32_t)GetInstances()->GetNumVertices() : 1, 0, 0, 0);
    }
    else
    {
        // Draw the mesh without index buffer
        vkCmdDraw(cmdBuffer, drawablePass.mNumVertices, GetInstances() ? (uint32_t)GetInstances()->GetNumVertices() : 1, 0, 0);
    }
}

bool DrawableLoader::LoadDrawables(Vulkan& vulkan, AssetManager& assetManager, tcb::span<VkRenderPass> vkRenderPasses, const char* const* renderPassNames, const std::string& meshFilename, const std::function<std::optional<Material> (const MeshObjectIntermediate::MaterialDef&)>& materialLoader, std::vector<Drawable>& drawables, tcb::span<const VkSampleCountFlagBits> renderPassMultisample, bool useInstancing, tcb::span<const uint32_t> renderPassSubpasses )
{
    LOGI("Loading Object mesh: %s...", meshFilename.c_str());

    std::vector<MeshObjectIntermediate> fatObjects;
    if (meshFilename.size() > 4 && meshFilename.substr(meshFilename.size() - 4) == std::string(".obj"))
    {
        // Load .obj file
        fatObjects = MeshObjectIntermediate::LoadObj(assetManager, meshFilename);
    }
    else
    {
        // Load .gltf file
        fatObjects = MeshObjectIntermediate::LoadGLTF(assetManager, meshFilename);
    }
    if (fatObjects.size() == 0)
    {
        LOGE("Error loading Object mesh: %s", meshFilename.c_str());
        return {};
    }

    // See if we can find instances, assuming we have no instance information in the gltf (ie Bistro scene)!
    auto instancedFatObjects = useInstancing ? MeshInstanceGenerator::FindInstances(std::move(fatObjects)) : MeshInstanceGenerator::NullFindInstances(std::move(fatObjects));
    fatObjects.clear();

    drawables.reserve(instancedFatObjects.size() );
    for (auto& [fatObject, instances] : instancedFatObjects)
    {
        // Get the material for this mesh
        std::optional<Material> material;
        if (fatObject.m_Materials.size() == 0)
        {
            // default material
            auto loadedMaterial = materialLoader( MeshObjectIntermediate::MaterialDef{ "textures/white_d.ktx" } );
            if (loadedMaterial.has_value())
                material.emplace( std::move(loadedMaterial.value()) );
        }
        else
        {
            if (fatObject.m_Materials.size() > 1)
            {
                LOGE("  Drawable loader does not support per-face materials, using first material");
            }
            auto loadedMaterial = materialLoader( fatObject.m_Materials[0] );
            if (loadedMaterial.has_value())
                material.emplace( std::move(loadedMaterial.value()) );
        }
        if (!material)
        {
            // It is valid for the material loader to return a null material - denotes we dont want to render this object!
            continue;
        }
        const Shader& shader = material->m_shader;

        uint32_t passMask = 0;  // max 32 passes!
        const ShaderPass* pFirstPass = nullptr;
        for (uint32_t pass = 0; pass < vkRenderPasses.size(); ++pass)
        {
            const char* pRenderPassName = renderPassNames[pass];
            const ShaderPass* pShaderPass = shader.GetShaderPass(pRenderPassName); ///TODO: std::string generated here!
            if (pShaderPass)
            {
                passMask |= 1 << pass;
            }
            if (!pFirstPass)
                pFirstPass = pShaderPass;
        }

        // If this drawable was in one or more passes (hopefully it was used for something) then initialize them
        if (pFirstPass)
        {
            ///TODO: implement having different bindings and packing for different passes

            MeshObject meshObject;
            const auto& vertexFormats = shader.m_shaderDescription->m_vertexFormats;
            MeshObject::CreateMesh(&vulkan, fatObject, (uint32_t)pFirstPass->m_shaderPassDescription.m_vertexFormatBindings[0], vertexFormats, &meshObject);

            // We are done with the FatObject here, Release it to save some memory earlier.
            fatObject.Release();

            // Potentially the vertex format has some Instance bindings in here (will have been ignored by the CreateMesh).
            std::optional<VertexBufferObject> vertexInstanceBuffer;
            const auto instanceFormatIt = std::find_if(vertexFormats.cbegin(), vertexFormats.cend(),
                [](const VertexFormat& f) { return f.inputRate == VertexFormat::eInputRate::Instance; });
            if (instanceFormatIt != vertexFormats.cend())
            {
                if (instanceFormatIt != vertexFormats.cbegin() && instanceFormatIt != vertexFormats.end()-1)
                {
                    LOGE("  Drawable loader (currently) only suports shaders with instance rate 'vertex' buffers at the beginning or end of their vertex layout");
                    return false;
                }
                // Create the instance data
                auto instancesSpan = tcb::make_span(instances);
                MeshInstance::tInstanceTransform identityTransform[1] { glm::identity<glm::mat3x4>() };
                if( instancesSpan.empty() )
                {
                    // If there are no 'instances' then the mesh occurs once and is at the origin (identity matrix).
                    // Since the shader assumes/requires an instance lets make an identity matrix.
                    instancesSpan = tcb::make_span( identityTransform );
                }
                assert(instanceFormatIt->span == sizeof(decltype(instancesSpan)::element_type)); ///TODO: be more flexible with what can go in the vertexInstanceBuffer
                if (!vertexInstanceBuffer.emplace().Initialize(&vulkan.GetMemoryManager(), instanceFormatIt->span, instancesSpan.size(), instancesSpan.data()))
                {
                    return false;
                }
            }
            else
            {
                if( !instances.empty() )
                {
                    LOGE("  Drawable loader found instances - expects shaders vertex layout to have instance data support");
                    return false;
                }
            }

            // Create the drawable
            if (!drawables.emplace_back(vulkan, std::move(material.value())).Init(vkRenderPasses, renderPassNames, passMask, std::move(meshObject), std::move(vertexInstanceBuffer), renderPassMultisample, renderPassSubpasses))
            {
                LOGE("Error initializing Drawable: %s", meshFilename.c_str());
            }
        }
    }
    return true;
}
