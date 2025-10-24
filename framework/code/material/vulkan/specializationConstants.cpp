//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "specializationConstants.hpp"


bool SpecializationConstants<Vulkan>::Init( const SpecializationConstantsLayout<Vulkan>& layout, const std::span<VertexElementData> constants )
{
    const auto& layoutMapEntry = layout.GetVkSpecializationMapEntry();
    assert( layoutMapEntry.size() == constants.size() );
    if (layoutMapEntry.empty())
    {
        mSpecializationData.reset();
        return true;
    }

    std::span<std::byte> specializationConstantsRaw { new std::byte[layout.GetBufferSize()], layout.GetBufferSize() };    // unsafe - raw pointer in the span!

    for (auto constantIdx = 0; constantIdx < layoutMapEntry.size(); ++constantIdx)
    {
        const auto& constantLayout = layoutMapEntry[constantIdx];

        // copy the loaded constant data into the constant buffer
        std::span constantDataRaw = constants[constantIdx].getUnsafeData();
        assert( constantLayout.size == constantDataRaw.size() );
        std::copy( constantDataRaw.begin(), constantDataRaw.end(), specializationConstantsRaw.begin() + constantLayout.offset );
    }

    VkSpecializationInfo vkSpecializationInfo {};
    vkSpecializationInfo.mapEntryCount = (uint32_t) layout.GetVkSpecializationMapEntry().size();
    vkSpecializationInfo.pMapEntries = layout.GetVkSpecializationMapEntry().data();
    vkSpecializationInfo.dataSize = specializationConstantsRaw.size();
    vkSpecializationInfo.pData = specializationConstantsRaw.data(); // move ownership of the allocated buffer.

    mSpecializationData.emplace().specializationInfo = vkSpecializationInfo;

    return true;
}

SpecializationConstants<Vulkan>::VulkanSpecializationData::VulkanSpecializationData( SpecializationConstants<Vulkan>::VulkanSpecializationData&& other ) noexcept : specializationInfo( other.specializationInfo )/*dumb move*/
{
    other.specializationInfo.mapEntryCount = 0;
    other.specializationInfo.pMapEntries = nullptr;
    other.specializationInfo.dataSize = 0;
    other.specializationInfo.pData = nullptr;
}
