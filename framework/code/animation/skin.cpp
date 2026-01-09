//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "skin.hpp"

Skin::Skin(SkinData&& d) noexcept : m_SkinData(std::move(d))
{
    m_SkinTransformMatrixes.resize(m_SkinData.GetNumJoints(), glm::identity<tSkinMatrix>());
}

void Skin::UpdateSkinMatrixes(const std::span<const tSkinMatrix> sceneMatrixs)
{
    assert(m_SkinTransformMatrixes.size() == m_SkinData.GetNumJoints());

    const auto& inverseBindMatrices = m_SkinData.GetInverseBindMatrices();
    for (uint32_t i = 0; int jointNodeId : m_SkinData.GetJoints())
        m_SkinTransformMatrixes[i++] = sceneMatrixs[jointNodeId] * inverseBindMatrices[i];//glm::transpose(m_SceneTransformMatrixes[jointNodeId]) * inverseBindMatrices[i];
}
