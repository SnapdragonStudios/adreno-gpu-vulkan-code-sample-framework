//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <span>
#include "../pipelineLayout.hpp"

// Forward declarations
using namespace Microsoft::WRL; // for ComPtr
class Dx12;
template<typename T_GFXAPI> class DescriptorSetLayout;
struct CreateSamplerObjectInfo;


/// Representation of the RootSignature (roughly the DirectX12 equivalent of a Vulkan pipeline layout).
/// template specialization of PipelineLayout<T_GFXAPI>
/// Builds/owns the root layout.
/// @ingroup Material
template<>
class PipelineLayout<Dx12>
{
    PipelineLayout& operator=(const PipelineLayout<Dx12>&) = delete;
    PipelineLayout(const PipelineLayout<Dx12>&) = delete;
public:
    PipelineLayout() noexcept {};
    PipelineLayout(PipelineLayout<Dx12>&&) noexcept = default;
    ~PipelineLayout();

    bool Init(Dx12&, const std::span<const DescriptorSetLayout<Dx12>>, const std::span<const CreateSamplerObjectInfo>);
    void Destroy(Dx12&);

    operator bool() const { return m_rootSignature.Get() != nullptr; }

    /// Get the root signature
    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
};
