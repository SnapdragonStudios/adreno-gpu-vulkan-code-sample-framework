//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include "specializationConstantDescription.hpp"
#include <span>

//#include "vulkan/specializationConstantsLayout.hpp"


// Forward declarations
class Vulkan;

/// Templated wrapper for collection of specialization constant layout data (eg VkSpecializationMapEntry for Vulkan VkSpecializationInfo::pMapEntries).
/// Expected to be specialized (compile will fail if this template is used rather than a specialization of it)
/// @ingroup Material
template<typename T_GFXAPI>
class SpecializationConstantsLayout
{
    SpecializationConstantsLayout( const SpecializationConstantsLayout<T_GFXAPI>& ) = delete;
    SpecializationConstantsLayout& operator=( const SpecializationConstantsLayout<T_GFXAPI>& ) = delete;
    SpecializationConstantsLayout& operator=(SpecializationConstantsLayout<T_GFXAPI>&&) noexcept = delete;
public:
    explicit SpecializationConstantsLayout(const std::span<const SpecializationConstantDescription> constantDescriptions) noexcept;
    SpecializationConstantsLayout(SpecializationConstantsLayout<T_GFXAPI>&& other) noexcept;

    static_assert(sizeof(SpecializationConstantsLayout <T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};

