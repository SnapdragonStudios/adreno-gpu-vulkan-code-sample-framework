//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "drawableLoader.hpp"
#include "drawable.hpp"
#include "mesh/meshIntermediate.hpp"
#include "system/os_common.h"
#include <cassert>
#include <limits>


DrawableLoaderBase::MeshStatistics DrawableLoaderBase::GatherStatistics(const std::span<MeshObjectIntermediate> meshObjects)
{
    MeshStatistics stats;
    stats.totalVerts = 0;
    stats.boundingBoxMin = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    stats.boundingBoxMax = glm::vec3(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
    for (const auto& mesh : meshObjects)
    {
        stats.totalVerts += mesh.m_VertexBuffer.size();

        for (const auto& OneFat : mesh.m_VertexBuffer)
        {
            stats.boundingBoxMin[0] = std::min(stats.boundingBoxMin[0], OneFat.position[0]);
            stats.boundingBoxMin[1] = std::min(stats.boundingBoxMin[1], OneFat.position[1]);
            stats.boundingBoxMin[2] = std::min(stats.boundingBoxMin[2], OneFat.position[2]);

            stats.boundingBoxMax[0] = std::max(stats.boundingBoxMax[0], OneFat.position[0]);
            stats.boundingBoxMax[1] = std::max(stats.boundingBoxMax[1], OneFat.position[1]);
            stats.boundingBoxMax[2] = std::max(stats.boundingBoxMax[2], OneFat.position[2]);
        }
    }
    if (stats.totalVerts == 0)
    {
        stats.boundingBoxMin = glm::vec3(0.0f, 0.0f, 0.0f);
        stats.boundingBoxMax = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return stats;
}

void DrawableLoaderBase::PrintStatistics(const std::span<MeshObjectIntermediate> meshObjects)
{
    MeshStatistics stats = GatherStatistics(meshObjects);
    LOGI("Model total Vertices: %zu", stats.totalVerts);
    LOGI("Model Bounding Box: (%0.2f, %0.2f, %0.2f) -> (%0.2f, %0.2f, %0.2f)", stats.boundingBoxMin[0], stats.boundingBoxMin[1], stats.boundingBoxMin[2], stats.boundingBoxMax[0], stats.boundingBoxMax[1], stats.boundingBoxMax[2]);
    LOGI("Model Extent: (%0.2f, %0.2f, %0.2f)", stats.boundingBoxMax[0] - stats.boundingBoxMin[0], stats.boundingBoxMax[1] - stats.boundingBoxMin[1], stats.boundingBoxMax[2] - stats.boundingBoxMin[2]);
}
