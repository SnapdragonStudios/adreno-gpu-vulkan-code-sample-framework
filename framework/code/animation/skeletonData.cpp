//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "skeletonData.hpp"

SkeletonData::SkeletonData(std::vector<SkeletonNodeData>&& Nodes, std::vector<const SkeletonNodeData*>&& NodesById, std::vector<const SkeletonNodeData*>&& rootNodes)
    : m_NodesById(std::move(NodesById)), m_RootNodes(std::move(rootNodes)), m_Nodes(std::move(Nodes))
{}
