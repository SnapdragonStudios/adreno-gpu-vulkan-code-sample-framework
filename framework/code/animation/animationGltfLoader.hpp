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
class AnimationData;

namespace tinygltf {
    class Model;
};

/// Gltf model processor for animation data (passed in to @MeshLoader::LoadGltf)
/// Creates and populates a vector of @AnimationData based on the animations inside the gltf model.
/// @ingroup Animation
class AnimationGltfProcessor
{
    AnimationGltfProcessor& operator=(const AnimationGltfProcessor&) = delete;
    AnimationGltfProcessor(const AnimationGltfProcessor&) = delete;
public:
    AnimationGltfProcessor();
    ~AnimationGltfProcessor();
    bool operator()(const tinygltf::Model& ModelData);
    std::vector<AnimationData> m_animations;
};
