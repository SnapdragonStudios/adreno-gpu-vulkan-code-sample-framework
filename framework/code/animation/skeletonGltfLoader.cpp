//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "skeletonGltfLoader.hpp"
#include "skeletonData.hpp"
#include "mesh/meshLoader.hpp"
#include "system/os_common.h"

#ifdef GLM_FORCE_QUAT_DATA_WXYZ
#error "code currently expects glm::quat to be xyzw"
#endif

SkeletonGltfProcessor::SkeletonGltfProcessor() {}
SkeletonGltfProcessor::~SkeletonGltfProcessor() {}


bool SkeletonGltfProcessor::operator()(const tinygltf::Model& ModelData)
{
    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    std::vector<SkeletonNodeData> nodes;
    std::vector<const SkeletonNodeData*> rootNodes;
    nodes.reserve(ModelData.nodes.size());  // ESSENTIAL that we reserve this to hold all the nodes up-front so there are no re-allocations as we add nodes.
    rootNodes.reserve(SceneData.nodes.size());
    std::vector<const SkeletonNodeData*> nodesByIdx;
    nodesByIdx.resize(ModelData.nodes.size(), nullptr);

    for (int rootNodeIdx : SceneData.nodes)
    {
        // Recursive lambda to add all nodes under a given parent node to the nodes vector and return a pointer to the head node.  addNodeAndChildrenLambdaRef is C++ foo to do lambda recursion.
        const auto addNodeAndChildrenLambda = [&ModelData, &nodes, &nodesByIdx](int rootNodeIdx) -> const SkeletonNodeData* {

            auto addNodeAndChildrenLambdaImpl = [&ModelData, &nodes, &nodesByIdx](const SkeletonNodeData* parent, int nodeIdx, bool addChildren, auto& addNodeAndChildrenLambdaRef) -> const SkeletonNodeData* {

                int childStartOffset = 0;
                SkeletonNodeData* pNode = nullptr;

                if (addChildren == false)
                {
                    // Add this node
                    nodes.emplace_back(SkeletonNodeData{ nodeIdx });
                    pNode = &nodes.back();
                    pNode->m_Parent = parent;
                    nodesByIdx[nodeIdx] = pNode;
                }
                else
                {
                    int numChildren = (int)ModelData.nodes[nodeIdx].children.size();

                    if (numChildren > 0)
                    {
                        // Got our node (kinda awful, could be passed in rather than looked up)
                        pNode = const_cast<SkeletonNodeData*>(nodesByIdx[nodeIdx]);

                        // Add the children (will be sequential, after all our siblings)
                        assert(pNode->m_Children == nullptr);
                        pNode->m_Children = &nodes.back() + 1;
                        pNode->m_NumChildren = numChildren;

                        for (int childIdx : ModelData.nodes[nodeIdx].children)
                            addNodeAndChildrenLambdaRef(pNode, childIdx, false, addNodeAndChildrenLambdaRef);

                        // Allow the children to add their children
                        for (int childIdx : ModelData.nodes[nodeIdx].children)
                            addNodeAndChildrenLambdaRef(nullptr, childIdx, true, addNodeAndChildrenLambdaRef);
                    }
                }
                
                return pNode;
            };

            // First call adds the node
            auto* pNode = addNodeAndChildrenLambdaImpl(nullptr, rootNodeIdx, false/*add children*/, addNodeAndChildrenLambdaImpl);
            // Second call adds all the children of the node (and does the recursion down the tree)
            addNodeAndChildrenLambdaImpl(nullptr, rootNodeIdx, true/*add children*/, addNodeAndChildrenLambdaImpl);
            return pNode;
        };

        rootNodes.push_back(addNodeAndChildrenLambda(rootNodeIdx));
    }

    // Fill in the transforms for all the nodes.  Use the RecurseModelNodes helper
    MeshLoader::RecurseModelNodes(ModelData, SceneData.nodes, [this, &nodesByIdx](const tinygltf::Model& ModelData, const MeshLoader::NodeTransform& WorldTransform, const tinygltf::Node& NodeData) -> bool {
        size_t NodeIdx = &NodeData - &ModelData.nodes[0];
        if (NodeIdx < 0 || NodeIdx >= ModelData.nodes.size())
        {
            assert(0);
            return false;
        }
        const_cast<SkeletonNodeData*>(nodesByIdx[NodeIdx])->m_LocalTransform = MeshLoader::NodeTransform(NodeData);
        return true;
    });


    if (nodes.size() != ModelData.nodes.size())
    {
        assert(0);
        return false;
    }

    SkeletonData skeletonData {std::move(nodes), std::move(nodesByIdx), std::move(rootNodes)};

    m_skeletons.emplace_back(std::move(skeletonData));

    return true;
}
