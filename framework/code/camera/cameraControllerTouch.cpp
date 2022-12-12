//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "cameraControllerTouch.hpp"

static const float cMouseRotSpeed = 0.1f;
static const float cMouseMoveSpeeed = 0.001f;

// Helpers
constexpr glm::vec3 cVecUp = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 cVecRight = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 cVecForward = glm::vec3(0.0f, 0.0f, -1.0f);


//-----------------------------------------------------------------------------

CameraControllerTouch::CameraControllerTouch() : CameraControllerBase()
    , m_LastMovementTouchPosition(0.0f)
    , m_CurrentMovementTouchPosition(0.0f)
    , m_LastLookaroundTouchPosition(0.0f)
    , m_CurrentLookaroundTouchPosition(0.0f)
    , m_MovementTouchId(-1)
    , m_LookaroundTouchId(-1)
{
}

//-----------------------------------------------------------------------------

CameraControllerTouch::~CameraControllerTouch()
{}

//-----------------------------------------------------------------------------

bool CameraControllerTouch::Initialize(uint32_t width, uint32_t height)
{
    return CameraControllerBase::Initialize(width, height);
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
        auto angleChange = mouseDiff * frameTime * m_RotateSpeed;

        m_LastLookaroundTouchPosition = m_CurrentLookaroundTouchPosition;
        rot = glm::angleAxis(angleChange.y, cVecForward) * rot * glm::angleAxis(angleChange.x, cVecUp);
    }

    if (m_MovementTouchId != -1)
    {
        auto mouseDiff = m_LastMovementTouchPosition - m_CurrentMovementTouchPosition;
        auto directionChange = mouseDiff * frameTime * m_MoveSpeed;

        position -= (cVecRight * directionChange.x) * rot;
        position += (cVecForward * directionChange.y) * rot;
    }
}

