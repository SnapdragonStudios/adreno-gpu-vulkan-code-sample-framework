//============================================================================================================
//
//
//                  Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "computable.hpp"
#include "materialPass.hpp"
#include "shaderDescription.hpp"

ComputablePassBase::ComputablePassBase( ComputablePassBase&& other) noexcept
    : mDispatchGroupCount( other.mDispatchGroupCount )
    , mMaterialPass( other.mMaterialPass )
{
    other.mDispatchGroupCount = {1,1,1};
}

void ComputablePassBase::SetDispatchThreadCount( const std::array<uint32_t, 3> threadCount )
{
    std::array<uint32_t, 3> groupCount;
    const auto& cWorkGroupLocalSize = mMaterialPass.mShaderPass.m_shaderPassDescription.m_workGroupSettings.localSize;
    if (cWorkGroupLocalSize[0] == 0 || cWorkGroupLocalSize[1] == 0 || cWorkGroupLocalSize[2] == 0)
    {
        // Workgroup local size must be defined in the shader definition if we want to use this function
        assert( 0 );
        return;
    }
    groupCount[0] = (threadCount[0] + cWorkGroupLocalSize[0] - 1) / cWorkGroupLocalSize[0];
    groupCount[1] = (threadCount[1] + cWorkGroupLocalSize[1] - 1) / cWorkGroupLocalSize[1];
    groupCount[2] = (threadCount[2] + cWorkGroupLocalSize[2] - 1) / cWorkGroupLocalSize[2];

    SetDispatchGroupCount( groupCount );
}
