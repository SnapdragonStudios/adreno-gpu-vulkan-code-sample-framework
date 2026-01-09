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
#include <memory>
#include "system/glm_common.hpp"
#include <span>


/// Skeleton node data container.  All the data needed to describe one nodes worth of data in a skeleton.
/// @ingroup Animation
class SkeletonNodeData
{
private:
    friend class SkeletonGltfProcessor;
    friend class SkeletonData;
    SkeletonNodeData(int nodeId) : m_NodeId(nodeId) {};
    SkeletonNodeData(const SkeletonNodeData&) = delete;
    SkeletonNodeData& operator=(const SkeletonNodeData&) = delete;
    SkeletonNodeData& operator=(SkeletonNodeData&&) = default;
public:
    SkeletonNodeData(SkeletonNodeData&&) = default; // not ideal that this is public but emplace_back needs this

    std::span<const SkeletonNodeData> Children() const  { return { m_Children, m_NumChildren }; }
    const SkeletonNodeData* const Parent() const        { return const_cast<const SkeletonNodeData* const>(m_Parent); }
    auto NodeId() const                                 { return m_NodeId;}
    auto& LocalTransform() const                        { return m_LocalTransform; }

private:
    const SkeletonNodeData*     m_Parent = nullptr;     ///< pointer to parent node (or nullptr if this is a top node)
    SkeletonNodeData*           m_Children = nullptr;   ///< pointer to array of children (or nullptr if this is a bottom/leaf node)
    uint32_t                    m_NumChildren = 0;      ///< number of children (in array)
    int                         m_NodeId;               ///< nodeId (from gltf file)
    glm::mat4                   m_LocalTransform{1.0f}; ///< local transform of this node
};

/// Skeleton data container.  All the data needed to describe a single skeleton hierarchy containing a set of nodes.
/// @ingroup Animation
class SkeletonData
{
public:
    SkeletonData(const SkeletonData&) = delete;
    SkeletonData& operator=(const SkeletonData&) = delete;
    SkeletonData(SkeletonData&&) = default;
    SkeletonData& operator=(SkeletonData&&) = default;
    SkeletonData(std::vector<SkeletonNodeData>&& Nodes, std::vector<const SkeletonNodeData*>&& NodesById, std::vector<const SkeletonNodeData*>&& rootNodes);

    const SkeletonNodeData* GetNodeById(uint32_t nodeId) const { return m_NodesById[nodeId]; }
    const auto& GetRootNodes() const { return m_RootNodes; }

protected:
    friend class Skeleton;
    friend class SkeletonGltfProcessor;
    std::vector<const SkeletonNodeData*>        m_NodesById;     ///< nodes ordered by NodeId
    std::vector<const SkeletonNodeData*>        m_RootNodes;     ///< root nodes (no set order)
    std::vector<SkeletonNodeData>               m_Nodes;       ///< Node data hierarchy (lookup start node via NodesById)
};
