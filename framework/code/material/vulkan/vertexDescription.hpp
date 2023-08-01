//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "material/vertexFormat.hpp"
#include <vulkan/vulkan.h>

/// Describes the Vulkan layout of a single vertex stream (not the buffer, just one of the the contained vertices).
/// @ingroup Material
class VertexDescription
{
    VertexDescription(const VertexDescription&) = delete;
    VertexDescription& operator=(const VertexDescription&) = delete;
public:
    VertexDescription(const VertexFormat&, uint32_t binding);

    size_t      GetStride()  const { return m_inputBindingDescription.stride; }
    uint32_t    GetBinding() const { return m_inputBindingDescription.binding; }
    const std::vector<VkVertexInputAttributeDescription>& GetVertexInputAttributeDescriptions() const { return m_inputAttributeDescriptions; }

    static VkFormat VkFormatFromElementType( const VertexFormat::Element::ElementType& );
protected:
    VkVertexInputBindingDescription m_inputBindingDescription;
    std::vector<VkVertexInputAttributeDescription> m_inputAttributeDescriptions;
};
