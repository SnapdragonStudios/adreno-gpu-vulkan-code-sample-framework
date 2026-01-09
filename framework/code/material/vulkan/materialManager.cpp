//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "materialManager.hpp"
#include "descriptorSetLayout.hpp"
#include "material.hpp"
#include "shader.hpp"
#include "../shaderDescription.hpp"
#include "system/os_common.h"
#include "vulkan/vulkan.hpp"
#include "texture/vulkan/texture.hpp"

#include <glm/glm.hpp>
static_assert(GLM_HAS_CONSTEXPR);

template<>
MaterialManager<Vulkan>::MaterialManager(Vulkan& gfxApi) noexcept
    : MaterialManagerBase(gfxApi)
{}

template<>
MaterialManager<Vulkan>::~MaterialManager()
{}

template<>
MaterialPass<Vulkan> MaterialManager<Vulkan>::CreateMaterialPassInternal(
    const ShaderPass<Vulkan>& shaderPass,
    uint32_t numFrameBuffers,
    const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
    const std::function<const PerFrameBufferVulkan(const std::string&)>& bufferLoader,
    const std::function<const ImageInfo<Vulkan>(const std::string&)>& imageLoader,
    const std::function<const tPerFrameAccelerationStructure(const std::string&)>& accelerationStructureLoader,
    const std::function<const VertexElementData(const std::string&)>& specializationStructureLoader,
    const std::string& passDebugName) const
{
    auto& vulkan = static_cast<Vulkan&>(mGfxApi);
    const std::vector<DescriptorSetLayout<Vulkan>>& descriptorSetLayouts = shaderPass.GetDescriptorSetLayouts();

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
    MaterialPass<Vulkan>::tTextureBindings textureBindings;
    MaterialPass<Vulkan>::tImageBindings imageBindings;
    MaterialPass<Vulkan>::tBufferBindings bufferBindings;
    MaterialPass<Vulkan>::tAccelerationStructureBindings accelerationStructureBindings;

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
            const DescriptorSetLayoutBase::DescriptorBinding binding{uint32_t(layoutIdx)/*set index*/, bindingNames.second};
            using DescriptorType = DescriptorSetDescription::DescriptorType;
            switch (binding.setBinding.type)
            {
            case DescriptorType::ImageSampler:
            case DescriptorType::ImageSampled:
            case DescriptorType::Sampler:
            {
                MaterialManagerBase::tPerFrameTexInfo pTextures = textureLoader(bindingName);	// Get the texture(s) and/or samplers from the callback
                descriptorCount = (uint32_t) pTextures.size();
                textureBindings.push_back({ std::move(pTextures), binding });
                break;
            }
            case DescriptorType::InputAttachment:
            case DescriptorType::ImageStorage:
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
                    MaterialManagerBase::tPerFrameTexInfo pTextures = textureLoader(bindingName);
                    std::vector<ImageInfo<Vulkan>> imageInfos;
                    descriptorCount = (uint32_t)pTextures.size();
                    imageInfos.reserve(descriptorCount);
                    for (const auto* pTexture : pTextures)
                        imageInfos.push_back( ImageInfo<Vulkan>(*pTexture));
                    imageBindings.push_back({ std::move(imageInfos), binding });
                }
                break;
            }

            case DescriptorType::UniformBuffer:
            case DescriptorType::StorageBuffer:
            {
                PerFrameBufferVulkan vkBuffers = bufferLoader(bindingName);	// Get the buffer(s) from the callback
                descriptorCount = (uint32_t) vkBuffers.size();
                bufferBindings.push_back({ std::move(vkBuffers), binding });
                break;
            }
#if VK_KHR_acceleration_structure
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif // defined(__clang__)
            case DescriptorType::AccelerationStructure:
            {
                assert(accelerationStructureLoader && "Needs accelerationStructureLoader function defining");
                tPerFrameAccelerationStructure vkAS = accelerationStructureLoader(bindingName);	// Get the buffer(s) from the callback
                accelerationStructureBindings.push_back({ std::move(vkAS), binding });
                break;
            }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif // defined(__clang__)
#endif // VK_KHR_acceleration_structure
            case DescriptorType::Unused:
                break;
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
            vkDescSetLayout = dynamicVkDescriptorSetLayouts.emplace_back(DescriptorSetLayout<Vulkan>::CreateVkDescriptorSetLayout(vulkan, vkBindings));

            // Add counts directly into pool sizes array!
            DescriptorSetLayout<Vulkan>::CalculatePoolSizes(vkBindings, poolSizes);
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

    SpecializationConstants<Vulkan> specializationConstants;
    specializationConstants.Init( shaderPass.GetSpecializationConstantsLayout(), { shaderConstantDatas } );

    return MaterialPass<Vulkan>(vulkan, shaderPass, std::move(descriptorPool), std::move(descriptorSets), std::move(dynamicVkDescriptorSetLayouts), std::move(textureBindings), std::move(imageBindings), std::move(bufferBindings), std::move(accelerationStructureBindings), std::move(specializationConstants));
}

template<>
Material<Vulkan> MaterialManager<Vulkan>::CreateMaterial(const Shader<Vulkan>& shader, uint32_t numFrameBuffers,
                                          const std::function<const MaterialManagerBase::tPerFrameTexInfo(const std::string&)>& textureLoader,
                                          const std::function<const PerFrameBuffer<Vulkan>(const std::string&)>& bufferLoader,
                                          const std::function<const ImageInfo<Vulkan>(const std::string&)>& imageLoader,
                                          const std::function<const MaterialManagerBase::tPerFrameAccelerationStructure(const std::string&)>& accelerationStructureLoader,
                                          const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader) const
{
    auto& vulkan = static_cast<Vulkan&>(mGfxApi);
    Material<Vulkan> material( shader, numFrameBuffers );

    // We do each pass seperately.
    const ShaderDescription* pShaderDescription = shader.m_shaderDescription;

    // iterate over the passes (in their index order - iterating std::map gaurantees to iterate in key order)
    const auto& shaderPasses = shader.GetShaderPasses();
    for (uint32_t passIdx = 0; const auto& passName : shader.GetShaderPassIndicesToNames())
    {
        const auto& shaderPass = shaderPasses[passIdx];
        material.AddMaterialPass(passName, CreateMaterialPassInternal(shaderPass, numFrameBuffers, textureLoader, bufferLoader, imageLoader, accelerationStructureLoader, specializationConstantLoader, passName));
        ++passIdx;
    }

    for (uint32_t whichBuffer = 0; whichBuffer < numFrameBuffers; ++whichBuffer)
    {
        material.UpdateDescriptorSets(whichBuffer);
    }

    return material;
}

//template<>
//MaterialBase MaterialManager<Vulkan>::CreateMaterial(GraphicsApiBase& gfxApi, const Shader& shader, uint32_t numFrameBuffers,
//                        const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
//                        const std::function<const PerFrameBufferBase(const std::string&)>& bufferLoader,
//                        const std::function<const ImageInfo(const std::string&)>& imageLoader,
//                        const std::function<const tPerFrameAccelerationStructure(const std::string&)>& accelerationStructureLoader,
//                        const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader) const /*override*/
//{
//    return CreateMaterial(static_cast<Vulkan&>(gfxApi),
//                          static_cast<const Shader<Vulkan>&>(shader),
//                          numFrameBuffers,
//                          textureLoader,
//                          bufferLoader,
//                          imageLoader,
//                          accelerationStructureLoader,
//                          specializationConstantLoader);
//}
