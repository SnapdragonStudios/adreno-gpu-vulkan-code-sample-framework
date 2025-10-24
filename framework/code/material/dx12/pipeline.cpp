//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <algorithm>
#include "dx12/dx12.hpp"
#include "dx12/renderPass.hpp"
#include "pipeline.hpp"
#include "pipelineLayout.hpp"
#include "pipelineVertexInputState.hpp"
#include "shader.hpp"
#include "shaderModule.hpp"
#include "../shaderDescription.hpp"

// Forward declarations
class Dx12;


Pipeline<Dx12>::Pipeline() noexcept
    : mPipeline()
{
}

Pipeline<Dx12>::Pipeline(ComPtr<ID3D12PipelineState> pipeline) noexcept
    : mPipeline( std::move(pipeline) )
{
    assert( mPipeline );
}

Pipeline<Dx12>::~Pipeline()
{
}

static D3D12_BLEND EnumToDx12( ShaderPassDescription::BlendFactor bf)
{
    switch( bf )
    {
    case ShaderPassDescription::BlendFactor::Zero:
        return D3D12_BLEND_ZERO;
    case ShaderPassDescription::BlendFactor::One:
        return D3D12_BLEND_ONE;
    case ShaderPassDescription::BlendFactor::SrcAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case ShaderPassDescription::BlendFactor::OneMinusSrcAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case ShaderPassDescription::BlendFactor::DstAlpha:
        return D3D12_BLEND_DEST_ALPHA;
    case ShaderPassDescription::BlendFactor::OneMinusDstAlpha:
        return D3D12_BLEND_INV_DEST_ALPHA;
    }
    assert( 0 );
    return D3D12_BLEND_ZERO;
}

static UINT EnumToDx12( Msaa msaa )
{
    return (UINT) msaa;
}


