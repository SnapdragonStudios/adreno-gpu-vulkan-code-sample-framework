// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "cameraControllerTouch.hpp"

static const float cMouseRotSpeed = 0.1f;
static const float cMouseMoveSpeeed = 0.1f;

// Helpers
constexpr glm::vec3 cVecUp = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 cVecRight = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 cVecForward = glm::vec3(0.0f, 0.0f, -1.0f);


//-----------------------------------------------------------------------------

CameraControllerTouch::CameraControllerTouch()
    : m_ScreenSize(1920.f, 1080.f)
    , m_LastMovementTouchPosition(0.0f)
    , m_CurrentMovementTouchPosition(0.0f)
    , m_LastLookaroundTouchPosition(0.0f)
    , m_CurrentLookaroundTouchPosition(0.0f)
    , m_MovementTouchId(-1)
    , m_LookaroundTouchId(-1)
{
}

//-----------------------------------------------------------------------------

bool CameraControllerTouch::Initialize(uint32_t width, uint32_t height)
{
    SetSize(width, height);
    return true;
}

//-----------------------------------------------------------------------------

void CameraControllerTouch::SetSize(uint32_t width, uint32_t height)
{
    m_ScreenSize = glm::vec2( width, height );
}

//-----------------------------------------------------------------------------

void CameraControllerTouch::TouchDownEvent(int iPointerID, float xPos, float yPos)
{
    if (xPos >= m_ScreenSize.x * 0.5f && m_LookaroundTouchId == -1)
    {
        m_LookaroundTouchId = iPointerID;
        m_LastLookaroundTouchPosition = { xPos, yPos };
        m_CurrentLookaroundTouchPosition = m_LastLookaroundTouchPosition;
    }
    else if (xPos < m_ScreenSize.x * 0.5f && m_MovementTouchId == -1)
    {
        m_MovementTouchId = iPointerID;
        m_LastMovementTouchPosition = { xPos, yPos };
        m_CurrentMovementTouchPosition = m_LastMovementTouchPosition;
    }
}

//-----------------------------------------------------------------------------

void CameraControllerTouch::TouchMoveEvent(int iPointerID, float xPos, float yPos)
{
    if (iPointerID == m_LookaroundTouchId)
    {
        m_CurrentLookaroundTouchPosition = { xPos, yPos };
    }
    else if (iPointerID == m_MovementTouchId)
    {
        m_CurrentMovementTouchPosition = { xPos, yPos };
    }
}

//-----------------------------------------------------------------------------

void CameraControllerTouch::TouchUpEvent(int iPointerID, float xPos, float yPos)
{
    if (iPointerID == m_LookaroundTouchId)
    {
        m_LookaroundTouchId = -1;
        m_CurrentLookaroundTouchPosition = { xPos, yPos };
    }
    else if (iPointerID == m_MovementTouchId)
    {
        m_MovementTouchId = -1;
        m_CurrentMovementTouchPosition = { xPos, yPos };
    }
}

//-----------------------------------------------------------------------------

void CameraControllerTouch::Update(float frameTime, glm::vec3& position, glm::quat& rot)
{
    if (m_LookaroundTouchId != -1)
    {
        auto mouseDiff = m_LastLookaroundTouchPosition - m_CurrentLookaroundTouchPosition;
        auto angleChange = mouseDiff * frameTime * cMouseRotSpeed;

        m_LastLookaroundTouchPosition = m_CurrentLookaroundTouchPosition;
        rot = glm::angleAxis(angleChange.y, cVecRight) * rot * glm::angleAxis(angleChange.x, cVecUp);
    }

    if (m_MovementTouchId != -1)
    {
        auto mouseDiff = m_LastMovementTouchPosition - m_CurrentMovementTouchPosition;
        auto directionChange = mouseDiff * frameTime * cMouseMoveSpeeed;

        position -= (cVecRight * directionChange.x) * rot;
        position += (cVecForward * directionChange.y) * rot;
    }
}

