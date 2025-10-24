//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "computable.hpp"
#include "system/os_common.h"
#include "dx12/commandList.hpp"


ComputablePass<Dx12>::~ComputablePass()
{
}


ComputablePass<Dx12>::ComputablePass( ComputablePass<Dx12>&& other ) noexcept
    : ComputablePassBase(std::move(other))
    , mPipeline(std::move(other.mPipeline))
    , mImageMemoryBarriers(std::move(other.mImageMemoryBarriers))
    , mBufferMemoryBarriers(std::move(other.mBufferMemoryBarriers))
    , mNeedsExecutionBarrier(other.mNeedsExecutionBarrier)
{
}


template<>
bool Computable<Dx12>::Init()
{
    LOGI( "Creating Computable" );

    const auto& materialPasses = mMaterial.GetMaterialPasses();
    mPasses.reserve( materialPasses.size() );

    return true;
}


template<>
void Computable<Dx12>::SetDispatchGroupCount( uint32_t passIdx, const std::array<uint32_t, 3>& groupCount )
{
    mPasses[passIdx].SetDispatchGroupCount( groupCount );
}

template<>
void Computable<Dx12>::SetDispatchThreadCount( uint32_t passIdx, const std::array<uint32_t, 3>& threadCount )
{
    mPasses[passIdx].SetDispatchThreadCount( threadCount );
}

template<>
void Computable<Dx12>::DispatchPass( CommandList<Dx12>& cmdList, const ComputablePass<Dx12>& computablePass, uint32_t bufferIdx ) const
{
    auto* cmdBuffer = cmdList.Get();

    // Add image barriers (if needed)
    if (computablePass.NeedsBarrier())
    {
    }

    // Bind the pipeline for this material

    // Bind everything the shader needs

    // Dispatch the compute task
}

template<>
void Computable<Dx12>::Dispatch( CommandList<Dx12>& cmdList, uint32_t bufferIdx, bool timers ) const
{
    for (uint32_t passIdx = 0; const auto & computablePass : GetPasses())
    {
        DispatchPass( cmdList, computablePass, bufferIdx % (uint32_t)computablePass.GetMaterialPass().GetDescriptorTables().size());
        ++passIdx;
    }
}

template<>
void Computable<Dx12>::Dispatch( CommandListBase& cmdList, uint32_t bufferIdx, bool timers ) const /*override*/
{
    Dispatch( apiCast<Dx12>( cmdList ), bufferIdx, timers );
}

template<>
void Computable<Dx12>::AddOutputBarriersToCmdList( CommandList<Dx12>& commandList ) const
{
    const auto& computableOutputBufferBarriers = GetBufferOutputMemoryBarriers();
    const auto& computableOutputImageBarriers = GetImageOutputMemoryBarriers();

    if (computableOutputBufferBarriers.empty() && computableOutputImageBarriers.empty())
        return;
}

template<>
void Computable<Dx12>::AddOutputBarriersToCmdList( CommandListBase& cmdList ) const /*override*/
{
    AddOutputBarriersToCmdList( apiCast<Dx12>( cmdList ) );
}
