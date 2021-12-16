// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "system/glm_common.hpp"

///
/// Basic camera controls for touchscreen.
/// Left side of screen to move, right side of screen to look around
/// @ingroup Camera
/// 
class CameraControllerTouch
{
    CameraControllerTouch(const CameraControllerTouch&) = delete;
    CameraControllerTouch& operator=(const CameraControllerTouch&) = delete;
public:
    CameraControllerTouch();

    bool    Initialize(uint32_t width, uint32_t height);

    // Inputs
    void    KeyDownEvent(uint32_t key) {}
    void    KeyUpEvent(uint32_t key) {}
    void    TouchDownEvent(int iPointerID, float xPos, float yPos);
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos);
    void    TouchUpEvent(int iPointerID, float xPos, float yPos);

    void    SetSize(uint32_t width, uint32_t height);
    void    Update(float frameTime, glm::vec3& position, glm::quat& rot);

protected:
    glm::vec2 m_ScreenSize;
    glm::vec2 m_LastMovementTouchPosition;
    glm::vec2 m_CurrentMovementTouchPosition;
    glm::vec2 m_LastLookaroundTouchPosition;
    glm::vec2 m_CurrentLookaroundTouchPosition;

    int m_MovementTouchId;
    int m_LookaroundTouchId;
};
