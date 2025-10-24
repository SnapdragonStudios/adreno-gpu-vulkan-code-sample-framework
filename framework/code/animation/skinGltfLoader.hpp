//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <vector>

// forward declarations
class SkinData;

namespace tinygltf {
    class Model;
};

/// Gltf model processor for skeleton (node hierarchy) data (passed in to @MeshLoader::LoadGltf)
/// Creates and populates a @SkeletonData based on the nodes inside the gltf model.
/// @ingroup Animation
class SkinGltfProcessor
{
    SkinGltfProcessor& operator=(const SkinGltfProcessor&) = delete;
    SkinGltfProcessor(const SkinGltfProcessor&) = delete;
public:
    SkinGltfProcessor() noexcept;
    ~SkinGltfProcessor();
    bool operator()(const tinygltf::Model& ModelData);
    std::vector<SkinData> m_skins;
};
