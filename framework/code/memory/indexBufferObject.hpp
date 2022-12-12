//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "bufferObject.hpp"


/// Buffer containing vertex indices
/// @ingroup Memory
class IndexBufferObject : public BufferObject
{
    IndexBufferObject(const IndexBufferObject&) = delete;
    IndexBufferObject& operator=(const IndexBufferObject&) = delete;
public:
    IndexBufferObject(VkIndexType);
    IndexBufferObject(IndexBufferObject&&) noexcept;
    IndexBufferObject& operator=(IndexBufferObject&&) noexcept;
    ~IndexBufferObject();

    /// Initialization
    template<typename T> bool Initialize( MemoryManager* pManager, size_t numIndices, const T* initialData, const bool dspUsable = false, const VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT );

    /// Initialization
    bool Initialize( MemoryManager* pManager, size_t numIndices, const bool dspUsable = false, const VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT );

    /// destroy buffer and leave in a state where it could be re-initialized
    void Destroy();

    /// create a copy of this index buffer (including a new copy of the data)
    IndexBufferObject Copy();

    /// Create a copy of this buffer with (potentially) different usage.  Will add vkCmdCopyBuffer commands to the provided 'copy' command buffer (caller's resposibility to execute this command list before either of the BufferObjects are destroyed).
    IndexBufferObject Copy(VkCommandBuffer copyCommandBuffer, VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryManager::MemoryUsage memoryUsage = MemoryManager::MemoryUsage::GpuExclusive ) const;

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return GetIndexTypeBytes() * mNumIndices; }

    /// Map this buffer to the cpu and return a guard object (automatically unmaps when it goes out of scope)
    template<typename T> MapGuard<T> Map();

    VkIndexType GetIndexType() const { return mIndexType; }
    uint32_t GetIndexTypeBytes() const { switch (mIndexType) { case VK_INDEX_TYPE_UINT8_EXT: return 1; case VK_INDEX_TYPE_UINT16: return 2; case VK_INDEX_TYPE_UINT32: return 4; default: assert(0); return 1; } }
    auto GetNumIndices() const { return mNumIndices; }

protected:
    MapGuard<void> MapVoid();
    bool Initialize(MemoryManager* pManager, size_t numIndices, const void* initialData, const bool dspUsable, const VkBufferUsageFlags usage);

private:
    size_t mNumIndices = 0;
    const VkIndexType mIndexType;
    bool mDspUsable = false;
    //std::unique_ptr<BufferObject> mVulkanBuffer;
    //std::unique_ptr<MappableBuffer> mDspBuffer;
};


template<typename T>
bool IndexBufferObject::Initialize(MemoryManager* pManager, size_t numIndices, const T* initialData, const bool dspUsable, const VkBufferUsageFlags usage)
{
    assert(sizeof(T) == GetIndexTypeBytes());
    return Initialize(pManager, numIndices, (const void*)initialData, dspUsable, usage);
}

inline bool IndexBufferObject::Initialize(MemoryManager* pManager, size_t numIndices, const bool dspUsable, const VkBufferUsageFlags usage)
{
    return Initialize(pManager, numIndices, (const void*)nullptr, dspUsable, usage);
}

template<typename T>
BufferObject::MapGuard<T> IndexBufferObject::Map()
{
    assert(sizeof(T) == GetIndexTypeBytes());
    return BufferObject::Map<T>();
}

inline BufferObject::MapGuard<void> IndexBufferObject::MapVoid()
{
    return BufferObject::Map<void>();
}
