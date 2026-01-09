//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <d3d12.h>
#include <span>
#include <vector>
#include "graphicsApi/renderPass.hpp"
#include "texture/textureFormat.hpp"
#include "graphicsApi/graphicsApiBase.hpp"

// Forward declarations
class Dx12;


/// Simple wrapper around VkRenderPass.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// Specialization of RenderPass<T_GFXAPI>
/// @ingroup Material
template<>
class RenderPass<Dx12>
{
    RenderPass& operator=(const RenderPass<Dx12>&) = delete;
    RenderPass(const RenderPass<Dx12>&) = delete;
public:
    RenderPass() noexcept;
    RenderPass(RenderPass<Dx12>&&) noexcept;
    RenderPass(std::span<const TextureFormat> ColorFormats, TextureFormat DepthFormat ) noexcept;
    RenderPass<Dx12>& operator=(RenderPass<Dx12>&&) noexcept = default;
    ~RenderPass();

    operator bool() const { return true; }  // currently always valid!

    const auto& GetRenderTargetFormats() const { return mRenderTargetFormats; }
    DXGI_FORMAT GetRenderTargetDepthFormat() const { return mRenderTargetDepthFormat; }

protected:
    std::vector<DXGI_FORMAT>    mRenderTargetFormats;
    DXGI_FORMAT                 mRenderTargetDepthFormat = DXGI_FORMAT_UNKNOWN;
};


RenderPass<Dx12> CreateRenderPass(Dx12&,
                                  std::span < const TextureFormat > ColorFormats,
                                  TextureFormat DepthFormat,
                                  Msaa Msaa,
                                  RenderPassInputUsage ColorInputUsage,
                                  RenderPassOutputUsage ColorOutputUsage,
                                  bool ShouldClearDepth,
                                  RenderPassOutputUsage DepthOutputUsage,
                                  std::span < const TextureFormat > ResolveFormats);

void ReleaseRenderPass(Dx12& dx12, RenderPass<Dx12>& renderPass);
