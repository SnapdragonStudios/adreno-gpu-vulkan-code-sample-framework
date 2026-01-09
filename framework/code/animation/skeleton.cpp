//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "skeleton.hpp"
#include "skeletonData.hpp"

Skeleton::Skeleton(const SkeletonData& skeletonData)
    : m_SkeletonData(skeletonData)
{
    m_WorldTransforms.resize( m_SkeletonData.m_NodesById.size(), glm::identity<glm::mat4>() );

    // iterate through node hierarchy to calculate the world transforms.
    for (const auto* rootNode : m_SkeletonData.m_RootNodes)
    {
        const auto TransformTree = [this](auto& rootNode, const auto& ParentTransform) -> void {
            auto TransformTreeImpl = [this](auto& rootNode, const auto& ParentTransform, auto& TransformTreeRef) -> void
            {
                auto WorldTransform = ParentTransform * rootNode.LocalTransform();
                m_WorldTransforms[rootNode.NodeId()] = WorldTransform;
                for (auto& child : rootNode.Children())
                    TransformTreeRef(child, WorldTransform, TransformTreeRef);
            };
            TransformTreeImpl(rootNode, ParentTransform, TransformTreeImpl);
        };
        TransformTree(*rootNode, glm::identity<glm::mat4>());
    }
}

void Skeleton::TransformLocalToWorld(const std::span<const glm::mat4> local, std::span<glm::mat4> world) const
{
    assert(local.size() == world.size());
    assert(world.size() == m_SkeletonData.m_NodesById.size());

    // iterate through node hierarchy to calculate the world transforms.
    for (const auto* rootNode : m_SkeletonData.m_RootNodes)
    {
        const auto TransformTree = [local, world](auto& node, const auto& ParentTransform) -> void {
            auto TransformTreeImpl = [local, world](auto& node, const auto& ParentTransform, auto& TransformTreeRef) -> void
            {
                const auto nodeId = node.NodeId();
                auto WorldTransform = ParentTransform * local[nodeId];
                world[nodeId] = WorldTransform;
                for (auto& child : node.Children())
                    TransformTreeRef(child, WorldTransform, TransformTreeRef);
            };
            TransformTreeImpl(node, ParentTransform, TransformTreeImpl);
        };
        TransformTree(*rootNode, glm::identity<glm::mat4>());
    }

}

Skeleton::~Skeleton()
{
}


