//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "material/vertexFormat.hpp"
#include <d3d12.h>

/// Describes the Dx12 elements (layout) for a single vertex stream (not the buffer, just one of the the contained vertices).
/// @ingroup Material
class VertexDescription
{
    VertexDescription(const VertexDescription&) = delete;
    VertexDescription& operator=(const VertexDescription&) = delete;
public:
    VertexDescription(const VertexFormat&, uint32_t binding);

    size_t      GetStride()  const { return m_Stride; }
    uint32_t    GetBinding() const { return m_Binding; }
    const std::vector<D3D12_INPUT_ELEMENT_DESC>& GetVertexElementDescs() const { return m_VertexElementDescs; }

    static DXGI_FORMAT DxgiFormatFromElementType(const VertexFormat::Element::ElementType& elementType);
protected:
    size_t m_Stride;
    uint32_t m_Binding;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_VertexElementDescs;
};
