//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

///
/// Vertex buffer template (platform agnostic)
/// @group memory
/// 

#include "buffer.hpp"
#include <vector>

// forward declarations
class VertexFormat;

#if 1

/// Buffer containing vertex data.
/// @ingroup Memory
template<typename T_GFXAPI>
class VertexBuffer : public BufferT<T_GFXAPI>
{
    ~VertexBuffer() noexcept = delete;                                          // Ensure this class template is specialized (and not used as-is)
    static_assert(sizeof(VertexBuffer<T_GFXAPI>) != sizeof(BufferT<T_GFXAPI>));  // Ensure this class template is specialized (and not used as-is)
};

#else

template<typename T_GFXAPI>
class VertexBuffer : public BufferT<T_GFXAPI>
{
public:
    VertexBuffer() noexcept;
    VertexBuffer(VertexBuffer&&) noexcept;
    VertexBuffer& operator=(VertexBuffer&&) noexcept;
    virtual ~VertexBuffer();

    bool Initialize(MemoryManager* pManager, size_t span, size_t numVerts, const void* initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex );
    template<typename T>
    bool Initialize(MemoryManager* pManager, size_t numVerts, const T* initialData, const bool dspUsable = false, const BufferUsageFlags usage = BufferUsageFlags::Vertex);

    /// destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy() override;

    /// create a copy of this vertex buffer (including a new copy of the data)
    VertexBuffer Copy();

    /// get number of bytes allocated
    size_t GetAllocationSize() const { return GetSpan() * GetNumVertices(); }

    /// get span (in bytes)
    size_t GetSpan() const { return mSpan; }

    /// get number of vertices allocated
    size_t GetNumVertices() const { return mNumVertices; }

    /// Vertex attributes
    void AddBindingAndAtributes(uint32_t binding, const VertexFormat& vertexFormat) {}

protected:
    size_t mSpan;
    size_t mNumVertices;
    bool mDspUsable;
};

template<typename T_GFXAPI> template<typename T>
bool VertexBuffer<T_GFXAPI>::Initialize(MemoryManager* pManager, size_t numVerts, const T* initialData, const bool dspUsable, const BufferUsageFlags usage )
{
    return false;//Initialize(pManager, sizeof(T), numVerts, initialData, dspUsable, usage);
}
#endif
