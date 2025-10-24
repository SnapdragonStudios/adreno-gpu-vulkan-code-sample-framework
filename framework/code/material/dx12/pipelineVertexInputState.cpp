//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include <algorithm>
#include <iterator>
#include "pipelineVertexInputState.hpp"
#include "material/shaderDescription.hpp"
#include "vertexDescription.hpp"

DXGI_FORMAT EnumToDx12(VertexFormat::Element::ElementType elementType)
{
    switch (elementType.type)
    {
        case VertexFormat::Element::ElementType::t::Int32:
            return DXGI_FORMAT_R32_SINT;
            break;
        case VertexFormat::Element::ElementType::t::Float:
            return DXGI_FORMAT_R32_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::Boolean:
            return DXGI_FORMAT_R32_UINT;
            break;
        case VertexFormat::Element::ElementType::t::Vec2:
            return DXGI_FORMAT_R32G32_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::Vec3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::Vec4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::Int16:
            return DXGI_FORMAT_R16_SINT;
            break;
        case VertexFormat::Element::ElementType::t::Float16:
            return DXGI_FORMAT_R16_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::F16Vec2:
            return DXGI_FORMAT_R16G16_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::F16Vec3:
            assert(0);//unsupported
            return DXGI_FORMAT_UNKNOWN;
            break;
        case VertexFormat::Element::ElementType::t::F16Vec4:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
            break;
        case VertexFormat::Element::ElementType::t::Null:
            return DXGI_FORMAT_UNKNOWN;
            break;
    }
    assert(0);//unsupported
    return DXGI_FORMAT_UNKNOWN;
}


PipelineVertexInputState<Dx12>::PipelineVertexInputState(const ShaderDescription& shaderDescription, const std::vector<uint32_t>& buffersToBind) noexcept
{
    mInputElementDescs.reserve(buffersToBind.size() * 4/*rough estimate!*/);

    for (uint32_t binding = 0; binding < buffersToBind.size(); ++binding)
    {
        const auto& vertexFormat = shaderDescription.m_vertexFormats[buffersToBind[binding]];
        VertexDescription vertexDesc { vertexFormat, binding };
        std::copy( std::begin(vertexDesc.GetVertexElementDescs()), std::end(vertexDesc.GetVertexElementDescs()), std::back_inserter(mInputElementDescs));
    }

    /*
    mInputElementDescs.reserve(buffersToBind.size());
    uint32_t location = 0;
    for (uint32_t b = 0; b < buffersToBind.size(); ++b)
    {
        const auto& vertexFormat = shaderDescription.m_vertexFormats[buffersToBind[b]];
        for (uint32_t e = 0; e < vertexFormat.elements.size(); ++e)
        {
            const auto& elementFormat = vertexFormat.elements[e];

            auto& elementDesc = mInputElementDescs.emplace_back();
            elementDesc.SemanticName = vertexFormat.elementIds[e].c_str();
            elementDesc.SemanticIndex = 0;
            elementDesc.Format = EnumToDx12(elementFormat.type);
            elementDesc.InputSlot = b;
            elementDesc.AlignedByteOffset = elementFormat.offset;
            elementDesc.InputSlotClass = vertexFormat.inputRate == VertexFormat::eInputRate::Vertex ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
            elementDesc.InstanceDataStepRate = vertexFormat.inputRate == VertexFormat::eInputRate::Vertex ? 0 : 1;
        }
    }
    */
}
