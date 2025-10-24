//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "pipelineLayout.hpp"
#include "descriptorSetLayout.hpp"
#include <iterator>
#include "dx12/dx12.hpp"
#include "texture/dx12/sampler.hpp"

PipelineLayout<Dx12>::~PipelineLayout()
{
    assert(m_rootSignature == nullptr);    // call Destroy prior to object being deleted
}

void PipelineLayout<Dx12>::Destroy(Dx12&)
{
    m_rootSignature.Reset();
}

bool PipelineLayout<Dx12>::Init(Dx12& dx12, const std::span<const DescriptorSetLayout<Dx12>> descriptorTableLayouts, const std::span<const CreateSamplerObjectInfo> rootSamplers)
{
    assert(!descriptorTableLayouts.empty());

    // Require that the first descriptor table (set) layout is for the root signature.

    // Get the root signature layout
    const auto& rootSignatureLayout = std::get<DescriptorSetLayout<Dx12>::RootSignatureParameters>(descriptorTableLayouts[0].GetLayout());

    using tRangeSpan = std::span <const D3D12_DESCRIPTOR_RANGE >;
    std::vector<tRangeSpan> pDescriptorTableRanges;
    pDescriptorTableRanges.reserve(descriptorTableLayouts.size()-1);

    // Get the descriptor tables
    for (auto i = 1; i < descriptorTableLayouts.size(); ++i)
    {
        const auto& descriptorTableParameters = std::get<DescriptorSetLayout<Dx12>::DescriptorTableParameters>(descriptorTableLayouts[i].GetLayout());
        pDescriptorTableRanges.push_back( { std::begin(descriptorTableParameters.ranges), std::end(descriptorTableParameters.ranges) });
    }

    // We may need to copy the root descriptor table so we can modify it (patch in descriptor table pointers)
    std::vector<D3D12_ROOT_PARAMETER> rootParametersWritable;
    std::span <const D3D12_ROOT_PARAMETER > pRootParameters { std::begin(rootSignatureLayout.root), std::end(rootSignatureLayout.root) };
    for (auto i=0; i < pRootParameters.size(); ++i)
    {
        if (pRootParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            std::copy(pRootParameters.begin(), pRootParameters.end(), std::back_inserter(rootParametersWritable));
            pRootParameters = { rootParametersWritable.begin(), rootParametersWritable.end() };

            size_t tableIdx = 0;
            for (; i < rootParametersWritable.size(); ++i)
            {
                D3D12_ROOT_PARAMETER& rootParam = rootParametersWritable[i];
                if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
                {
                    rootParam.DescriptorTable.NumDescriptorRanges = pDescriptorTableRanges[tableIdx].size();
                    rootParam.DescriptorTable.pDescriptorRanges = pDescriptorTableRanges[tableIdx].data();
                }
            }

            break;
        }
    }

    D3D12_STATIC_SAMPLER_DESC staticSamplers[]{ {
        .Filter = D3D12_FILTER_ANISOTROPIC,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        .MaxLOD = FLT_MAX
    } };

    std::vector<D3D12_STATIC_SAMPLER_DESC> rootSamplersDx12;
    UINT i = 0;
    std::transform(rootSamplers.begin(), rootSamplers.end(), std::back_inserter(rootSamplersDx12), [&i](const auto& a) -> D3D12_STATIC_SAMPLER_DESC {
        auto samplerDesc = StructToDx12( a );

        return D3D12_STATIC_SAMPLER_DESC{
            .Filter = samplerDesc.Filter,
            .AddressU = samplerDesc.AddressU,
            .AddressV = samplerDesc.AddressV,
            .AddressW = samplerDesc.AddressW,
            .MipLODBias = samplerDesc.MipLODBias,
            .MaxAnisotropy = samplerDesc.MaxAnisotropy,
            .ComparisonFunc = samplerDesc.ComparisonFunc,
            .MinLOD = samplerDesc.MinLOD,
            .MaxLOD = samplerDesc.MaxLOD,
            .ShaderRegister = i++,
            .RegisterSpace = 0,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        };
    });

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{
        .NumParameters = (UINT)pRootParameters.size(),
        .pParameters = pRootParameters.data(),
        .NumStaticSamplers = _countof(staticSamplers),
        .pStaticSamplers = staticSamplers,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };

    ComPtr<ID3DBlob> signatureData;
    ComPtr<ID3DBlob> error;
    if (!Dx12::CheckError("D3D12SerializeRootSignature", D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureData, &error)))
    {
        return false;
    }

    if (!Dx12::CheckError("CreateRootSignature", dx12.GetDevice()->CreateRootSignature(0, signatureData->GetBufferPointer(), signatureData->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
        return false;

    return true;
}

