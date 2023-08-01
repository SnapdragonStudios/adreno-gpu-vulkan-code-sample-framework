//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <vector>

// forward declarations
struct CameraData;

namespace tinygltf {
    class Model;
};

/// Gltf model processor for camera data (passed in to @MeshLoader::LoadGltf) 
/// Creates and populates a vector of @Camera based on the camras inside the gltf model.
/// @ingroup Camera
class CameraGltfProcessor
{
    CameraGltfProcessor& operator=(const CameraGltfProcessor&) = delete;
    CameraGltfProcessor(const CameraGltfProcessor&) = delete;
public:
    CameraGltfProcessor();
    ~CameraGltfProcessor();
    bool operator()(const tinygltf::Model& ModelData);
    std::vector<CameraData> m_cameras;
};
