//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "accelerationInstanceBufferObject.hpp"

///////////////////////////////////////////////////////////////////////////////

AccelerationInstanceBufferObject::AccelerationInstanceBufferObject(AccelerationInstanceBufferObject&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

AccelerationInstanceBufferObject& AccelerationInstanceBufferObject::operator=(AccelerationInstanceBufferObject&& other) noexcept
{
    if (this != &other)
    {
        Buffer<Vulkan>::operator=(std::move(other));
        mNumInstances = other.mNumInstances;
        other.mNumInstances = 0;
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

bool AccelerationInstanceBufferObject::Initialize(MemoryManager* pManager, size_t numInstances)
{
    mNumInstances = numInstances;

    return Buffer<Vulkan>::Initialize(pManager, (VkDeviceSize)(sizeof(VkAccelerationStructureInstanceKHR) * numInstances), BufferUsageFlags::AccelerationStructureBuild | BufferUsageFlags::ShaderDeviceAddress, MemoryUsage::CpuToGpu);
}

///////////////////////////////////////////////////////////////////////////////

void AccelerationInstanceBufferObject::Destroy()
{
    mNumInstances = 0;
    Buffer<Vulkan>::Destroy();
}

///////////////////////////////////////////////////////////////////////////////
