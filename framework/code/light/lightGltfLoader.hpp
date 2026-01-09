//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <memory>

// forward declarations
class LightList;

namespace tinygltf {
    class Model;
};

/// Gltf model processor for lighting data (passed in to @MeshLoader::LoadGltf) 
/// Creates and populates a LightList based on the lights inside the gltf model.
/// @ingroup Light
class LightGltfProcessor
{
    LightGltfProcessor& operator=(const LightGltfProcessor&) = delete;
    LightGltfProcessor(const LightGltfProcessor&) = delete;
public:
    /// @brief Constructor
    /// @param defaultIntensityCutoff light intensity to be treated as zero (used to calculate light range)
    /// @param maxDirectionals maximum number of directional lights to load
    LightGltfProcessor(float defaultIntensityCutoff, uint32_t maxDirectionals) : m_defaultIntensityCutoff(defaultIntensityCutoff), m_maxDirectionals(maxDirectionals) { }
    bool operator()(const tinygltf::Model& ModelData);

    std::unique_ptr<LightList> m_lightList;
    const float m_defaultIntensityCutoff;
    const uint32_t m_maxDirectionals;
};
