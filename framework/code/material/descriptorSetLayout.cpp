//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "descriptorSetLayout.hpp"
#include "descriptorSetDescription.hpp"
#include "vulkan/vulkan.hpp"
#include <array>
#include <cassert>

DescriptorSetLayout::DescriptorSetLayout()
{
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    assert(m_descriptorSetLayout == VK_NULL_HANDLE);
}

void DescriptorSetLayout::Destroy(Vulkan& vulkan)
{
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vulkan.m_VulkanDevice, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSetLayoutBindings.clear();
    m_descriptorPoolSizes.clear();
}

DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
    : m_nameToBinding( std::move(other.m_nameToBinding))
    , m_descriptorSetLayoutBindings(std::move(other.m_descriptorSetLayoutBindings))
    , m_descriptorPoolSizes(std::move(other.m_descriptorPoolSizes))
{
    m_descriptorSetLayout = other.m_descriptorSetLayout;
    other.m_descriptorSetLayout = VK_NULL_HANDLE;
}

bool DescriptorSetLayout::Init(Vulkan& vulkan, const DescriptorSetDescription& description)
{
    const size_t numBindings = description.m_descriptorTypes.size();
    uint32_t index = 0;
    bool dynamicDescriptorCount = false;

    m_descriptorSetLayoutBindings.clear();
    m_descriptorSetLayoutBindings.reserve(numBindings);
    for (const auto& it : description.m_descriptorTypes)
    {
        auto& binding = m_descriptorSetLayoutBindings.emplace_back();
        binding.binding = index++;
        binding.descriptorCount = it.count;
        if (it.count <= 0)
            // Count of 0 denotes that we dont (yet) know how many descriptors will go into this slot - layout will be created alongside the descriptor set.
            dynamicDescriptorCount = true;
        bool readOnly = it.readOnly;
        switch (it.type) {
        case DescriptorSetDescription::DescriptorType::ImageSampler:
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            assert(readOnly);
            break;
        case DescriptorSetDescription::DescriptorType::UniformBuffer:
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            assert(readOnly);
            break;
        case DescriptorSetDescription::DescriptorType::StorageBuffer:
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case DescriptorSetDescription::DescriptorType::ImageStorage:
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        case DescriptorSetDescription::DescriptorType::InputAttachment:
            binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            assert(readOnly);
            break;
        case DescriptorSetDescription::DescriptorType::DrawIndirectBuffer:
            assert(0 && "DrawIndirectBuffer is not a supported binding to a descriptor set");
            break;
        }
        VkShaderStageFlagBits stageFlags = (VkShaderStageFlagBits)0;
        if (it.stages & DescriptorSetDescription::StageFlag::t::Vertex)
            binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        if (it.stages & DescriptorSetDescription::StageFlag::t::Fragment)
            binding.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (it.stages & DescriptorSetDescription::StageFlag::t::Geometry)
            binding.stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if (it.stages & DescriptorSetDescription::StageFlag::t::Compute)
            binding.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
        binding.pImmutableSamplers = nullptr;

        assert(it.names.size() <= 1);   ///TODO: only one name supported, needs to store the index within the descriptor as well as the binding index if we want to support this! (the 'for' loop below is not the full implementation)
        for (const auto& name : it.names)
        {
            auto nameToBindingEmplaced = m_nameToBinding.try_emplace(name, BindingTypeAndIndex{ binding.descriptorType, binding.binding, binding.descriptorCount!=1/*isArray*/, readOnly });
            assert(nameToBindingEmplaced.second); // name must be unique
        }
    }

    m_descriptorPoolSizes.clear();

    //
    // Create the descriptor set layout
    //
    assert(m_descriptorSetLayout == VK_NULL_HANDLE);
    if (!dynamicDescriptorCount)
    {
        m_descriptorSetLayout = CreateVkDescriptorSetLayout(vulkan, m_descriptorSetLayoutBindings);

        CalculatePoolSizes(m_descriptorSetLayoutBindings, m_descriptorPoolSizes);
    }

    return true;
}


VkDescriptorSetLayout DescriptorSetLayout::CreateVkDescriptorSetLayout(Vulkan& vulkan, const tcb::span<const VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings)
{
    //
    // Create the descriptor set layout
    //
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
    layoutInfo.pBindings = descriptorSetLayoutBindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    VkResult retVal = vkCreateDescriptorSetLayout(vulkan.m_VulkanDevice, &layoutInfo, nullptr, &descriptorSetLayout);
    if (!CheckVkError("vkCreateDescriptorSetLayout()", retVal))
    {
        return VK_NULL_HANDLE;
    }
    return descriptorSetLayout;
}


void DescriptorSetLayout::CalculatePoolSizes(const tcb::span<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings, std::vector<VkDescriptorPoolSize>& descriptorPoolSizes)
{
#define MAX_USED_DESCRIPTOR_TYPES   ((VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1) + 2)    // Take a guess at how many unique descript types will be used

    descriptorPoolSizes.reserve(MAX_USED_DESCRIPTOR_TYPES);

    //
    // Calculate the descriptor set pool sizes
    //
    for (const auto& binding : descriptorSetLayoutBindings)
    {
        auto it = std::find_if(descriptorPoolSizes.begin(), descriptorPoolSizes.end(), [&binding](const VkDescriptorPoolSize& x) { return x.type == binding.descriptorType; });
        if (it == descriptorPoolSizes.end())
        {
            descriptorPoolSizes.push_back({ binding.descriptorType, binding.descriptorCount });
        }
        else
        {
            it->descriptorCount += binding.descriptorCount;
        }
    }
}