template<>
Pipeline<Dx12> CreatePipeline( Dx12& dx12,
                               const ShaderPassDescription& shaderPassDescription,
                               const PipelineLayout<Dx12>& pipelineLayout,
                               const PipelineVertexInputState<Dx12>& inputLayout,
                               const PipelineRasterizationState<Dx12>& rasterizationState,
                               const SpecializationConstants<Dx12>&,
                               const ShaderModules<Dx12>& shaderModules,
                               const RenderPass<Dx12>& renderPass,
                               uint32_t subpass,
                               Msaa msaa)
{
    // State for rasterization, such as polygon fill mode is defined.
    const auto& fixedFunctionSettings = shaderPassDescription.m_fixedFunctionSettings;
    D3D12_CULL_MODE cullMode = (fixedFunctionSettings.cullBackFace == false) ? ((fixedFunctionSettings.cullFrontFace == false) ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_FRONT) : D3D12_CULL_MODE_BACK;
    D3D12_RASTERIZER_DESC rasterizerState{
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = cullMode,
        .FrontCounterClockwise = FALSE,
        .DepthBias = fixedFunctionSettings.depthBiasEnable ? (int)fixedFunctionSettings.depthBiasConstant : D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = fixedFunctionSettings.depthBiasEnable ? fixedFunctionSettings.depthBiasClamp : D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = fixedFunctionSettings.depthBiasEnable ? fixedFunctionSettings.depthBiasSlope : D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .DepthClipEnable = fixedFunctionSettings.depthClampEnable ? TRUE : FALSE,
        .MultisampleEnable = FALSE,
        .AntialiasedLineEnable = FALSE,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };

    // Setup blending/transparency
    D3D12_BLEND_DESC blendState{
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
    };
    const auto& outputSettings = shaderPassDescription.m_outputs;
    assert(outputSettings.size() <= 8/*max render targets currently in Dx12*/);
    for (auto i=0; i < outputSettings.size(); ++i)
    {
        const auto& outputSetting = outputSettings[i];
        blendState.RenderTarget[i] = D3D12_RENDER_TARGET_BLEND_DESC{
            .BlendEnable = outputSetting.blendEnable ? TRUE : FALSE,
            .LogicOpEnable = FALSE,
            .SrcBlend = EnumToDx12(outputSetting.srcColorBlendFactor),
            .DestBlend = EnumToDx12(outputSetting.dstColorBlendFactor),
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = EnumToDx12(outputSetting.srcAlphaBlendFactor),
            .DestBlendAlpha = EnumToDx12(outputSetting.dstAlphaBlendFactor),
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = outputSetting.colorWriteMask & (D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE | D3D12_COLOR_WRITE_ENABLE_ALPHA)
        };
    }

    // Setup depth testing
    D3D12_COMPARISON_FUNC depthFunction;
    switch( fixedFunctionSettings.depthCompareOp ) {
        case ShaderPassDescription::DepthCompareOp::LessEqual:
            depthFunction = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            break;
        case ShaderPassDescription::DepthCompareOp::Equal:
            depthFunction = D3D12_COMPARISON_FUNC_EQUAL;
            break;
        case ShaderPassDescription::DepthCompareOp::Greater:
            depthFunction = D3D12_COMPARISON_FUNC_GREATER;
            break;
    }
    D3D12_DEPTH_STENCIL_DESC depthState {
        .DepthEnable = fixedFunctionSettings.depthTestEnable ? TRUE : FALSE,
        .DepthWriteMask = fixedFunctionSettings.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc = depthFunction,
        .StencilEnable = FALSE,
        .StencilReadMask = 0xff,
        .StencilWriteMask = 0xff,
    };

    DXGI_SAMPLE_DESC sampleDesc {
        .Count = EnumToDx12(msaa),
        .Quality = 0
    };

    uint32_t sampleMask = UINT_MAX;
    const auto& sampleShadingSettings = shaderPassDescription.m_sampleShadingSettings;
    assert(sampleShadingSettings.sampleShadingEnable == false); // not currently implemented
    assert(sampleShadingSettings.forceCenterSample == false);   // not currently implemented
    if (sampleShadingSettings.sampleShadingMask != 0)
    {
        assert(sampleDesc.Count <= 32 ); // sampleMask is only 32bits currently! Easy fix if we want > 32x MSAA
        sampleMask = sampleShadingSettings.sampleShadingMask & ((1u << sampleDesc.Count) -1u);
    }

    using tShaderModuleData = ShaderModule<Dx12>::tData;
    tShaderModuleData emptyData = {};
    auto [vertData, fragData] = std::visit( [&](auto& m) -> std::pair<const tShaderModuleData&,const tShaderModuleData&>
    {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, GraphicsShaderModules<Dx12>>)
        {
            return { m.vert.GetShaderData(), m.frag.GetShaderData() };
        }
        else if constexpr (std::is_same_v<T, GraphicsShaderModuleVertOnly<Dx12>>)
        {
            return { m.vert.GetShaderData(), emptyData };
        }
        else
        {
            assert(0);    // unsupported shader module type (eg ComputeShaderModule)
            return { emptyData, emptyData };
        }
    }, shaderModules.m_modules );

    ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = inputLayout.GetInputLayoutDesc();
    psoDesc.pRootSignature = const_cast<ID3D12RootSignature*>( pipelineLayout.GetRootSignature() );
    psoDesc.VS.pShaderBytecode = vertData.data();
    psoDesc.VS.BytecodeLength = vertData.size();
    psoDesc.PS.pShaderBytecode = fragData.data();
    psoDesc.PS.BytecodeLength = fragData.size();
    psoDesc.RasterizerState = rasterizerState;
    psoDesc.BlendState = blendState;
    psoDesc.DepthStencilState = depthState;
    psoDesc.SampleMask = sampleMask;
    psoDesc.SampleDesc = sampleDesc;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = outputSettings.size();

    const auto& renderPassFormats = renderPass.GetRenderTargetFormats();
    psoDesc.NumRenderTargets = (UINT) renderPassFormats.size();
    assert(psoDesc.NumRenderTargets <= 8);
    psoDesc.NumRenderTargets = std::min(psoDesc.NumRenderTargets, 8u);
    for (auto i=0; i < psoDesc.NumRenderTargets; ++i)
        psoDesc.RTVFormats[i] = renderPassFormats[i];
    psoDesc.DSVFormat = renderPass.GetRenderTargetDepthFormat();

    assert( psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask == 0xf );

    if (S_OK != dx12.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))
    {
        // Error
        return {};
    }
    return Pipeline<Dx12>(std::move(pipelineState));
}
