//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "descriptorSetLayout.hpp"
#include "../descriptorSetDescription.hpp"
//#include "dx12/dx12.hpp"
#include <array>
#include <cassert>

static D3D12_SHADER_VISIBILITY EnumToDx12(DescriptorSetDescription::StageFlag::t f) {
    switch(f)
    {
        case DescriptorSetDescription::StageFlag::t::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case DescriptorSetDescription::StageFlag::t::Fragment:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        case DescriptorSetDescription::StageFlag::t::Geometry:
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case DescriptorSetDescription::StageFlag::t::Compute:
            return D3D12_SHADER_VISIBILITY_ALL;
        case DescriptorSetDescription::StageFlag::t::None:
            assert(0);
        default:
            // Mixing flags (or using any of the ray flags) just make the descriptor visible to all stages.
            // DirectX 12 has a less finely grained control over this stuff (compared to other graphics api).
            return D3D12_SHADER_VISIBILITY_ALL;
    }
    return D3D12_SHADER_VISIBILITY_ALL;
}

DescriptorSetLayout<Dx12>::DescriptorSetLayout() noexcept
    : DescriptorSetLayoutBase()
{
}

DescriptorSetLayout<Dx12>::~DescriptorSetLayout()
{
//    assert(m_descriptorSetLayout == VK_NULL_HANDLE);
}

void DescriptorSetLayout<Dx12>::Destroy(Dx12& dx12)
{
//    if (m_descriptorSetLayout != VK_NULL_HANDLE)
//    {
//        vkDestroyDescriptorSetLayout(dx12.m_VulkanDevice, m_descriptorSetLayout, nullptr);
//        m_descriptorSetLayout = VK_NULL_HANDLE;
//    }
//    m_descriptorSetLayoutBindings.clear();
//    m_descriptorPoolSizes.clear();
}

DescriptorSetLayout<Dx12>::DescriptorSetLayout(DescriptorSetLayout<Dx12>&& other) noexcept
    : DescriptorSetLayoutBase(std::move(other))
//    , m_descriptorSetLayoutBindings(std::move(other.m_descriptorSetLayoutBindings))
//    , m_descriptorPoolSizes(std::move(other.m_descriptorPoolSizes))
{
//    m_descriptorSetLayout = other.m_descriptorSetLayout;
//    other.m_descriptorSetLayout = VK_NULL_HANDLE;
}

