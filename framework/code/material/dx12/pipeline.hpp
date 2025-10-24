//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <span>
#include "../pipeline.hpp"

// Forward declarations
class Dx12;
class ShaderPassDescription;
enum class Msaa;
template<typename T_GFXAPI> class PipelineLayout;
template<typename T_GFXAPI> class GraphicsShaderModules;
template<typename T_GFXAPI> class PipelineVertexInputState;
template<typename T_GFXAPI> class ShaderModules;
template<typename T_GFXAPI> class RenderPass;
using namespace Microsoft::WRL; // for ComPtr


/// Simple wrapper around Pipeline.
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// Specialization of Pipeline<T_GFXAPI>
/// @ingroup Material
template<>
class Pipeline<Dx12> final
{
    Pipeline& operator=( const Pipeline<Dx12>& ) = delete;
public:
    Pipeline() noexcept;
    ~Pipeline();
    Pipeline( Pipeline<Dx12>&& ) noexcept = default;
    Pipeline<Dx12>& operator=( Pipeline<Dx12>&& ) noexcept = default;
    Pipeline( ComPtr<ID3D12PipelineState> ) noexcept;

    Pipeline Copy() const { return Pipeline{*this}; }

    operator bool() const { return mPipeline; }

    ID3D12PipelineState* GetPipeline() const { return mPipeline.Get(); }

private:
    Pipeline( const Pipeline<Dx12>& src ) noexcept {
        mPipeline = src.mPipeline;
    }
    ComPtr<ID3D12PipelineState> mPipeline;
};


template<>
Pipeline<Dx12> CreatePipeline(Dx12&,
                              const ShaderPassDescription&,
                              const PipelineLayout<Dx12>&,
                              const PipelineVertexInputState<Dx12>& inputLayout,
                              const PipelineRasterizationState<Dx12>&,
                              const SpecializationConstants<Dx12>&,
                              const ShaderModules<Dx12>&,
                              const RenderPass<Dx12>&,
                              uint32_t subpass,
                              Msaa msaa);
