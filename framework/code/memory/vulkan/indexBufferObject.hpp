//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

class Vulkan;

#include "memoryMapped.hpp"
#include "bufferObject.hpp"
#include "memory/indexBuffer.hpp"

using IndexBufferObject = IndexBuffer<Vulkan>;

// Template specialization for Vulkan Index buffer
template<>
class IndexBuffer<Vulkan> final : public IndexBufferT<Vulkan>
{
public:
    IndexBuffer(IndexType i) noexcept : IndexBufferT<Vulkan>(i), mVkIndexType( IndexTypeToVk(i) ) {}
    //IndexBufferObject(VkIndexType) noexcept;
    IndexBuffer(IndexBuffer<Vulkan>&& o) noexcept : IndexBufferT(std::move(static_cast<IndexBufferT<Vulkan>&&>(o))), mVkIndexType( o.mVkIndexType ) {}
    IndexBuffer<Vulkan>& operator=(IndexBuffer<Vulkan>&& o) noexcept { assert(mVkIndexType == o.mVkIndexType); IndexBufferT<Vulkan>::operator=(std::move(static_cast<IndexBuffer<Vulkan> &&>(o))); return *this; };
    ~IndexBuffer() {}

    VkIndexType GetVkIndexType() const { return mVkIndexType; }

    /// Create a copy of this buffer with (potentially) different usage.  Will add vkCmdCopyBuffer commands to the provided 'copy' command buffer (caller's resposibility to execute this command list before either of the BufferObjects are destroyed).
    IndexBuffer Copy(VkCommandBuffer copyCommandBuffer, BufferUsageFlags bufferUsage = BufferUsageFlags::Index | BufferUsageFlags::TransferDst, MemoryUsage memoryUsage = MemoryUsage::GpuExclusive ) const;

protected:
    static VkIndexType IndexTypeToVk(IndexType indexType);

protected:
    const VkIndexType   mVkIndexType;
};
