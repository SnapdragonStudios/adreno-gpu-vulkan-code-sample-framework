//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "cameraGltfLoader.hpp"
#include "cameraData.hpp"
#include "mesh/meshLoader.hpp"

#ifdef GLM_FORCE_QUAT_DATA_WXYZ
#error "code currently expects glm::quat to be xyzw"
#endif

CameraGltfProcessor::CameraGltfProcessor() {}
CameraGltfProcessor::~CameraGltfProcessor() {}

bool CameraGltfProcessor::operator()(const tinygltf::Model& ModelData)
{
    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];
    auto& cameras = m_cameras;

    // Go through all the scene nodes, grabbing all the cameras
    bool success = MeshLoader::RecurseModelNodes(ModelData, SceneData.nodes, [&cameras](const tinygltf::Model& ModelData, const MeshLoader::NodeTransform& Transform, const tinygltf::Node& NodeData) -> bool {

        const static std::string perspectiveTypeName("perspective");
        const static std::string orthographicTypeName("orthographic");

        if (NodeData.camera >= 0)
        {
            size_t NodeIdx = &NodeData - &ModelData.nodes[0];
            if (NodeIdx < 0 || NodeIdx >= ModelData.nodes.size())
                NodeIdx = -1;

            const tinygltf::Camera& gltfCamera = ModelData.cameras[NodeData.camera];
            if (gltfCamera.type == perspectiveTypeName)
            {
                CameraData cameraData{};
                cameraData.Position = { Transform.Matrix[3][0], Transform.Matrix[3][1], Transform.Matrix[3][2] };
                glm::mat3 rotMatrix = glm::mat3(Transform);
                rotMatrix[0] = glm::normalize(rotMatrix[0]);    // remove scale from rotation matrix
                rotMatrix[1] = glm::normalize(rotMatrix[1]);
                rotMatrix[2] = glm::normalize(rotMatrix[2]);
                cameraData.Orientation = glm::quat(rotMatrix);
                cameraData.NodeId = (int)NodeIdx;
                //gltfCamera.perspective.yfov;
                cameras.emplace_back( cameraData );
            }
            else if (gltfCamera.type == orthographicTypeName)
            {
                //TODO: support ortho
            }
        }
        return true;
        });

    if (!success)
        return false;

    return true;
}
