//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once


/// Templated wrapper for collection of specialization constants (eg VkSpecializationMapEntry for Vulkan VkSpecializationInfo::pMapEntries).
/// Expected to be specialized (compile will fail if this template is used rather than a specialization of it)
/// @ingroup Material
template<typename T_GFXAPI>
class SpecializationConstants
{
    SpecializationConstants( const SpecializationConstants& ) = delete;
    SpecializationConstants operator=( const SpecializationConstants& ) = delete;
public:
    SpecializationConstants( SpecializationConstants&& ) noexcept = default;
    SpecializationConstants() noexcept = default;
    //bool Init( const SpecializationConstantsLayout<T_GFXAPI>& layout, const std::span<VertexElementData> constants ) {}

    //we are ok with this generic class being used as-is (eg by Dx12 who does not have specialization conatants!)
    //static_assert(sizeof(SpecializationConstantsLayout <T_GFXAPI>) >= 1);   // Ensure this class template is specialized (and not used as-is)
};

