//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <span>
#include <string>
#include <vector>
#include "system/glm_common.hpp"

class SkinData {
    friend class SkinGltfProcessor;
    SkinData(const SkinData&) = delete;
    SkinData& operator=(const SkinData&) = delete;
    SkinData(std::string name, std::vector<int>&& nodeIds, std::vector<glm::mat4>&& inverseBindMatrices) noexcept
        : Name(std::move(name))
        , NodeIds(std::move(nodeIds))
        , m_InverseBindMatrices(std::move(inverseBindMatrices))
    {}
public:
    SkinData(SkinData&&) noexcept = default;
    SkinData& operator=(SkinData&&) noexcept = default;

    size_t GetNumJoints() const { return NodeIds.size(); }
    const auto& GetJoints() const { return NodeIds; }

    const auto& GetInverseBindMatrices() const { return m_InverseBindMatrices; }

protected:
    std::string Name;
    std::vector<int> NodeIds;                       ///< gltf node indices (one per joint)
    std::vector<glm::mat4> m_InverseBindMatrices;   ///< one per NodeId (one per joint)
};
