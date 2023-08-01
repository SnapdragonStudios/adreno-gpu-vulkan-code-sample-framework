//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "material/specializationConstantsLayout.hpp"
#include <span>

// Forward declarations
class Vulkan;

/// Simple wrapper around collection of VkSpecializationMapEntry (for VkSpecializationInfo::pMapEntries).
/// @ingroup Material
template<>
class SpecializationConstantsLayout<Vulkan>
{
    SpecializationConstantsLayout( const SpecializationConstantsLayout<Vulkan>& ) = delete;
    SpecializationConstantsLayout& operator=( const SpecializationConstantsLayout<Vulkan>& ) = delete;
public:
    explicit SpecializationConstantsLayout( const std::span<const SpecializationConstantDescription> constantDescriptions ) noexcept;
    SpecializationConstantsLayout( SpecializationConstantsLayout<Vulkan>&& other ) noexcept : mBufferSize( other.mBufferSize ), mEntries( std::move(other.mEntries ) ) { other.mBufferSize = 0; }
    SpecializationConstantsLayout& operator=( SpecializationConstantsLayout<Vulkan>&& ) noexcept = delete;

    size_t GetBufferSize() const { return mBufferSize; }
    const auto& GetVkSpecializationMapEntry() const { return mEntries; }

private:
    size_t mBufferSize;
    std::vector<VkSpecializationMapEntry> mEntries;
};

