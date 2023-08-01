//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <map>
#include <string>
#include <memory>//TEMP
#include <span>

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

    static VkDescriptorSetLayout CreateVkDescriptorSetLayout(Vulkan& vulkan, const std::span<const VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings);
    static void CalculatePoolSizes(const std::span<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings, std::vector<VkDescriptorPoolSize>& descriptorPoolSizes/*output*/);

    const auto& GetVkDescriptorSetLayoutBinding() const { return m_descriptorSetLayoutBindings; }
    const auto& GetVkDescriptorSetLayout() const { return m_descriptorSetLayout; }
    const auto& GetNameToBinding() const { return m_nameToBinding; }
    const auto& GetDescriptorPoolSizes() const { return m_descriptorPoolSizes; }

    struct BindingTypeAndIndex {
        VkDescriptorType type;
        uint32_t index;
        bool isArray;
        bool isReadOnly;
    };
private:
    std::map<std::string, BindingTypeAndIndex> m_nameToBinding; ///< For each named descriptor slot store the relevant binding index (one name per binding for simplicity) ///< @TODO should this be in here, or in the materialPass.

    // Vulkan objects
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;   ///< Vulkan descriptor set layout object.  Can be VK_NULL_HANDLE after Init if there are bindings with 'dynamic' descriptorCount (0)
    std::vector<VkDescriptorSetLayoutBinding > m_descriptorSetLayoutBindings;
    std::vector<VkDescriptorPoolSize> m_descriptorPoolSizes;
};
