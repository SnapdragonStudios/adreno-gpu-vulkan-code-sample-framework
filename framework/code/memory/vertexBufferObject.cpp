//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "vertexBufferObject.hpp"
#include "material/vertexDescription.hpp"


VertexBufferObject::VertexBufferObject()
    : BufferObject()
    , mSpan(0)
    , mNumVertices(0)
    , mDspUsable(false)
{
}


///////////////////////////////////////////////////////////////////////////////

VertexBufferObject::~VertexBufferObject()
{
    mBindings.clear();
    mAttributes.clear();
}


///////////////////////////////////////////////////////////////////////////////

VertexBufferObject::VertexBufferObject(VertexBufferObject&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

VertexBufferObject& VertexBufferObject::operator=(VertexBufferObject&& other) noexcept
{
    BufferObject::operator=(std::move(other));
    if (&other != this)
    {
        mSpan = other.mSpan;
        other.mSpan = 0;
        mNumVertices = other.mNumVertices;
        other.mNumVertices = 0;
        mDspUsable = other.mDspUsable;
        mBindings = std::move(other.mBindings);
        mAttributes = std::move(other.mAttributes);
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

bool VertexBufferObject::Initialize(MemoryManager* pManager, size_t span, size_t numVerts, const void* initialData, const bool dspUsable, const VkBufferUsageFlags usage )
{
    mNumVertices = numVerts;
    mSpan = span;
    mDspUsable = dspUsable;

    if (dspUsable)
    {
        mManager = pManager;
        assert(0); // dsp currently unsupported
        return false;
    }
    else
    {
        return BufferObject::Initialize(pManager, (VkDeviceSize) (mSpan * mNumVertices), usage, initialData);
    }
}

///////////////////////////////////////////////////////////////////////////////

void VertexBufferObject::Destroy()
{
    mBindings.clear();
    mAttributes.clear();
    mNumVertices = 0;
    BufferObject::Destroy();
}

///////////////////////////////////////////////////////////////////////////////

VertexBufferObject VertexBufferObject::Copy()
{
    VertexBufferObject copy;
    if (copy.Initialize(mManager, GetSpan(), GetNumVertices(), Map<void>().data(), mDspUsable, mBufferUsageFlags ))
    {
        copy.mBindings = mBindings;
        copy.mAttributes = mAttributes;
        return copy;
    }
    else
    {
        return {};
    }
}

///////////////////////////////////////////////////////////////////////////////

VertexBufferObject VertexBufferObject::Copy( VkCommandBuffer vkCommandBuffer, VkBufferUsageFlags bufferUsage, MemoryManager::MemoryUsage memoryUsage) const
{
    VertexBufferObject copy;

    copy.mNumVertices = mNumVertices;
    copy.mSpan = mSpan;
    copy.mDspUsable = mDspUsable;

    if (mDspUsable)
    {
        assert( 0 ); // dsp currently unsupported
        return {};
    }

    // Create the buffer we are copying into.
    size_t size = GetSpan() * GetNumVertices();
    if (!copy.BufferObject::Initialize( mManager, size, bufferUsage, memoryUsage ))
        return {};

    if (((mBufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0) || ((bufferUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0))
    {
        // This copy must be done via copy cmd and transferable buffers (otherwise call VertexBufferObject::Copy() )
        assert( 0 );
        return {};
    }

    // Use gpu copy commands to copy buffer data
    if (!mManager->CopyData( vkCommandBuffer, mVmaBuffer, copy.mVmaBuffer, size, 0, 0 ))
        return {};
    copy.mBindings = mBindings;
    copy.mAttributes = mAttributes;
    return copy;
}

///////////////////////////////////////////////////////////////////////////////

void VertexBufferObject::AddBindingAndAtributes(uint32_t binding, const VertexFormat& vertexFormat)
{
    AddBinding(binding, (uint32_t)vertexFormat.span, vertexFormat.inputRate==VertexFormat::eInputRate::Vertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE);

    VertexDescription v(vertexFormat, binding);
    mAttributes.insert(mAttributes.end(), v.GetVertexInputAttributeDescriptions().begin(), v.GetVertexInputAttributeDescriptions().end());
}

///////////////////////////////////////////////////////////////////////////////

void VertexBufferObject::AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)
{
    VkVertexInputBindingDescription vibd = {};
    vibd.binding = binding;
    vibd.stride = stride;
    vibd.inputRate = inputRate;
    mBindings.push_back(vibd);
}

///////////////////////////////////////////////////////////////////////////////

void VertexBufferObject::AddAttribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format)
{
    VkVertexInputAttributeDescription viad = {};
    viad.binding = binding;
    viad.location = location;
    viad.offset = offset;
    viad.format = format;
    mAttributes.push_back(viad);
}

///////////////////////////////////////////////////////////////////////////////

VkPipelineVertexInputStateCreateInfo VertexBufferObject::CreatePipelineState() const
{
    assert(mBindings.size() > 0);
    assert(mAttributes.size() > 0);
    VkPipelineVertexInputStateCreateInfo visci = {};
    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    visci.pNext = nullptr;
    visci.vertexBindingDescriptionCount = (uint32_t)mBindings.size();
    visci.pVertexBindingDescriptions = &mBindings[0];
    visci.vertexAttributeDescriptionCount = (uint32_t)mAttributes.size();
    visci.pVertexAttributeDescriptions = &mAttributes[0];
    return visci;
}

///////////////////////////////////////////////////////////////////////////////
