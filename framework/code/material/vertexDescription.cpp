// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "vertexDescription.hpp"
#include <cassert>

VertexDescription::VertexDescription(const VertexFormat& format, uint32_t binding)
{
    m_inputBindingDescription.stride = format.span;
    m_inputBindingDescription.binding = binding;

    m_inputAttributeDescriptions.reserve( format.elements.size() );

    uint32_t locationIdx = 0;
    for( auto& emement : format.elements )
    {
        VkVertexInputAttributeDescription inputAttributeDescription;
        inputAttributeDescription.binding = binding;
        inputAttributeDescription.location = locationIdx++;
        inputAttributeDescription.format = VkFormatFromElementType(emement.type);
        inputAttributeDescription.offset = emement.offset;
        m_inputAttributeDescriptions.push_back( inputAttributeDescription );
    }
}

VkFormat VertexDescription::VkFormatFromElementType( const VertexFormat::Element::ElementType& elementType )
{
    switch(elementType) {
        case VertexFormat::Element::ElementType::t::Int32:
            return VK_FORMAT_R32_SINT;
        case VertexFormat::Element::ElementType::t::Float:
            return VK_FORMAT_R32_SFLOAT;
        case VertexFormat::Element::ElementType::t::Vec2:
            return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Element::ElementType::t::Vec3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Element::ElementType::t::Vec4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            assert(0);
            return VK_FORMAT_UNDEFINED;
            break;
    }
}
