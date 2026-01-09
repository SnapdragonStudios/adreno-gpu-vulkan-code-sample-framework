//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <optional>
#include <span>
#include "../specializationConstants.hpp"
#include "specializationConstantsLayout.hpp"

// Forward declarations


/// Specialization constant data for a pipeline.
/// @ingroup Material
template<>
class SpecializationConstants<Vulkan> final
{
    SpecializationConstants( const SpecializationConstants& ) = delete;
    SpecializationConstants operator=( const SpecializationConstants& ) = delete;
public:
    SpecializationConstants( SpecializationConstants&& ) noexcept = default;
    SpecializationConstants() noexcept = default;
    bool Init(const SpecializationConstantsLayout<Vulkan>& layout, const std::span<VertexElementData> constants);

    const VkSpecializationInfo* GetVkSpecializationInfo() const { return mSpecializationData.has_value() ? &mSpecializationData.value().specializationInfo : nullptr; }

private:
    struct VulkanSpecializationData
    {
        VulkanSpecializationData( const VulkanSpecializationData& ) = delete;
        VulkanSpecializationData operator=( const VulkanSpecializationData& ) = delete;

        VulkanSpecializationData() noexcept : specializationInfo() {}
        ~VulkanSpecializationData() { delete[] (std::byte*) specializationInfo.pData/*we own just the data*/; specializationInfo.pData = nullptr; }
        VulkanSpecializationData( VulkanSpecializationData&& ) noexcept;
        VkSpecializationInfo specializationInfo;
    };
    // container for specialization data prepared for vulkan use!
    std::optional<VulkanSpecializationData> mSpecializationData;
};
