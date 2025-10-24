//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "specializationConstantsLayout.hpp"
//#include "vulkan/vulkan.hpp"

SpecializationConstantsLayout<Vulkan>::SpecializationConstantsLayout( const std::span<const SpecializationConstantDescription> constantDescriptions ) noexcept
{
    mEntries.reserve( constantDescriptions.size() );

    uint32_t currentDataOffsetBytes = 0;
    for (const auto& constantDescription : constantDescriptions)
    {
        auto constantSizeBytes = constantDescription.type.size();
        auto constantAlignmentBytes = constantDescription.type.alignment();
        currentDataOffsetBytes = (currentDataOffsetBytes + constantAlignmentBytes - 1) & ~(constantAlignmentBytes - 1);
        mEntries.push_back( VkSpecializationMapEntry{ constantDescription.constantIndex, currentDataOffsetBytes, (size_t) constantSizeBytes } );
        currentDataOffsetBytes += constantSizeBytes;
    }

    mBufferSize = currentDataOffsetBytes;
}
