//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <volk/volk.h>
#include <vector>
//#include <memory>//TEMP
#include <span>
#include "../descriptorSetLayout.hpp"

// Forward declares
class DescriptorSetDescription;
class Vulkan;


/// Representation of the descriptor set layout.
/// template specialization of DescriptSetLayout<T_GFXAPI>
/// Builds/owns the Vulkan Descriptor Set Layout that can be used to allocate the descriptor sets.
/// Also has a mapping from descriptor slots name (nice name) to the their shader binding index.
/// @ingroup Material
template<>
class DescriptorSetLayout<Vulkan> : public DescriptorSetLayoutBase
{
    DescriptorSetLayout& operator=(const DescriptorSetLayout<Vulkan>&) = delete;
    DescriptorSetLayout(const DescriptorSetLayout<Vulkan>&) = delete;
public:
    DescriptorSetLayout() noexcept;
    ~DescriptorSetLayout();
    DescriptorSetLayout(DescriptorSetLayout<Vulkan>&&) noexcept;

    bool Init(Vulkan& vulkan, const DescriptorSetDescription&);
    void Destroy(Vulkan& vulkan);

    static VkDescriptorSetLayout CreateVkDescriptorSetLayout(Vulkan& vulkan, const std::span<const VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings);
    static void CalculatePoolSizes(const std::span<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings, std::vector<VkDescriptorPoolSize>& descriptorPoolSizes/*output*/);

    const auto& GetVkDescriptorSetLayoutBinding() const { return m_descriptorSetLayoutBindings; }
    const auto& GetVkDescriptorSetLayout() const { return m_descriptorSetLayout; }
    const auto& GetDescriptorPoolSizes() const { return m_descriptorPoolSizes; }

private:
    // Vulkan objects
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;   ///< Vulkan descriptor set layout object.  Can be VK_NULL_HANDLE after Init if there are bindings with 'dynamic' descriptorCount (0)
    std::vector<VkDescriptorSetLayoutBinding > m_descriptorSetLayoutBindings;
    std::vector<VkDescriptorPoolSize> m_descriptorPoolSizes;
};
