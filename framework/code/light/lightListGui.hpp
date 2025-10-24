//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "lightList.hpp"

/// @brief Implements GUI for editing lights
class LightListGui
{
public:
    LightListGui(const LightList& original);

    // Display the given light list in the gui and update as needed.
    // Does not add or remove lights from lightList, merely updates the contained values.
    // Assumes (REQUIRES) the lightList is the same one as 'm_LightListOriginal' and has not had items added or removed such that the lightList mismatches that in m_LightListOriginal.
    // @returns true if lightList was modified.
    bool UpdateGui(LightList& lightList);

    // Copy of original (gltf) lights
    const LightList  m_LightListOriginal;

protected:
};

