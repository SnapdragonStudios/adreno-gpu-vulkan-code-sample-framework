//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <vector>
#include "system/glm_common.hpp"

// forward declarations
class SkeletonData;


/// Skeleton class, contains the runtime representation of a skeleton
/// @ingroup Animation
class Skeleton
{
    Skeleton& operator=(const Skeleton&) = delete;
    Skeleton(const Skeleton&) = delete;
public:
    Skeleton(const SkeletonData&);
    ~Skeleton();

    const auto GetTransforms() const            { return m_WorldTransforms; }
    const SkeletonData& GetSkeletonData() const { return m_SkeletonData; }

private:
    const SkeletonData&     m_SkeletonData; //NOT owned, do not delete before deleting this class!
    std::vector<glm::mat4>  m_WorldTransforms;  // Current world transforms by nodeId order (same order as @SkeletonData::NodesById).
};
