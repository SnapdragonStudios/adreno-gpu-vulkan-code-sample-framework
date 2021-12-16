// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "bufferObject.hpp"
#include <vector>

// forward declarations
class VertexFormat;

/// Buffer containing vertex data.
/// @ingroup Memory
class VertexBufferObject : public BufferObject
{
    VertexBufferObject& operator=(const VertexBufferObject&) = delete;
    VertexBufferObject(const VertexBufferObject&) = delete;
public:
    VertexBufferObject();
    VertexBufferObject(VertexBufferObject&&) noexcept;
    VertexBufferObject& operator=(VertexBufferObject&&) noexcept;
    virtual ~VertexBufferObject();

    bool Initialize(MemoryManager* pManager, size_t span, size_t numVerts, const void* initialData, const bool dspUsable = false);
    template<typename T>
    bool Initialize(MemoryManager* pManager, size_t numVerts, const T* initialData, const bool dspUsable = false);

    /// destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy() override;

    /// create a copy of this vertex buffer (including a new copy of the data)
    VertexBufferObject Copy();

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
bool VertexBufferObject::Initialize(MemoryManager* pManager, size_t numVerts, const T* initialData, const bool dspUsable)
{
    return Initialize(pManager, sizeof(T), numVerts, initialData, dspUsable);
}
