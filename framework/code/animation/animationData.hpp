//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <string>
#include <vector>
#include "system/glm_common.hpp"

/// Data describing one frame of animation data for a single node.
/// @ingroup Animation
struct AnimationFrameData {
    glm::vec3 Translation = { 0.0f,0.0f,0.0f };
    glm::quat Rotation = glm::identity<glm::quat>();
    glm::vec3 Scale = {1.0f,1.0f,1.0f};
    float Timestamp;
};

/// Animation node data container.  All the data needed to describe one nodes worth of data for a single skeletal animation.
/// @ingroup Animation
struct AnimationNodeData
{
    AnimationNodeData(AnimationNodeData&&) noexcept = default;
    AnimationNodeData& operator=(AnimationNodeData&&) noexcept = default;
    AnimationNodeData(const AnimationNodeData&) = delete;
    AnimationNodeData& operator=(const AnimationNodeData&) = delete;
    AnimationNodeData( std::vector<AnimationFrameData>&& frames, uint32_t NodeId ) noexcept : Frames( std::move( frames ) ), NodeId( NodeId ) {}
    
    std::vector<AnimationFrameData> Frames;     ///< Frames of animation data associated with this node (for a single animation)
    uint32_t NodeId;                            ///< gltf node index
};

/// Animation data container.  All the data needed to describe a single skeletal animation on a set of nodes.
/// @ingroup Animation
class AnimationData
{
    AnimationData(const AnimationData&) = delete;
    AnimationData& operator=(const AnimationData&) = delete;
public:
    AnimationData(std::string name, std::vector<AnimationNodeData>&& nodes) : Name(std::move(name)), Nodes(std::move(nodes)) {}
    AnimationData(AnimationData&&) = default;
    AnimationData& operator=(AnimationData&&) = default;

    const auto& GetNodes() const { return Nodes; }
protected:
    std::string Name;
    std::vector<AnimationNodeData> Nodes;
};

