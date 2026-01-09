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
#include <vector>
#include <variant>
#include "../descriptorSetLayout.hpp"

// Forward declares
class DescriptorSetDescription;
class Dx12;


/// Representation of a descriptor table (set) for Dx12.
/// template specialization of DescriptSetLayout<T_GFXAPI>
/// Used to describe the root signature or a descriptor table pointed to by the root signature.
/// Also has a mapping from descriptor slots name (nice name) to the their table index.
/// @ingroup Material
template<>
class DescriptorSetLayout<Dx12> : public DescriptorSetLayoutBase
{
    DescriptorSetLayout& operator=(const DescriptorSetLayout<Dx12>&) = delete;
    DescriptorSetLayout(const DescriptorSetLayout<Dx12>&) = delete;
public:
    DescriptorSetLayout() noexcept;
    ~DescriptorSetLayout();
    DescriptorSetLayout(DescriptorSetLayout<Dx12>&&) noexcept;

    bool Init(Dx12& , const DescriptorSetDescription&);
    void Destroy(Dx12&);

    struct RootSignatureParameters {
        std::vector<D3D12_ROOT_PARAMETER> root;
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
    };
    struct DescriptorTableParameters {
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
    };
    const auto& GetLayout() const { return m_Layout;  }

    template<bool ROOT> const auto& GetLayout() const;
    template<> const auto& GetLayout<true>() const { return std::get<RootSignatureParameters>(m_Layout); }
    template<> const auto& GetLayout<false>() const { return std::get<DescriptorTableParameters>(m_Layout); }

private:
    std::variant < RootSignatureParameters, DescriptorTableParameters > m_Layout;
};
