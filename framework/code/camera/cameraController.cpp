// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "cameraController.hpp"
#include <glm/gtc/quaternion.hpp>

static const float cMouseRotSpeed = 0.1f;

// Helpers
constexpr glm::vec3 cVecUp = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 cVecRight = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 cVecForward = glm::vec3(0.0f, 0.0f, -1.0f);


//-----------------------------------------------------------------------------

CameraController::CameraController()
    : m_LastMousePosition(0.0f)
    , m_CurrentMousePosition(0.0f)
    , m_ScreenSize(1920.f,1080.f)
{
}

//-----------------------------------------------------------------------------

bool CameraController::Initialize(uint32_t width, uint32_t height)
{
    SetSize(width, height);
    return true;
}

//-----------------------------------------------------------------------------

void CameraController::SetSize(uint32_t width, uint32_t height)
{
    m_ScreenSize = glm::vec2( width, height );
}

//-----------------------------------------------------------------------------

enum CameraController::KeysDownBits CameraController::KeyToBits(uint32_t key)
{
    switch (key)
    {
    case 'W':
        return KeysDownBits::eForward;
        break;
    case 'A':
        return KeysDownBits::eLeft;
        break;
    case 'S':
        return KeysDownBits::eBackward;
        break;
    case 'D':
        return KeysDownBits::eRight;
        break;
    default:
        return KeysDownBits::eNone;
        break;
    }
}

void CameraController::KeyDownEvent(uint32_t key)
{
    KeysDownBits keyDown = KeyToBits(key);
    m_KeysDown = KeysDownBits(m_KeysDown | keyDown );
}

//-----------------------------------------------------------------------------

void CameraController::KeyUpEvent(uint32_t key)
{
    KeysDownBits keyUp = KeyToBits(key);
    m_KeysDown = KeysDownBits(m_KeysDown & ~keyUp);
}

//-----------------------------------------------------------------------------

void CameraController::TouchDownEvent(int iPointerID, float xPos, float yPos)
{
    //assert(m_touchDown==false);
    m_touchDown = true;
    m_LastMousePosition = { xPos, yPos };
    m_CurrentMousePosition = m_LastMousePosition;
}

//-----------------------------------------------------------------------------

void CameraController::TouchMoveEvent(int iPointerID, float xPos, float yPos)
{
    m_CurrentMousePosition = { xPos, yPos };
}

//-----------------------------------------------------------------------------

void CameraController::TouchUpEvent(int iPointerID, float xPos, float yPos)
{
    m_touchDown = false;
    m_CurrentMousePosition = { xPos, yPos };
}

//-----------------------------------------------------------------------------

void CameraController::Update(float frameTime, glm::vec3& position, glm::quat& rot)
{
    if (m_touchDown)
    {
        auto mouseDiff = m_LastMousePosition - m_CurrentMousePosition;
        auto angleChange = mouseDiff * frameTime * cMouseRotSpeed;

        m_LastMousePosition = m_CurrentMousePosition;
        rot = glm::angleAxis(angleChange.y, cVecRight) * rot * glm::angleAxis(angleChange.x, cVecUp);
    }

    if (m_KeysDown != KeysDownBits::eNone)
    {
        if (m_KeysDown & (KeysDownBits::eLeft | KeysDownBits::eRight))
        {
            float direction = (m_KeysDown & KeysDownBits::eLeft) ? -1.0f : 1.0f;
            position += cVecRight * direction * rot;
        }
        if (m_KeysDown & (KeysDownBits::eForward | KeysDownBits::eBackward))
        {
            float direction = (m_KeysDown & KeysDownBits::eBackward) ? -1.0f : 1.0f;
            position += cVecForward * direction * rot;
        }
    }
}

