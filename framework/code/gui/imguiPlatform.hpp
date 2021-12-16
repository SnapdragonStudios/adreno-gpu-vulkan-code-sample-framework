// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "imguiBase.hpp"

// forward declarations
class Vulkan;

///
/// @brief Platform specific imgui class declaration
/// Implementation is in the Windows / Android (or whatever) subdirectory.
/// Does the input/screen (but not rendering) for whichever platform we are running on
/// @ingroup GUI
///
class GuiImguiPlatform : public GuiImguiBase
{
public:
    GuiImguiPlatform();
    ~GuiImguiPlatform();
    bool Initialize(uintptr_t windowHandle) override = 0;
    bool Initialize(uintptr_t windowHandle, uint32_t deviceWidth, uint32_t deviceHeight, uint32_t renderWidth, uint32_t renderHeight);
    void Update() override;

    bool TouchDownEvent(int iPointerID, float xPos, float yPos) override;
    bool TouchMoveEvent(int iPointerID, float xPos, float yPos) override;
    bool TouchUpEvent(int iPointerID, float xPos, float yPos) override;
};

