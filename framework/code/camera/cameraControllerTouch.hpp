//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"
#include "cameraController.hpp"

///
/// Basic camera controls for touchscreen.
/// Left side of screen to move, right side of screen to look around
/// @ingroup Camera
/// 
class CameraControllerTouch : public CameraControllerBase
{
    CameraControllerTouch(const CameraControllerTouch&) = delete;
    CameraControllerTouch& operator=(const CameraControllerTouch&) = delete;
public:
    CameraControllerTouch();
    ~CameraControllerTouch() override;

    bool Initialize(uint32_t width, uint32_t height) override;

    // Inputs
    //void    KeyDownEvent(uint32_t key) override {}
    //void    KeyUpEvent(uint32_t key) override {}
    void    TouchDownEvent(int iPointerID, float xPos, float yPos) override;
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos) override;
    void    TouchUpEvent(int iPointerID, float xPos, float yPos) override;

    /// Update the camera controller and modify the output position/rotation accordingly
    /// @param frameTime time in seconds since last Update.
    /// @param position in - current camera position, out - camera position after applying controller
    /// @param rot in - current camera rotation, out - camera rotation after applying controller
    void Update(float frameTime, glm::vec3& position, glm::quat& rot) override;

protected:
    glm::vec2   m_LastMovementTouchPosition;
    glm::vec2   m_CurrentMovementTouchPosition;
    glm::vec2   m_LastLookaroundTouchPosition;
    glm::vec2   m_CurrentLookaroundTouchPosition;

    int         m_MovementTouchId;
    int         m_LookaroundTouchId;
};
