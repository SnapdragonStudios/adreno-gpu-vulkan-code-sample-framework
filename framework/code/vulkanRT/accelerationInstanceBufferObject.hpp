//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "memory/vulkan/bufferObject.hpp"
#include <vector>
#include <span>

// forward declarations

// Class
class AccelerationInstanceBufferObject : public BufferT<Vulkan>
{
    AccelerationInstanceBufferObject& operator=(const AccelerationInstanceBufferObject&) = delete;
    AccelerationInstanceBufferObject(const AccelerationInstanceBufferObject&) = delete;
public:
    AccelerationInstanceBufferObject() = default;
    AccelerationInstanceBufferObject(AccelerationInstanceBufferObject&&) noexcept;
    AccelerationInstanceBufferObject& operator=(AccelerationInstanceBufferObject&&) noexcept;
    virtual ~AccelerationInstanceBufferObject() {};

    bool Initialize(MemoryManager* pManager, size_t numInstances);

    // destroy buffer and leave in a state where it could be re-initialized
    virtual void Destroy() override;

    // get number of instances allocated
    size_t GetNumInstances() const { return mNumInstances; }
    
    // Map this buffer to the cpu and return a guard object (automatically unmaps)
    MapGuard<Vulkan, VkAccelerationStructureInstanceKHR> Map()
    {
        assert(mManager);
        return { *this, mManager->Map<VkAccelerationStructureInstanceKHR>(mAllocatedBuffer) };
    }

protected:
    size_t mNumInstances = 0;
};
