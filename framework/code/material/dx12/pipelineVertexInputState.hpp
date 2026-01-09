//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <d3d12.h>
#include <vector>
#include "../pipelineVertexInputState.hpp"


// Forward declarations
class ShaderDescription;
class Dx12;


/// Template specialization for PipelineVertexInputState on Dx12.
/// Empty class since DX12 does not need to have anything equivalient defined.
/// @ingroup Material
template<>
class PipelineVertexInputState<Dx12>
{
    PipelineVertexInputState(const PipelineVertexInputState<Dx12>&) = delete;
    PipelineVertexInputState<Dx12>& operator=(const PipelineVertexInputState<Dx12>&) = delete;
public:
    PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind /*indices of buffers (in shaderDescription.m_vertexFormats) to bind in this input state*/ ) noexcept;
    PipelineVertexInputState(PipelineVertexInputState<Dx12>&&) noexcept = default;

    const auto& GetInputElementDescs() const { return mInputElementDescs; }
    D3D12_INPUT_LAYOUT_DESC GetInputLayoutDesc() const { return D3D12_INPUT_LAYOUT_DESC { mInputElementDescs.data(), (uint32_t) mInputElementDescs.size() }; }

protected:

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputElementDescs;
};

