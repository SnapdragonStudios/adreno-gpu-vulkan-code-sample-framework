//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "materialManager.hpp"
#include "descriptorSetLayout.hpp"
#include "vulkan/material.hpp"
#include "shader.hpp"
#include "shaderDescription.hpp"
#include "system/os_common.h"
#include "vulkan/vulkan.hpp"
#include "texture/vulkan/texture.hpp"
//#include "material/vulkan/specializationConstantsLayout.hpp"

#include <glm/glm.hpp>
static_assert(GLM_HAS_CONSTEXPR);

template<>
MaterialManagerT<Vulkan>::MaterialManagerT()
{}

template<>
MaterialManagerT<Vulkan>::~MaterialManagerT()
{}

template<>
MaterialPass MaterialManagerT<Vulkan>::CreateMaterialPassInternal(
    Vulkan& vulkan,
    const ShaderPass<Vulkan>& shaderPass,
    uint32_t numFrameBuffers,
    const std::function<const MaterialPass::tPerFrameTexInfo(const std::string&)>& textureLoader,
    const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
    const std::function<const ImageInfo(const std::string&)>& imageLoader,
    const std::function<const MaterialPass::tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader,
    const std::function<const VertexElementData(const std::string&)>& specializationStructureLoader,
    const std::string& passDebugName) const
{
    const std::vector<DescriptorSetLayout>& descriptorSetLayouts = shaderPass.GetDescriptorSetLayouts();

    // Copy the Vulkan descriptor set layout handles from our descriptor set layout class.
    // There may be 'null' layouts in here - denoting descriptor set layouts with 'dynamic' descriptorCount that needs to be calculated from the textureLoader functions.
    // We will fill in any blank entries.
    std::vector<VkDescriptorSetLayout> vkDescSetLayouts;
    vkDescSetLayouts.reserve(descriptorSetLayouts.size());
    for (const auto& setLayout : descriptorSetLayouts)
        vkDescSetLayouts.push_back(setLayout.GetVkDescriptorSetLayout());
    // Storage for the dynamic descriptor set layouts
    std::vector<VkDescriptorSetLayout> dynamicVkDescriptorSetLayouts;

    std::vector<VkDescriptorPoolSize> poolSizes;

    //
    // Gather the (named) texture and uniform buffer slots (for this material pass)
    //
    MaterialPass::tTextureBindings textureBindings;
    MaterialPass::tImageBindings imageBindings;
    MaterialPass::tBufferBindings bufferBindings;
    MaterialPass::tAccelerationStructureBindings accelerationStructureBindings;

    for (size_t layoutIdx = 0; layoutIdx < descriptorSetLayouts.size(); ++layoutIdx)
    {
        const auto& descSetLayout = descriptorSetLayouts[layoutIdx];
        VkDescriptorSetLayout& vkDescSetLayout = vkDescSetLayouts[layoutIdx];

        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        if (vkDescSetLayout == VK_NULL_HANDLE)
        {
            // We need to copy in the binding data so we can fill in the gaps!
            vkBindings = descSetLayout.GetVkDescriptorSetLayoutBinding();
        }

        for (const auto& bindingNames : descSetLayout.GetNameToBinding())
        {
            uint32_t descriptorCount = 0;
            const std::string& bindingName = bindingNames.first;
            const MaterialPass::MaterialDescriptorBinding binding{uint32_t(layoutIdx)/*set index*/, bindingNames.second};
            switch (binding.setBinding.type)
            {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
                MaterialPass::tPerFrameTexInfo pTextures = textureLoader(bindingName);	// Get the texture(s) and/or samplers from the callback
                descriptorCount = (uint32_t) pTextures.size();
                textureBindings.push_back({ std::move(pTextures), binding });
                break;
            }
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                if (imageLoader)
                {
                    const ImageInfo imageInfo(imageLoader(bindingName));
                    descriptorCount = 1;
                    imageBindings.push_back({ {imageInfo}, binding });
                }
                else
                {
                    // Fallback to using the texture callback
                    MaterialPass::tPerFrameTexInfo pTextures = textureLoader(bindingName);
                    std::vector<ImageInfo> imageInfos;
                    descriptorCount = (uint32_t)pTextures.size();
                    imageInfos.reserve(descriptorCount);
                    for (const auto* pTexture : pTextures)
                        imageInfos.push_back(ImageInfo(*pTexture));
                    imageBindings.push_back({ std::move(imageInfos), binding });
                }
                break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                tPerFrameVkBuffer vkBuffers = bufferLoader(bindingName);	// Get the buffer(s) from the callback
                descriptorCount = (uint32_t) vkBuffers.size();
                bufferBindings.push_back({ std::move(vkBuffers), binding });
                break;
            }
#if VK_KHR_acceleration_structure
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif // defined(__clang__)
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            {
                assert(accelerationStructureLoader && "Needs accelerationStructureLoader function defining");
                MaterialPass::tPerFrameVkAccelerationStructure vkAS = accelerationStructureLoader(bindingName);	// Get the buffer(s) from the callback
                accelerationStructureBindings.push_back({ std::move(vkAS), binding });
                break;
            }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif // defined(__clang__)
#endif // VK_KHR_acceleration_structure
            default:
                assert(0);	// needs implementing
                break;
            }

            // Fill in descriptor count if 'dynamically sized'
            if (!vkBindings.empty() && vkBindings[binding.setBinding.index].descriptorCount == 0)
            {
                vkBindings[binding.setBinding.index].descriptorCount = descriptorCount;
            }
        }

        if (vkDescSetLayout == VK_NULL_HANDLE)
        {
            // Create the descriptor set layout that is unique to this descriptor set (ideally we share the layouts but in the case of 'dynamic' descriptorCounts we cannot)
            vkDescSetLayout = dynamicVkDescriptorSetLayouts.emplace_back(DescriptorSetLayout::CreateVkDescriptorSetLayout(vulkan, vkBindings));

            // Add counts directly into pool sizes array!
            DescriptorSetLayout::CalculatePoolSizes(vkBindings, poolSizes);
        }
    }

    //
    // Add in the pool sizes for non 'dynamic' descriptor set layouts
    //
    for (const auto& layout : descriptorSetLayouts)
    {
        if (poolSizes.size() == 0)
            poolSizes = layout.GetDescriptorPoolSizes();
        else
        {
            ///TODO: n^2 search, 'could' be slowish - although not normal to have more than one or two sets
            for (const auto& newPoolSize : layout.GetDescriptorPoolSizes())
            {
                bool found = false;
                for (auto& poolSize : poolSizes)
                {
                    if (poolSize.type == newPoolSize.type)
                    {
                        poolSize.descriptorCount += newPoolSize.descriptorCount;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    poolSizes.push_back(newPoolSize);
            }
        }
    }

    uint32_t poolSizeTotal = 0;
    for (auto& poolSize : poolSizes)
    {
        poolSize.descriptorCount *= numFrameBuffers;
        poolSizeTotal += poolSize.descriptorCount;
    }

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    //if (poolSizeTotal > 0)  ok to make a pool with size zero (expected if a shader has a descriptor set with no entries)
    {
        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        //PoolInfo.flags = 0;     // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to allow them to be returned
        poolInfo.maxSets = numFrameBuffers * descriptorSetLayouts.size();  // Descriptor sets also come out of this pool
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        if (VK_SUCCESS != vkCreateDescriptorPool(vulkan.m_VulkanDevice, &poolInfo, NULL, &descriptorPool))
        {
            assert(0);
        }
    }

    //
    // Create the descriptor sets to be populated
    //

    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.reserve(numFrameBuffers * descriptorSetLayouts.size());	// pre-size so memory does not move in emplace_back

    if (!descriptorSetLayouts.empty())
    {
        for (uint32_t whichBuffer = 0; whichBuffer < numFrameBuffers; ++whichBuffer)
        {
            //
            // Create the descriptor set
            //
            VkDescriptorSetAllocateInfo descSetInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            descSetInfo.descriptorPool = descriptorPool;
            descSetInfo.descriptorSetCount = (uint32_t) vkDescSetLayouts.size();
            descSetInfo.pSetLayouts = vkDescSetLayouts.data();
            descriptorSets.resize( descriptorSets.size() + descSetInfo.descriptorSetCount, VK_NULL_HANDLE );
            VkDescriptorSet* pNewDescriptorSets = &descriptorSets[descriptorSets.size() - descSetInfo.descriptorSetCount];
            VkResult result = vkAllocateDescriptorSets( vulkan.m_VulkanDevice, &descSetInfo, pNewDescriptorSets );
            if (result != VK_SUCCESS)
            {
                assert(0);
            }

            for (const VkDescriptorSet* pDescriptorSet = pNewDescriptorSets; pDescriptorSet <= &descriptorSets.back(); ++pDescriptorSet)
            {
                vulkan.SetDebugObjectName( *pDescriptorSet, passDebugName.c_str() );
                ++pNewDescriptorSets;
            }
        }
    }

    // Create the specialization constants for this material pass
    const auto& shaderPassConstantDescriptions = shaderPass.m_shaderPassDescription.m_constants;
    std::vector<VertexElementData> shaderConstantDatas;
    shaderConstantDatas.reserve( shaderPassConstantDescriptions.size() );
    for (const auto& constantDescription : shaderPassConstantDescriptions)
        shaderConstantDatas.push_back( specializationStructureLoader( constantDescription.name ) );

    SpecializationConstants specializationConstants;
    specializationConstants.Init( shaderPass.GetSpecializationConstantsLayout(), { shaderConstantDatas } );

    return MaterialPass(vulkan, shaderPass, std::move(descriptorPool), std::move(descriptorSets), std::move(dynamicVkDescriptorSetLayouts), std::move(textureBindings), std::move(imageBindings), std::move(bufferBindings), std::move(accelerationStructureBindings), std::move(specializationConstants));
}

template<>
Material MaterialManagerT<Vulkan>::CreateMaterial(Vulkan& vulkan, const ShaderT<Vulkan>& shader, uint32_t numFrameBuffers,
                                          const std::function<const MaterialManagerT<Vulkan>::tPerFrameTexInfo(const std::string&)>& textureLoader,
                                          const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
                                          const std::function<const ImageInfo(const std::string&)>& imageLoader,
                                          const std::function<const MaterialManagerT<Vulkan>::tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader,
                                          const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader) const
{
    Material material( shader, numFrameBuffers );

    // We do each pass seperately.
    const ShaderDescription* pShaderDescription = shader.m_shaderDescription;

    // iterate over the passes (in their index order - iterating std::map gaurantees to iterate in key order)
    const auto& shaderPasses = shader.GetShaderPasses();
    for (uint32_t passIdx = 0; const auto& passName : shader.GetShaderPassIndicesToNames())
    {
        const auto& shaderPass = shaderPasses[passIdx];
        material.AddMaterialPass(passName, CreateMaterialPassInternal(vulkan, shaderPass, numFrameBuffers, textureLoader, bufferLoader, imageLoader, accelerationStructureLoader, specializationConstantLoader, passName));
        ++passIdx;
    }

    for (uint32_t whichBuffer = 0; whichBuffer < numFrameBuffers; ++whichBuffer)
    {
        material.UpdateDescriptorSets(whichBuffer);
    }

    return material;
}

template<>
Material MaterialManagerT<Vulkan>::CreateMaterial(GraphicsApiBase& gfxApi, const Shader& shader, uint32_t numFrameBuffers,
                        const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
                        const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
                        const std::function<const ImageInfo(const std::string&)>& imageLoader,
                        const std::function<const tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader,
                        const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader) const /*override*/
{
    return CreateMaterial(static_cast<Vulkan&>(gfxApi), static_cast<const ShaderT<Vulkan>&>(shader), numFrameBuffers,
                          textureLoader, bufferLoader, imageLoader, accelerationStructureLoader, specializationConstantLoader);
}
