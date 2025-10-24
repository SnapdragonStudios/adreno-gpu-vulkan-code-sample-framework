//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "vulkan/vulkan.hpp"
#include "bufferObject.hpp"
#include "memory/vertexBuffer.hpp"
#include <span>

class Vulkan;


/// Buffer containing vertex data (specialized for Vulkan).
/// @ingroup Memory
template<>
class VertexBuffer<Vulkan> : public Buffer<Vulkan>
{
    VertexBuffer& operator=(const VertexBuffer<Vulkan>&) = delete;
    VertexBuffer(const VertexBuffer<Vulkan>&) = delete;
public:
    VertexBuffer() noexcept;
    VertexBuffer(VertexBuffer<Vulkan>&&) noexcept;
    VertexBuffer<Vulkan>& operator=(VertexBuffer<Vulkan>&&) noexcept;
    virtual ~VertexBuffer();

    bool Initialize(MemoryManager* pManager, size_t span, size_t numVerts, const void* initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex );
    template<typename T>
    bool Initialize(MemoryManager* pManager, const std::span<const T> initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex);
    template<typename T>
    bool Initialize(MemoryManager* pManager, size_t numVerts, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex);

    bool Update(MemoryManager* pManager, size_t dataSize, const void* newData);

    bool GetMeshData(MemoryManager* pManager, size_t dataSize, void* outputData) const;

    /// destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy() override;

    /// create a copy of this vertex buffer (including a new copy of the data)
    VertexBuffer<Vulkan> Copy();

    /// create a copy of this vertex buffer (using vkCmdCopyBuffer)
    VertexBuffer<Vulkan> Copy( VkCommandBuffer vkCommandBuffer, BufferUsageFlags bufferUsage, MemoryUsage memoryUsage ) const;

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return GetSpan() * GetNumVertices(); }

    /// get span (in bytes)
    size_t GetSpan() const { return mSpan; }

    /// get number of vertices allocated
    size_t GetNumVertices() const { return mNumVertices; }

    // Vulkan vertex attributes
    void AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);
    void AddAttribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format);
    void AddBindingAndAtributes(uint32_t binding, const VertexFormat& vertexFormat); // helper, does AddBinding and AddAttribute(s)
    const auto& GetBindings() const { return mBindings; }
    const auto& GetAttributes() const { return mAttributes; }
    VkPipelineVertexInputStateCreateInfo CreatePipelineState() const;

    /// Access (cpu mapped) vertex data using attribute information
    template<typename T>
    const T* GetAttributeData(const MemoryCpuMappedUntyped& pMappedVertData, uint32_t location, uint32_t vertIndex, VkFormat expectedFormat) const
    {
        assert(mAttributes[location].location == location);      // going to assume locations are in order within mAttributes and contiguous.  May need to revisit this code if not!
        assert(mAttributes[location].format == expectedFormat);   // in the future we could convert, for now just expect it to match
        assert(vertIndex < mNumVertices);
        return (const T*)((static_cast<const uint8_t*>(pMappedVertData.data())) + mAttributes[location].offset + mSpan * vertIndex);
    }

protected:
    size_t mSpan;
    size_t mNumVertices;
    bool mDspUsable;
    std::vector<VkVertexInputBindingDescription> mBindings;
    std::vector<VkVertexInputAttributeDescription> mAttributes;
};

template<typename T>
bool VertexBuffer<Vulkan>::Initialize(MemoryManager* pManager, const std::span<const T> initialData, const bool dspUsable, const BufferUsageFlags usage)
{
    return Initialize(pManager, sizeof(T), initialData.size(), initialData.data(), dspUsable, usage);
}

template<typename T>
bool VertexBuffer<Vulkan>::Initialize(MemoryManager* pManager, size_t numVertices, const bool dspUsable, const BufferUsageFlags usage)
{
    return Initialize(pManager, sizeof(T), numVertices, nullptr, dspUsable, usage);
}
