//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "cameraControllerTouch.hpp"

static const float cMouseRotSpeed = 0.1f;
static const float cTouchMoveSpeedMultipler = 0.001f;

// Helpers
constexpr glm::vec3 cVecViewRight = glm::vec3( 1.0f, 0.0f, 0.0f );    // x-direction (vector pointing to right of screen)!
constexpr glm::vec3 cVecViewForward = glm::vec3( 0.0f, 0.0f, -1.0f ); // z-direction (vector pointing into screen)


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

void CameraControllerTouch::Update(float frameTime, glm::vec3& position, glm::quat& rot, bool& cut)
{
    cut = false;
    if (m_LookaroundTouchId != -1)
    {
        auto mouseDiff = m_LastLookaroundTouchPosition - m_CurrentLookaroundTouchPosition;
        auto angleChange = mouseDiff * frameTime * m_RotateSpeed;

        m_LastLookaroundTouchPosition = m_CurrentLookaroundTouchPosition;
        // one (touch) rotation axis is relative to the view direction, other is relative to world - prevents camera from 'twisting' although does introduce gimbal when looking along the UP axis and rotationg left/right.
        rot = glm::angleAxis( angleChange.x, m_WorldUp ) * rot * glm::angleAxis( angleChange.y, cVecViewRight );
        rot = glm::normalize( rot );
    }

    if (m_MovementTouchId != -1)
    {
        auto mouseDiff = m_LastMovementTouchPosition - m_CurrentMovementTouchPosition;
        auto directionChange = mouseDiff * frameTime * m_MoveSpeed * cTouchMoveSpeedMultipler;

        position -= rot * cVecViewRight * directionChange.x;
        position += rot * cVecViewForward * directionChange.y;
    }
}

