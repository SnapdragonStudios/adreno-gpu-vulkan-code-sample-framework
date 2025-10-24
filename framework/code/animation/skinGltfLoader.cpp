//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "skinGltfLoader.hpp"
#include "skinData.hpp"
#include "mesh/meshLoader.hpp"
#include "system/os_common.h"
#include <algorithm>
#include <cassert>


SkinGltfProcessor::SkinGltfProcessor() noexcept {}
SkinGltfProcessor::~SkinGltfProcessor() {}


bool SkinGltfProcessor::operator()(const tinygltf::Model& ModelData)
{
    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    m_skins.reserve(ModelData.skins.size());
    for (const auto& skin : ModelData.skins)
    {

        // Grab a pointer to the inverse bind matrix data (one per joint in the skin).
        const tinygltf::Accessor& inverseBindMatricesAccessorData = ModelData.accessors[skin.inverseBindMatrices];
        const size_t inverseBindMatricesDataItemCount = inverseBindMatricesAccessorData.count;
        const auto& ibmBuffer = ModelData.bufferViews[inverseBindMatricesAccessorData.bufferView];

        if ((ibmBuffer.byteStride != 4 && ibmBuffer.byteStride != 0/*packed/default*/) || (ibmBuffer.byteOffset & 3) != 0)
        {
            LOGE("Error reading time data for gltf skin inverse bind matrices\"%s\" (expecting contiguous array of aligned matrix data)", skin.name.c_str());
            return false;
        }
        std::span<glm::mat4> ibmDataSrcPtr{(glm::mat4*)&ModelData.buffers[ibmBuffer.buffer].data[ibmBuffer.byteOffset + inverseBindMatricesAccessorData.byteOffset], inverseBindMatricesDataItemCount};

        m_skins.push_back(SkinData{skin.name, std::vector<int>{skin.joints}/*copy*/, std::vector<glm::mat4>{ibmDataSrcPtr.begin(), ibmDataSrcPtr.end()}});
    }
    return true;
}
