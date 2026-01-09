//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "vertexBufferObject.hpp"
#include "material/dx12/vertexDescription.hpp"


///////////////////////////////////////////////////////////////////////////////

VertexBuffer<Dx12>::~VertexBuffer()
{
    //mBindings.clear();
    //mAttributes.clear();
}


///////////////////////////////////////////////////////////////////////////////

VertexBuffer<Dx12>::VertexBuffer(VertexBuffer<Dx12>&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

VertexBuffer<Dx12>& VertexBuffer<Dx12>::operator=(VertexBuffer<Dx12>&& other) noexcept
{
    Buffer<Dx12>::operator=(std::move(other));
    if (&other != this)
    {
        mSpan = other.mSpan;
        other.mSpan = 0;
        mNumVertices = other.mNumVertices;
        other.mNumVertices = 0;
        mDspUsable = other.mDspUsable;
        mView = other.mView;
        other.mView = {};
        //mBindings = std::move(other.mBindings);
        //mAttributes = std::move(other.mAttributes);
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

bool VertexBuffer<Dx12>::Initialize(MemoryManager* pManager, size_t span, size_t numVerts, const void* initialData, const bool dspUsable, const BufferUsageFlags usage )
{
    mNumVertices = numVerts;
    mSpan = span;
    mDspUsable = dspUsable;

    mView = {};

    if (dspUsable)
    {
        mManager = pManager;
        assert(0); // dsp currently unsupported
        return false;
    }
    else
    {
        if (!Buffer<Dx12>::Initialize(pManager, static_cast<size_t>(mSpan * mNumVertices), usage, initialData))
            return false;

        mView.BufferLocation = mAllocatedBuffer.GetResource()->GetGPUVirtualAddress();
        mView.SizeInBytes = mNumVertices * mSpan;
        mView.StrideInBytes = mSpan;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

void VertexBuffer<Dx12>::Destroy()
{
    mView = {};
    //mBindings.clear();
    //mAttributes.clear();
    mNumVertices = 0;
    Buffer<Dx12>::Destroy();
}

///////////////////////////////////////////////////////////////////////////////

VertexBuffer<Dx12> VertexBuffer<Dx12>::Copy()
{
    VertexBuffer copy;
    if (copy.Initialize(mManager, GetSpan(), GetNumVertices(), Map<void>().data(), mDspUsable, mBufferUsageFlags ))
    {
        //copy.mBindings = mBindings;
        //copy.mAttributes = mAttributes;
        copy.mElements = mElements;
        return copy;
    }
    else
    {
        return {};
    }
}

///////////////////////////////////////////////////////////////////////////////

void VertexBuffer<Dx12>::AddBindingAndAtributes(uint32_t binding, const VertexFormat& vertexFormat)
{
    VertexDescription v(vertexFormat, binding);
    mElements.insert(mElements.end(), v.GetVertexElementDescs().begin(), v.GetVertexElementDescs().end());
}
//
/////////////////////////////////////////////////////////////////////////////////
//
//void VertexBuffer<Dx12>::AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)
//{
//    VkVertexInputBindingDescription vibd = {};
//    vibd.binding = binding;
//    vibd.stride = stride;
//    vibd.inputRate = inputRate;
//    mBindings.push_back(vibd);
//}
//
/////////////////////////////////////////////////////////////////////////////////
//
//void VertexBuffer<Dx12>::AddAttribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format)
//{
//    VkVertexInputAttributeDescription viad = {};
//    viad.binding = binding;
//    viad.location = location;
//    viad.offset = offset;
//    viad.format = format;
//    mAttributes.push_back(viad);
//}
//
/////////////////////////////////////////////////////////////////////////////////
//
//VkPipelineVertexInputStateCreateInfo VertexBuffer<Dx12>::CreatePipelineState() const
//{
//    assert(mBindings.size() > 0);
//    assert(mAttributes.size() > 0);
//    VkPipelineVertexInputStateCreateInfo visci = {};
//    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//    visci.pNext = nullptr;
//    visci.vertexBindingDescriptionCount = (uint32_t)mBindings.size();
//    visci.pVertexBindingDescriptions = &mBindings[0];
//    visci.vertexAttributeDescriptionCount = (uint32_t)mAttributes.size();
//    visci.pVertexAttributeDescriptions = &mAttributes[0];
//    return visci;
//}

///////////////////////////////////////////////////////////////////////////////
