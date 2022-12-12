//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @defgroup GUI
/// Game (onscreen) GUI functionality.

#include <vulkan/vulkan.h>

///
/// @brief (Pure virtual) base for gui implementations (gui solution agnostic).
/// @ingroup GUI
///
class Gui
{
public:
    virtual ~Gui() = 0;
    virtual bool Initialize(uintptr_t windowHandle, uint32_t renderWidth, uint32_t renderHeight) = 0;
    virtual void Update() = 0;
    virtual VkCommandBuffer Render(uint32_t frameIdx, VkFramebuffer frameBuffer) = 0;
    virtual void Render(VkCommandBuffer cmdBuffer) = 0;

    /// @returns True if the GUI is capturing mouse events (and so they shouldnt be sent to our application code)
    virtual bool WantCaptureMouse() const = 0;
    /// @returns True if the GUI is capturing keyboard events (and so they shouldnt be sent to our application code)
    virtual bool WantCaptureKeyboard() const = 0;

    /// Touch(screen) events passed from the application.
    virtual bool TouchDownEvent(int iPointerID, float xPos, float yPos) = 0;
    virtual bool TouchMoveEvent(int iPointerID, float xPos, float yPos) = 0;
    virtual bool TouchUpEvent(int iPointerID, float xPos, float yPos) = 0;
};

inline Gui::~Gui() {}
