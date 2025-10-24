//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "material.hpp"
#include "materialManager.hpp"
#include "../shaderDescription.hpp"
#include "../specializationConstants.hpp"
#include "descriptorSetLayout.hpp"
#include "shader.hpp"
#include "system/os_common.h"
#include "dx12/dx12.hpp"
#include "texture/Dx12/texture.hpp"

#include <glm/glm.hpp>
static_assert(GLM_HAS_CONSTEXPR);


MaterialManager<Dx12>::MaterialManager( Dx12& dx12 ) noexcept : MaterialManagerBase(dx12)
{}

MaterialManager<Dx12>::~MaterialManager()
{}

MaterialPass<Dx12> MaterialManager<Dx12>::CreateMaterialPassInternal(
    const ShaderPass<Dx12>& shaderPass,
    uint32_t numFrameBuffers,
    const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
    const std::function<const PerFrameBuffer<Dx12>(const std::string&)>& bufferLoader,
    const std::function<const ImageInfo<Dx12>(const std::string&)>& imageLoader,
    const std::function<const tPerFrameAccelerationStructure(const std::string&)>& accelerationStructureLoader,
    const std::function<const VertexElementData(const std::string&)>& specializationStructureLoader,
    const std::string& passDebugName) const
{
    using ImageInfo = ImageInfo<Dx12>;
    auto& dx12 = static_cast<Dx12&>(mGfxApi);
    const std::vector<DescriptorSetLayout<Dx12>>& descriptorSetLayouts = shaderPass.GetDescriptorSetLayouts();

    //
    // Gather the (named) texture and uniform buffer slots (for this material pass)
    //
    MaterialPass<Dx12>::tTextureBindings textureBindings;
    MaterialPass<Dx12>::tImageBindings imageBindings;
    MaterialPass<Dx12>::tBufferBindings bufferBindings;
    MaterialPass<Dx12>::tAccelerationStructureBindings accelerationStructureBindings;

    for (uint32_t layoutIdx = 0; layoutIdx < (uint32_t) descriptorSetLayouts.size(); ++layoutIdx)
    {
        const auto& descSetLayout = descriptorSetLayouts[layoutIdx];
        std::vector<DescriptorBinding> dxBindings;

        for (const auto& bindingNames : descSetLayout.GetNameToBinding())
        {
            uint32_t descriptorCount = 0;
            const std::string& bindingName = bindingNames.first;
            const DescriptorSetLayoutBase::BindingTypeAndIndex& binding = bindingNames.second;
            using DescriptorType = DescriptorSetDescription::DescriptorType;
            switch (binding.type)
            {
            case DescriptorType::ImageSampler:
            case DescriptorType::ImageSampled:
            case DescriptorType::Sampler:
            {
                MaterialManagerBase::tPerFrameTexInfo pTextures = textureLoader(bindingName);	// Get the texture(s) and/or samplers from the callback
                descriptorCount = (uint32_t) pTextures.size();
                textureBindings.push_back({std::move(pTextures), {layoutIdx, binding}});
                break;
            }
            case DescriptorType::InputAttachment:
            case DescriptorType::ImageStorage:
            {
                if (imageLoader)
                {
                    const ImageInfo imageInfo(imageLoader(bindingName));
                    descriptorCount = 1;
                    imageBindings.push_back({{imageInfo}, {layoutIdx, binding}});
                }
                else
                {
                    // Fallback to using the texture callback
                    MaterialManagerBase::tPerFrameTexInfo pTextures = textureLoader(bindingName);
                    std::vector<ImageInfo> imageInfos;
                    descriptorCount = (uint32_t)pTextures.size();
                    imageInfos.reserve(descriptorCount);
                    for (const auto* pTexture : pTextures)
                        imageInfos.push_back(ImageInfo(*pTexture));
                    imageBindings.push_back({std::move(imageInfos), {layoutIdx, binding}});
                }
                break;
            }

            case DescriptorType::UniformBuffer:
            case DescriptorType::StorageBuffer:
            {
                PerFrameBuffer<Dx12> dxBuffers = bufferLoader(bindingName);	// Get the buffer(s) from the callback
                descriptorCount = (uint32_t) dxBuffers.size();
                bufferBindings.push_back({std::move(dxBuffers), {layoutIdx, binding}});
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
                MaterialPass<Dx12>::tPerFrameAccelerationStructure vkAS = accelerationStructureLoader(bindingName);	// Get the buffer(s) from the callback
                accelerationStructureBindings.push_back({ std::move(vkAS), binding });
                break;
            }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif // defined(__clang__)
#endif // VK_KHR_acceleration_structure
            case DescriptorType::DescriptorTable:
            {
                // Skip past this (Name points to one of the other descriptor sets in descriptorSetLayouts)
                break;
            }
            default:
                assert(0);	// needs implementing
                break;
            }

            // Fill in descriptor count if 'dynamically sized'
            if (!dxBindings.empty() && dxBindings[binding.index].count == 0)
            {
                dxBindings[binding.index].count = descriptorCount;
            }
        }
    }

    SpecializationConstants<Dx12> specializationConstants;
    return MaterialPass<Dx12>(dx12, shaderPass/*, std::move(descriptorPool), std::move(descriptorTables), std::move(dynamicVkDescriptorSetLayouts)*/,
        std::move(textureBindings), std::move(imageBindings), std::move(bufferBindings), std::move(accelerationStructureBindings), std::move(specializationConstants));
}

//Material<Dx12> MaterialManager<Dx12>::CreateMaterial(Dx12& dx12, const Shader<Dx12>& shader, uint32_t numFrameBuffers,
//                                          const std::function<const MaterialManager<Dx12>::tPerFrameTexInfo(const std::string&)>& textureLoader,
//                                          const std::function<const PerFrameBuffer<Dx12>(const std::string&)>& bufferLoader,
//                                          const std::function<const ImageInfo(const std::string&)>& imageLoader,
//                                          const std::function<const MaterialManager<Dx12>::tPerFrameAccelerationStructure(const std::string&)>& accelerationStructureLoader,
//                                          const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader) const

Material<Dx12> MaterialManager<Dx12>::CreateMaterial(   const Shader<Dx12>& shader,
                                                        uint32_t numFrameBuffers,
                                                        const std::function<const MaterialManager<Dx12>::tPerFrameTexInfo( const std::string& )>& textureLoader,
                                                        const std::function<const PerFrameBuffer<Dx12>( const std::string& )>& bufferLoader,
                                                        const std::function<const ImageInfo<Dx12>( const std::string& )>& imageLoader,
                                                        const std::function<const MaterialManager<Dx12>::tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader,
                                                        const std::function<const VertexElementData( const std::string& )>& specializationConstantLoader ) const
{
    Material<Dx12> material( shader, numFrameBuffers );

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
