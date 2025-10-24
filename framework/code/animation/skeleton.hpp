//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <span>
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

    const auto& GetTransforms() const           { return m_WorldTransforms; }
    const SkeletonData& GetSkeletonData() const { return m_SkeletonData; }

    /// Transform local matrices for a skeleton to world space (using the skeleton hierarchy)
    /// local and world can point to the same data if desired (skeleton is assumed to be a tree structure)
    void TransformLocalToWorld(const std::span<const glm::mat4> local, std::span<glm::mat4> world) const;

private:
    const SkeletonData&     m_SkeletonData;     //NOT owned, do not delete before deleting this class!
    std::vector<glm::mat4>  m_WorldTransforms;  // Current world transforms by nodeId order (same order as @SkeletonData::NodesById).
};
