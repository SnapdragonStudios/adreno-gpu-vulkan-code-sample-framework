//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

class Dx12;

#include "memoryMapped.hpp"
#include "bufferObject.hpp"
#include "memory/indexBuffer.hpp"


// Template specialization for Dx12 Index buffer
template<>
class IndexBuffer<Dx12> final : public IndexBufferT<Dx12>
{
public:
    IndexBuffer(IndexType i) noexcept : IndexBufferT<Dx12>(i) {}
    //IndexBuffer(VkIndexType) noexcept;
    IndexBuffer(IndexBuffer<Dx12>&& o) noexcept = default;
    IndexBuffer<Dx12>& operator=(IndexBuffer<Dx12>&& o) noexcept = default;
    ~IndexBuffer() {}

    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;
};
