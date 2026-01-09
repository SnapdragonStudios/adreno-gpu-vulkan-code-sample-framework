//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "skinData.hpp"

#include <span>
#include <vector>
#include "system/glm_common.hpp"


class Skin
{
    Skin(const Skin&) = delete;
    Skin& operator=(const Skin&) = delete;

    SkinData m_SkinData;
    std::vector<glm::mat4> m_SkinTransformMatrixes;
public:
    Skin(SkinData&& d) noexcept;
    typedef decltype(m_SkinTransformMatrixes)::value_type tSkinMatrix;

    const auto& GetSkinTransformMatrices() const { return m_SkinTransformMatrixes; }

    void UpdateSkinMatrixes(const std::span<const tSkinMatrix> sceneMatrixs);
};
