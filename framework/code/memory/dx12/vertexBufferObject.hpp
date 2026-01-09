//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "dx12/dx12.hpp"
#include "bufferObject.hpp"
#include "memory/vertexBuffer.hpp"
#include <span>

class Dx12;
using VertexBufferDx12 = VertexBuffer<Dx12>;


/// Buffer containing vertex data (specialized for DX12).
/// @ingroup Memory
template<>
class VertexBuffer<Dx12> : public Buffer<Dx12>
{
    VertexBuffer& operator=(const VertexBuffer<Dx12>&) = delete;
    VertexBuffer(const VertexBuffer<Dx12>&) = delete;
public:
    VertexBuffer() noexcept = default;
    VertexBuffer(VertexBuffer<Dx12>&&) noexcept;
    VertexBuffer<Dx12>& operator=(VertexBuffer<Dx12>&&) noexcept;
    virtual ~VertexBuffer();

    bool Initialize(MemoryManager* pManager, size_t span, size_t numVerts, const void* initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex );
    template<typename T>
    bool Initialize(MemoryManager* pManager, const std::span<const T> initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex);

    /// destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy() override;

    /// create a copy of this vertex buffer (including a new copy of the data)
    VertexBuffer<Dx12> Copy();

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return GetSpan() * GetNumVertices(); }

    /// get span (in bytes)
    size_t GetSpan() const { return mSpan; }

    /// get number of vertices allocated
    size_t GetNumVertices() const { return mNumVertices; }

    // Dx12 vertex attributes
    //void AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);
    //void AddAttribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format);
    void AddBindingAndAtributes(uint32_t binding, const VertexFormat& vertexFormat); // helper, does AddBinding and AddAttribute(s)
    //const auto& GetBindings() const { return mBindings; }
    //const auto& GetAttributes() const { return mAttributes; }
    //VkPipelineVertexInputStateCreateInfo CreatePipelineState() const;
    const auto& GetVertexBufferView() const { return mView; }
    const auto& GetElementDescs() const { return mElements; }

    /// Access (cpu mapped) vertex data using attribute information
    //template<typename T>
    //const T* GetAttributeData(const MemoryCpuMappedUntyped& pMappedVertData, uint32_t location, uint32_t vertIndex, VkFormat expectedFormat) const
    //{
    //    assert(mAttributes[location].location == location);      // going to assume locations are in order within mAttributes and contiguous.  May need to revisit this code if not!
    //    assert(mAttributes[location].format == expectedFormat);   // in the future we could convert, for now just expect it to match
    //    assert(vertIndex < mNumVertices);
    //    return (const T*)((static_cast<const uint8_t*>(pMappedVertData.data())) + mAttributes[location].offset + mSpan * vertIndex);
    //}

protected:
    size_t mSpan = 0;
    size_t mNumVertices = 0;
    bool mDspUsable = false;
    //std::vector<VkVertexInputBindingDescription> mBindings;
    //std::vector<VkVertexInputAttributeDescription> mAttributes;
    D3D12_VERTEX_BUFFER_VIEW mView{};
    std::vector<D3D12_INPUT_ELEMENT_DESC> mElements;

};

template<typename T>
bool VertexBuffer<Dx12>::Initialize(MemoryManager* pManager, const std::span<const T> initialData, const bool dspUsable, const BufferUsageFlags usage )
{
    return Initialize(pManager, sizeof(T), initialData.size(), initialData.data(), dspUsable, usage);
}
