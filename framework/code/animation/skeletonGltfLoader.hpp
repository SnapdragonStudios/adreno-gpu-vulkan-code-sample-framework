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
class SkeletonData;

namespace tinygltf {
    class Model;
};

/// Gltf model processor for skeleton (node hierarchy) data (passed in to @MeshLoader::LoadGltf)
/// Creates and populates a @SkeletonData based on the nodes inside the gltf model.
/// @ingroup Animation
class SkeletonGltfProcessor
{
    SkeletonGltfProcessor& operator=(const SkeletonGltfProcessor&) = delete;
    SkeletonGltfProcessor(const SkeletonGltfProcessor&) = delete;
public:
    SkeletonGltfProcessor();
    ~SkeletonGltfProcessor();
    bool operator()(const tinygltf::Model& ModelData);
    std::vector<SkeletonData> m_skeletons;
};
