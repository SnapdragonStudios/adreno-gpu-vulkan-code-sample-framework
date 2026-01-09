//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "vertexDescription.hpp"
#include <algorithm>
#include <cassert>
#include <set>
#include "system/os_common.h"


// Forward declaration
static std::pair<const char*, uint32_t> ParseSemanticName(const std::string& name);


VertexDescription::VertexDescription(const VertexFormat& format, uint32_t binding)
{
    m_Stride = format.span;
    m_Binding = binding;

    m_VertexElementDescs.reserve( format.elements.size() );

    // THIS NEEDS TO BE FILLED IN FOR INSTANCE DATA TO WORK (FOR NOW JUST ASSERT)
    uint32_t verticesPerInstance = 0;
    assert(format.inputRate != VertexFormat::eInputRate::Instance);
    // END

    uint32_t locationIdx = 0;
    for( auto i = 0; i<format.elements.size(); ++i )
    {
        auto& element = format.elements[i];
        auto& elementId = format.elementIds[i];

        std::pair<const char*,uint32_t> semantic = ParseSemanticName(elementId);
        if (semantic.first == nullptr)
        {
            LOGE("Invalid semantic name (\"%s\") in vertex format.", elementId.c_str());
        }

        D3D12_INPUT_ELEMENT_DESC elementDesc{
            .SemanticName = semantic.first,
            .SemanticIndex = semantic.second,
            .Format = DxgiFormatFromElementType(element.type),
            .InputSlot = binding,
            .AlignedByteOffset = element.offset,
            .InputSlotClass = format.inputRate == VertexFormat::eInputRate::Vertex ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
            .InstanceDataStepRate = format.inputRate == VertexFormat::eInputRate::Vertex ? 0 : verticesPerInstance,
        };
        m_VertexElementDescs.push_back(elementDesc);
    }
}

DXGI_FORMAT VertexDescription::DxgiFormatFromElementType( const VertexFormat::Element::ElementType& elementType )
{
    switch(elementType.type) {
        case VertexFormat::Element::ElementType::t::Int32:
            return DXGI_FORMAT_R32_SINT;
        case VertexFormat::Element::ElementType::t::UInt32:
            return DXGI_FORMAT_R32_UINT;
        case VertexFormat::Element::ElementType::t::Float:
            return DXGI_FORMAT_R32_FLOAT;
        case VertexFormat::Element::ElementType::t::Boolean:
            return DXGI_FORMAT_R32_UINT;
        case VertexFormat::Element::ElementType::t::Vec2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::Element::ElementType::t::Vec3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::Element::ElementType::t::Vec4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexFormat::Element::ElementType::t::IVec2:
            return DXGI_FORMAT_R32G32_SINT;
        case VertexFormat::Element::ElementType::t::IVec3:
            return DXGI_FORMAT_R32G32B32_SINT;
        case VertexFormat::Element::ElementType::t::IVec4:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case VertexFormat::Element::ElementType::t::UVec2:
            return DXGI_FORMAT_R32G32_UINT;
        case VertexFormat::Element::ElementType::t::UVec3:
            return DXGI_FORMAT_R32G32B32_UINT;
        case VertexFormat::Element::ElementType::t::UVec4:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case VertexFormat::Element::ElementType::t::Int16:
            return DXGI_FORMAT_R16_SINT;
        case VertexFormat::Element::ElementType::t::UInt16:
            return DXGI_FORMAT_R16_UINT;
        case VertexFormat::Element::ElementType::t::Float16:
            return DXGI_FORMAT_R16_FLOAT;
        case VertexFormat::Element::ElementType::t::F16Vec2:
            return DXGI_FORMAT_R16G16_FLOAT;
        //case VertexFormat::Element::ElementType::t::F16Vec3:
        //    return DXGI_FORMAT_R16G16B16_FLOAT;// undefined in DXGI
        case VertexFormat::Element::ElementType::t::F16Vec4:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case VertexFormat::Element::ElementType::t::I16Vec2:
            return DXGI_FORMAT_R16G16_SINT;
        //case VertexFormat::Element::ElementType::t::I16Vec3:
        //    return DXGI_FORMAT_R16G16B16_SINT;// undefined in DXGI
        case VertexFormat::Element::ElementType::t::I16Vec4:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case VertexFormat::Element::ElementType::t::U16Vec2:
            return DXGI_FORMAT_R16G16_UINT;
        //case VertexFormat::Element::ElementType::t::U16Vec3:
        //    return DXGI_FORMAT_R16G16B16_UINT;// undefined in DXGI
        case VertexFormat::Element::ElementType::t::U16Vec4:
            return DXGI_FORMAT_R16G16B16A16_UINT;

        default:
            assert(0);
            return DXGI_FORMAT_UNKNOWN;
            break;
    }
}

// Searchable set of supported vertex shader semantic names.
// Because we created the name stringviews from 'C' char strings we can be confident they are \0 terminated although this is not guaranteed by the string_view spec.
static std::set<std::string_view> sSemanticNames{
    {"BINORMAL"},
    {"BLENDINDICES"},
    {"BLENDWEIGHT"},
    {"COLOR"},
    {"NORMAL"},
    {"POSITION"},
    {"POSITIONT"},
    {"PSIZE"},
    {"TANGENT"},
    {"TEXCOORD"}};

static std::pair<const char*, uint32_t> ParseSemanticName(const std::string& name)
{
    uint32_t semanticIndex = 0;

    auto numStartIt = std::find_if(name.begin(), name.end(), [](char c)->bool { return isdigit(c) != 0; });
    if (numStartIt != name.end())
    {
        semanticIndex = strtol(&(*numStartIt), nullptr, 10);
    }
    auto semanticIt = std::find(sSemanticNames.begin(), sSemanticNames.end(), std::string_view(name));
    if (semanticIt == sSemanticNames.end())
        return {};
    else
        return { semanticIt->data(), semanticIndex};
}

