//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "indexBufferObject.hpp"


D3D12_INDEX_BUFFER_VIEW IndexBuffer<Dx12>::GetIndexBufferView() const
{
    DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
    UINT formatBytes = 2;
    switch (mIndexType) {
        case IndexType::IndexU16:
            format = DXGI_FORMAT_R16_UINT;
            formatBytes = 2;
            break;
        case IndexType::IndexU32:
            format = DXGI_FORMAT_R32_UINT;
            formatBytes = 4;
            break;
        default:
            assert(0);
            break;
    }

    return D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = mAllocatedBuffer.GetResource()->GetGPUVirtualAddress(),
        .SizeInBytes = (UINT) GetNumIndices() * formatBytes,
        .Format = format
    };
}
