//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include <algorithm>
#include <iterator>
#include "renderPass.hpp"
#include "dx12/dx12.hpp"

// Forward declarations
class Dx12;


RenderPass<Dx12>::RenderPass() noexcept
{
}

RenderPass<Dx12>::~RenderPass()
{
}

RenderPass<Dx12>::RenderPass( RenderPass<Dx12>&& ) noexcept
{
    assert( 0 );    // move operator needed to keep containers happy, but currently not expected to be called (and so not implemented)
}


RenderPass<Dx12>::RenderPass(std::span<const TextureFormat> ColorFormats, TextureFormat DepthFormat) noexcept
{
    std::transform(ColorFormats.begin(), ColorFormats.end(),
                   std::back_inserter(mRenderTargetFormats), [](TextureFormat t) { return TextureFormatToDx(t); });
    mRenderTargetDepthFormat = TextureFormatToDx( DepthFormat );
}


RenderPass<Dx12> CreateRenderPass(Dx12& dx12,
                                  std::span<const TextureFormat> ColorFormats,
                                  TextureFormat DepthFormat,
                                  Msaa Msaa,
                                  RenderPassInputUsage ColorInputUsage,
                                  RenderPassOutputUsage ColorOutputUsage,
                                  bool ShouldClearDepth,
                                  RenderPassOutputUsage DepthOutputUsage,
                                  std::span<const TextureFormat> ResolveFormats)
{
    return RenderPass<Dx12>{ ColorFormats, DepthFormat };
}

void ReleaseRenderPass(Dx12& dx12, RenderPass<Dx12>& renderPass)
{
    renderPass = {};
}

