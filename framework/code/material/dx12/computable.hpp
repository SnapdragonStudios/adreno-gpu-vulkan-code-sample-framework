//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <d3d12.h>
#include "../computable.hpp"
#include "material.hpp"
#include "pipeline.hpp"


template<>
class ImageMemoryBarrier<Dx12> final : public ImageMemoryBarrierBase
{
public:
    typedef D3D12_RESOURCE_BARRIER tBarrier;
    ImageMemoryBarrier( D3D12_RESOURCE_BARRIER&& barrier ) noexcept
        : vkBarrier( std::move( barrier ) )
    {}
    D3D12_RESOURCE_BARRIER vkBarrier;
};
static_assert(sizeof( ImageMemoryBarrier<Dx12> ) == sizeof( D3D12_RESOURCE_BARRIER ));

template<>
class BufferMemoryBarrier<Dx12> final : public BufferMemoryBarrierBase
{
public:
    typedef D3D12_RESOURCE_BARRIER tBarrier;
    BufferMemoryBarrier( D3D12_RESOURCE_BARRIER&& barrier ) noexcept
        : vkBarrier( std::move( barrier ) )
    {}
    tBarrier vkBarrier;
};
static_assert(sizeof( BufferMemoryBarrier<Dx12> ) == sizeof( D3D12_RESOURCE_BARRIER ));


template<>
class ComputablePass<Dx12> final : public ComputablePassBase
{
    ComputablePass( const ComputablePass<Dx12>& ) = delete;
    ComputablePass& operator=( const ComputablePass<Dx12>& ) = delete;
public:
    ComputablePass( const MaterialPass<Dx12>& materialPass, Pipeline<Dx12> pipeline, /*VkPipelineLayout pipelineLayout,*/ std::vector<D3D12_RESOURCE_BARRIER> imageMemoryBarriers, std::vector<D3D12_RESOURCE_BARRIER> bufferMemoryBarriers, bool needsExecutionBarrier)
        : ComputablePassBase( materialPass )
        , mPipeline( std::move( pipeline ) )
        //, mPipelineLayout( pipelineLayout )
        , mImageMemoryBarriers( std::move( imageMemoryBarriers ) )
        , mBufferMemoryBarriers( std::move( bufferMemoryBarriers ) )
        , mNeedsExecutionBarrier( needsExecutionBarrier )
    {}
    ComputablePass( ComputablePass<Dx12>&& ) noexcept;
    ~ComputablePass();

    const auto& GetMaterialPass() const { return apiCast<Dx12>( mMaterialPass ); }

    const auto& GetVkImageMemoryBarriers() const { return mImageMemoryBarriers; }
    const auto& GetVkBufferMemoryBarriers() const { return mBufferMemoryBarriers; }
    const bool NeedsBarrier() const { return mNeedsExecutionBarrier || (!mImageMemoryBarriers.empty()) || (!mBufferMemoryBarriers.empty()); };   ///< @return true if there needs to be a barrier before executing this compute pass

    Pipeline<Dx12>                      mPipeline;
    //VkPipelineLayout                    mPipelineLayout;            // Owned by ShaderPass or MaterialPass

protected:
    std::vector<D3D12_RESOURCE_BARRIER> mImageMemoryBarriers;     ///< Image barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::vector<D3D12_RESOURCE_BARRIER> mBufferMemoryBarriers;   ///< Buffer barriers for ENTRY in to this pass.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    bool                                mNeedsExecutionBarrier = false;///< Denotes if we need an execution barrier for ENTRY to this pass (even if there are no image or buffer barriers).
};

