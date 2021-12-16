// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <map>
#include <string>
#include <memory>//TEMP

// Forward declares
class DescriptorSetDescription;
class Vulkan;

/// Representation of the descriptor set layout.
/// Builds/owns the Vulkan Descriptor Set Layout that can be used to allocate the descriptor sets.
/// Also has a mapping from descriptor slots name (nice name) to the their shader binding index.
/// @ingroup Material
class DescriptorSetLayout
{
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
public:
    DescriptorSetLayout();
    ~DescriptorSetLayout();
    DescriptorSetLayout(DescriptorSetLayout&&) noexcept;

    bool Init(Vulkan& vulkan, const DescriptorSetDescription&);
    void Destroy(Vulkan& vulkan);

    const auto& GetVkDescriptorSetLayoutBinding() const { return m_descriptorSetLayoutBindings; }
    const auto& GetVkDescriptorSetLayout() const { return m_descriptorSetLayout; }
    const auto& GetNameToBinding() const { return m_nameToBinding; }
    const auto& GetDescriptorPoolSizes() const { return m_descriptorPoolSizes; }

    struct BindingTypeAndIndex {
        VkDescriptorType type;
        uint32_t index;
    };
private:
    std::map<std::string, BindingTypeAndIndex> m_nameToBinding; ///< For each named descriptor slot store the relevant binding index (one name per binding for simplicity) ///< @TODO should this be in here, or in the materialPass.

    // Vulkan objects
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding > m_descriptorSetLayoutBindings;
    std::vector<VkDescriptorPoolSize> m_descriptorPoolSizes;
};
