// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "system/glm_common.hpp"

///
/// Basic camera controller
/// Mouse (or touch) to look around and keyboard 'wasd' to move.
/// @ingroup Camera
/// 
class CameraController
{
    CameraController(const CameraController&) = delete;
    CameraController& operator=(const CameraController&) = delete;
public:
    CameraController();

    bool    Initialize(uint32_t width, uint32_t height);

    // Inputs
    void    KeyDownEvent(uint32_t key);
    void    KeyUpEvent(uint32_t key);
    void    TouchDownEvent(int iPointerID, float xPos, float yPos);
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos);
    void    TouchUpEvent(int iPointerID, float xPos, float yPos);

    void    SetSize(uint32_t width, uint32_t height);
    void    Update(float frameTime, glm::vec3& position, glm::quat& rot);

protected:
    enum KeysDownBits {
        eNone = 0,
        eForward = 0x1,
        eBackward = 0x2,
        eLeft = 0x4,
        eRight = 0x8
    };
    static enum KeysDownBits KeyToBits(uint32_t key);
protected:
    glm::vec2 m_ScreenSize;;
    glm::vec2 m_LastMousePosition;
    glm::vec2 m_CurrentMousePosition;
    bool m_touchDown = false;

    KeysDownBits m_KeysDown = KeysDownBits::eNone;
    typedef std::underlying_type_t<KeysDownBits> KeysDownBits_base;
};

