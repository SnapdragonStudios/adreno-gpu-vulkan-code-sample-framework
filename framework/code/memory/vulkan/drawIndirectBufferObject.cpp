//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "drawIndirectBufferObject.hpp"

template<>
uint32_t DrawIndirectBuffer<Vulkan>::GetDrawCommandBytes() const
{
    switch (mIndirectBufferType) {
    case eType::Draw:
        return sizeof( VkDrawIndirectCommand );
    case eType::IndexedDraw:
        return sizeof( VkDrawIndexedIndirectCommand );
    case eType::MeshTasks:
        return sizeof( VkDrawMeshTasksIndirectCommandEXT );
        break;
    default:
        assert(0);
        return 0;
    }
}

