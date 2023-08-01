//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "indexBufferObject.hpp"

VkIndexType IndexBuffer<Vulkan>::IndexTypeToVk(IndexType indexType)
{
    VkIndexType vkIndexType = VK_INDEX_TYPE_NONE_KHR;
    switch (indexType) {
    case IndexType::IndexU8:
        vkIndexType = VK_INDEX_TYPE_UINT8_EXT;
        break;
    case IndexType::IndexU16:
        vkIndexType = VK_INDEX_TYPE_UINT16;
        break;
    case IndexType::IndexU32:
        vkIndexType = VK_INDEX_TYPE_UINT32;
        break;
    case IndexType::Unknown:
        break;
    }
    return vkIndexType;
}

///////////////////////////////////////////////////////////////////////////////

IndexBuffer<Vulkan> IndexBuffer<Vulkan>::Copy( VkCommandBuffer vkCommandBuffer, BufferUsageFlags bufferUsage, MemoryUsage memoryUsage ) const
{
    IndexBuffer<Vulkan> copy( mIndexType );

    copy.mNumIndices = mNumIndices;
    copy.mDspUsable = mDspUsable;

    if (mDspUsable)
    {
        assert( 0 ); // dsp currently unsupported
        return copy;
    }

    if (( ( mBufferUsageFlags & BufferUsageFlags::TransferSrc) == 0 ) || ( ( bufferUsage & BufferUsageFlags::TransferDst) == 0 ))
    {
        // treat this as an error - use Copy() for buffers that are not setup for transfer
        assert( 0 );
        return copy;
    }

    // Create the buffer we are copying into.
    size_t size = GetIndexTypeBytes() * mNumIndices;
    if (!copy.BufferT::Initialize( mManager, size, bufferUsage, memoryUsage ))
        return copy;

    // Use gpu copy commands to copy buffer data
    if (!mManager->CopyData( vkCommandBuffer, mAllocatedBuffer, copy.mAllocatedBuffer, size, 0, 0 ))
    {
        copy.Destroy();
        return copy;
    }
    return copy;
}