bool DescriptorSetLayout<Dx12>::Init(Dx12& dx12, const DescriptorSetDescription& description)
{
    if (!DescriptorSetLayoutBase::Init(description))
        return false;

    const size_t numDescriptors = description.m_descriptorTypes.size();
    uint32_t shaderRegisterIndex = 0;
    if (description.m_setIndex == 0)
    {
        // Root signature
        auto& [rootParams, rootSamplers] = m_Layout.emplace<RootSignatureParameters>();

        rootParams.reserve(numDescriptors);
        rootSamplers.reserve(numDescriptors);
        uint32_t rootParamCount = 0;
        uint32_t rootSamplerCount = 0;
        uint32_t shaderRegisterIndex = 0;

        for (const auto& it : description.m_descriptorTypes)
        {
            bool validRootParam = false;
            bool validRootSampler = false;
            D3D12_ROOT_PARAMETER rootParam {};
            D3D12_STATIC_SAMPLER_DESC rootSampler {};

            if (it.descriptorIndex < 0)
            {
                // Index of < 0 denotes we want to use sequential parameter indices.
                ///TODO: look for collisions or determine how/if we want to handle out of order desciptor indices or enforce shaders that have an explicit binding index to define indices for all descriptors.
            }
            else
                assert(0);  ///TODO: implement user defined indices for DX12

            assert(it.count == 1);  // do we need to support a count > 1 (or <=0) for the root signature

            bool readOnly = it.readOnly;
            switch (it.type) {
                case DescriptorSetDescription::DescriptorType::ImageSampler:
                    assert(0);  //not supported in the root (currently)
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::ImageSampled:
                    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                    validRootParam = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::Sampler:
//                    assert(0);// Needs data setting into rootSampler
                    rootSampler = {};
                    validRootSampler = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::UniformBuffer:
                    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                    validRootParam = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::StorageBuffer:
                    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
                    validRootParam = true;
                    break;
                case DescriptorSetDescription::DescriptorType::ImageStorage:
                    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
                    validRootParam = true;
                    break;
                case DescriptorSetDescription::DescriptorType::InputAttachment:
                    assert(0);  ///TODO: implement DX12 equivalent to input attachements (if one exists with render passes)
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::AccelerationStructure:
                    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                    validRootParam = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::DrawIndirectBuffer:
                    assert(0 && "DrawIndirectBuffer is not a supported binding to a root signature");
                    break;
                case DescriptorSetDescription::DescriptorType::DescriptorTable:
                    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    validRootParam = true;
                    break;
            }

            if (validRootParam)
            {
                switch (rootParam.ParameterType) {
                    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                        rootParam.DescriptorTable = {}; // needs filling out when creating the root signature (in PipelineLayout<Dx12>::Init)
                        break;
                    case D3D12_ROOT_PARAMETER_TYPE_CBV:
                    case D3D12_ROOT_PARAMETER_TYPE_SRV:
                    case D3D12_ROOT_PARAMETER_TYPE_UAV:
                        rootParam.Descriptor = D3D12_ROOT_DESCRIPTOR {
                                               .ShaderRegister = shaderRegisterIndex++,
                                               .RegisterSpace = 0
                    };
                        break;
                    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                        assert(0);//not currently supported
                        break;
                }
                rootParam.ShaderVisibility = EnumToDx12(it.stages);
                rootParams.emplace_back(rootParam);
            }
            else if (validRootSampler)
            {
                rootSamplers.emplace_back(rootSampler);
            }
        }
    }
    else
    {
        // Descriptor table (non root)
        auto& [descriptors] = m_Layout.emplace<DescriptorTableParameters>();
        descriptors.reserve(numDescriptors);

        for (const auto& it : description.m_descriptorTypes)
        {
            D3D12_DESCRIPTOR_RANGE descriptor {};
            bool validDescriptor = false;

            bool readOnly = it.readOnly;
            switch (it.type) {
                case DescriptorSetDescription::DescriptorType::ImageSampler:
                    assert(0); // not currently valid (needs 2 slots?)
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::ImageSampled:
                    descriptor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                    validDescriptor = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::Sampler:
                    descriptor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                    validDescriptor = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::UniformBuffer:
                    descriptor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                    validDescriptor = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::StorageBuffer:
                    descriptor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                    validDescriptor = true;
                    break;
                case DescriptorSetDescription::DescriptorType::ImageStorage:
                    descriptor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                    validDescriptor = true;
                    break;
                case DescriptorSetDescription::DescriptorType::InputAttachment:
                    assert(0);  ///TODO: implement DX12 equivalent to input attachements (if one exists with render passes)
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::AccelerationStructure:
                    descriptor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                    validDescriptor = true;
                    assert(readOnly);
                    break;
                case DescriptorSetDescription::DescriptorType::DrawIndirectBuffer:
                    assert(0 && "DrawIndirectBuffer is not a supported binding to a descriptor set");
                    break;
                case DescriptorSetDescription::DescriptorType::DescriptorTable:
                    assert(0);  // Descriptor table cannot reference another table
                    break;
            }
        
            if (validDescriptor)
            {
                descriptor.NumDescriptors = it.count;
                if (it.count <= 0)
                {
                    // Count of 0 denotes that we dont (yet) know how many descriptors will go into this slot.
                    ///TODO: support variable descriptor counts for DX12
                    assert(0);
                }
        
                if (it.descriptorIndex < 0)
                {
                    // Index of < 0 denotes we want to use sequential descriptor binding indices.
                    ///TODO: look for collisions or determine how/if we want to handle out of order desciptor indices or enforce shaders that have an explicit binding index to define indices for all descriptors.
                    ///TODO: also ensure D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND works in this case
                    descriptor.BaseShaderRegister = shaderRegisterIndex;
                    shaderRegisterIndex += descriptor.NumDescriptors;
                }
                else
                    assert(0);  ///TODO: implement user defined descriptor indices for DX12
        
                descriptor.RegisterSpace = 1;
                descriptor.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                descriptors.emplace_back(descriptor);
            }
        }
    }

    return true;
}


//VkDescriptorSetLayout DescriptorSetLayout<Dx12>::CreateVkDescriptorSetLayout(Dx12& dx12, const std::span<const VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings)
//{
//    //
//    // Create the descriptor set layout
//    //
//    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
//    layoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
//    layoutInfo.pBindings = descriptorSetLayoutBindings.data();
//
//    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
//
//    VkResult retVal = vkCreateDescriptorSetLayout(dx12.m_VulkanDevice, &layoutInfo, nullptr, &descriptorSetLayout);
//
//    LOGI("vkCreateDescriptorSetLayout");
//    for (const auto& binding: descriptorSetLayoutBindings)
//    {
//        LOGI("   binding: %u\tdescriptorType: 0x%x  descriptorCount: %d  stageFlags: 0x%x  pImmutableSamplers : %p", binding.binding, binding.descriptorType, binding.descriptorCount, binding.stageFlags, binding.pImmutableSamplers);
//    }
//
//    if (!CheckVkError("vkCreateDescriptorSetLayout()", retVal))
//    {
//        return VK_NULL_HANDLE;
//    }
//    return descriptorSetLayout;
//}
//
//
//void DescriptorSetLayout<Dx12>::CalculatePoolSizes(const std::span<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings, std::vector<VkDescriptorPoolSize>& descriptorPoolSizes)
//{
//#define MAX_USED_DESCRIPTOR_TYPES   ((VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1) + 2)    // Take a guess at how many unique descript types will be used
//
//    descriptorPoolSizes.reserve(MAX_USED_DESCRIPTOR_TYPES);
//
//    //
//    // Calculate the descriptor set pool sizes
//    //
//    for (const auto& binding : descriptorSetLayoutBindings)
//    {
//        auto it = std::find_if(descriptorPoolSizes.begin(), descriptorPoolSizes.end(), [&binding](const VkDescriptorPoolSize& x) { return x.type == binding.descriptorType; });
//        if (it == descriptorPoolSizes.end())
//        {
//            descriptorPoolSizes.push_back({ binding.descriptorType, binding.descriptorCount });
//        }
//        else
//        {
//            it->descriptorCount += binding.descriptorCount;
//        }
//    }
//}



