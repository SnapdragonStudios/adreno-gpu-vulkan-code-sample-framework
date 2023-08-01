//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "SphereCamera.h"

//=============================================================================
// CONSTANTS
//=============================================================================
const float kCameraMomentum = 2000.0f;          // Time (ms) it takes the camera to stop moving after touches stop
const float kCameraMomentumScale = 0.0625f;     // Multiplication factor to get from screen movement to world movement

//=============================================================================
// GLOBALS
//=============================================================================

//-----------------------------------------------------------------------------
float L_SphereCamClamp(float x, float a, float b)
//-----------------------------------------------------------------------------
{
    return x < a ? a : (x > b ? b : x);
}



//-----------------------------------------------------------------------------
CSphereCamera::CSphereCamera()
//-----------------------------------------------------------------------------
{
    m_uiLastTouchTime = 0;
    m_LastTouchPos = glm::vec3(0.0f, 0.0f, 0.0f);
    m_CurrentTouchDelta = glm::vec3(0.0f, 0.0f, 0.0f);

    m_iScreenWidth = 1280;
    m_iScreenHeight = 720;

    // Sphere Camera
    m_Position = glm::vec3(0.0f, 0.0f, 6.0f);
    m_Distance = 6.0f;
    m_MinDistance = 1.0f;
    m_MaxDistance = 200.0f;

    m_LookAt = glm::vec3(0.0f, 0.0f, 0.0f);;
    m_Pitch = 0.0f;
    m_MinPitch = MIN_CAMERA_PITCH;
    m_MaxPitch = MAX_CAMERA_PITCH;

    m_Yaw = 0.0f;
    m_TouchID = -1;

    m_MoveVelocity = 0.25f;
    m_TouchDisabled = false;

    // Time Variables
    m_uiElapsedTime = 0;
    m_uiLastTime = OS_GetTimeMS();

    m_Aspect = 1.0f;
    m_FOV = PI * 0.25f;
    m_NearClip = 1.0f;
    m_FarClip = 100.0f;
}

//-----------------------------------------------------------------------------
CSphereCamera::~CSphereCamera()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void CSphereCamera::TouchDownEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    if (m_TouchDisabled)
        return;

    if(m_TouchID >= 0)
    {
        // Ignore this second touch down event
        return;
    }

    m_uiLastTouchTime = OS_GetTimeMS();

    float fltPosX = +2.0f * (float)xPos/(float)(m_iScreenWidth - 1) - 1.0f;
    float fltPosY = -2.0f * (float)yPos/(float)(m_iScreenHeight - 1) + 1.0f;

    // LogInfo("(Spam) TouchDownEvent(%d): (%0.2f, %0.2f) => (%0.2f, %0.2f)", iPointerID, xPos, yPos, fltPosX, fltPosY);

    m_TouchID = iPointerID;

    m_LastTouchPos = glm::vec3(fltPosX, fltPosY, 0.0f);
    m_CurrentTouchDelta = glm::vec3(0.0f, 0.0f, 0.0f);

    //LogInfo("(Spam) *******************************************");
    //LogInfo("(Spam) Current Touch Delta: (%0.3f, %0.3f, %0.3f)", m_CurrentTouchDelta[0], m_CurrentTouchDelta[1], m_CurrentTouchDelta[2]);
    //LogInfo("(Spam) *******************************************");

}

//-----------------------------------------------------------------------------
void CSphereCamera::TouchMoveEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    if (m_TouchDisabled)
        return;

    if(m_TouchID != iPointerID)
    {
        // Ignore this second touch move event
        return;
    }

    float fltPosX = +2.0f * (float)xPos/(float)(m_iScreenWidth - 1) - 1.0f;
    float fltPosY = -2.0f * (float)yPos/(float)(m_iScreenHeight - 1) + 1.0f;

    // LogInfo("(Spam) TouchMoveEvent(%d): (%0.2f, %0.2f) => (%0.2f, %0.2f)", iPointerID, xPos, yPos, fltPosX, fltPosY);

    // Momentum camera
    uint32_t TimeNow = OS_GetTimeMS();
    uint32_t ElapsedTime = TimeNow - m_uiLastTouchTime;

    // The base problem is that on PC we get multiple move messages for every draw message.
    // On Android it is one to one.
    if(ElapsedTime != 0)
    {
        // This puts the current delta as a speed
        m_CurrentTouchDelta = glm::vec3(fltPosX, fltPosY, 0.0f);
        m_CurrentTouchDelta -= m_LastTouchPos;
        m_CurrentTouchDelta *= 1000.0f;
        m_CurrentTouchDelta /= (float)ElapsedTime;
    }

    // Can update this AFTER updating the current move delta
    m_uiLastTouchTime = OS_GetTimeMS();
    m_LastTouchPos = glm::vec3(fltPosX, fltPosY, 0.0f);
}

//-----------------------------------------------------------------------------
void CSphereCamera::TouchUpEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    if (m_TouchDisabled)
        return;

    if(m_TouchID != iPointerID)
    {
        // Ignore this second touch up event
        return;
    }

    m_uiLastTouchTime = OS_GetTimeMS();

    float fltPosX = +2.0f * (float)xPos/(float)(m_iScreenWidth - 1) - 1.0f;
    float fltPosY = -2.0f * (float)yPos/(float)(m_iScreenHeight - 1) + 1.0f;
    m_LastTouchPos = glm::vec3(fltPosX, fltPosY, 0.0f);

    //LogInfo("TouchUpEvent(%d): (%0.2f, %0.2f) => (%0.2f, %0.2f)", iPointerID, xPos, yPos, fltPosX, fltPosY);

    m_TouchID = -1;
}

//-----------------------------------------------------------------------------
void CSphereCamera::Tick(float ElapsedTime)
//-----------------------------------------------------------------------------
{
    if (m_TouchDisabled)
        return;

    float CameraVelocity = 0.0f;

    // Handle the Momentum
    uint32_t TimeNow = OS_GetTimeMS();

    if (m_uiLastTouchTime == 0)
        m_uiLastTouchTime = TimeNow;

    uint32_t TouchElapsedTime = TimeNow - m_uiLastTouchTime;

    // Handle Movement
    if(m_TouchID >= 0)
    {
        CameraVelocity = m_MoveVelocity * ElapsedTime;

        // LogInfo("(Spam) Camera Movement Velocity: %0.2f", CameraVelocity);

        float Inverse = -1.0f;
        m_Pitch += Inverse * CameraVelocity * m_CurrentTouchDelta[1];
        m_Yaw += Inverse * CameraVelocity * m_CurrentTouchDelta[0];
    }


    // Don't add momentum if still touching
    if(m_TouchID < 0 && TouchElapsedTime != 0)
    {
        float SwipeLerp = 1.0f - (float)TouchElapsedTime / kCameraMomentum;
        SwipeLerp = L_SphereCamClamp(SwipeLerp, 0.0f, 1.0f);

        if(SwipeLerp > 0.0)
        {
            glm::vec3 CurrentDelta = SwipeLerp * kCameraMomentumScale * m_CurrentTouchDelta;
            // LogInfo("(Spam) Current Delta (Swipe Lerp = %0.3f): (%0.3f, %0.3f, %0.3f)", SwipeLerp, m_CurrentTouchDelta[0], m_CurrentTouchDelta[1], m_CurrentTouchDelta[2]);

            // This control method only works on Android due to the fact that on PC
            // there are many move updates per frame and some get lost.
            float Inverse = -1.0f;
            float YawAdjust = Inverse * m_MoveVelocity * CurrentDelta[0];
            m_Yaw += YawAdjust;

            // LogInfo("(Spam) YawAdjust = %0.2f", YawAdjust);

            // Don't want momentum on vertical part
            // m_Pitch += Inverse * CameraVelocity * CurrentDelta[1];
        }
    }


    // Want to limit the pitch
    m_Pitch = L_SphereCamClamp(m_Pitch, m_MinPitch, m_MaxPitch);

    // Want to limit the distance
    m_Distance = L_SphereCamClamp(m_Distance, m_MinDistance, m_MaxDistance);

    // First rotate around X-Axis to by Pitch amount
    m_Position = glm::vec3(0.0f, 0.0f, m_Distance);

    glm::mat4 PitchTrans = glm::rotate(glm::mat4(1.0f), m_Pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_Position = PitchTrans * glm::vec4(m_Position, 0.0);

    // Now rotate around the Y-Axis by Yaw amount
    glm::mat4 YawTrans = glm::rotate(glm::mat4(1.0f), -m_Yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    m_Position = YawTrans * glm::vec4(m_Position, 0.0);

    // Adjust by LookAt
    m_Position += m_LookAt;

    // Rebuild the view matrix based on changes to the camera
    glm::vec3 LocalUp = glm::vec3(0.0f, 1.0f, 0.0f);
    m_ViewMatrix = glm::lookAtRH(m_Position, m_LookAt, LocalUp);


    m_ProjectionMatrix = glm::perspectiveRH(m_FOV, m_Aspect, m_NearClip, m_FarClip);

    glm::mat4 viewProj = m_ProjectionMatrix * m_ViewMatrix;
    m_InverseViewProjection = glm::inverse(viewProj);
}

//-----------------------------------------------------------------------------
void CSphereCamera::SetPosition(glm::vec3&Position)
//-----------------------------------------------------------------------------
{ 
    m_Position = Position; 

    // X-Value is getting inverted from FPS camera and I don't want
    // to debug it now :)
    // m_Position[0] *= -1.0f;

    // Update Pitch, Yaw, Distance, etc.
    UpdateValues();
}

//-----------------------------------------------------------------------------
void CSphereCamera::SetLookAt(glm::vec3&LookAt)
//-----------------------------------------------------------------------------
{ 
    m_LookAt = LookAt; 

    // Update Pitch, Yaw, Distance, etc.
    UpdateValues();
}

//-----------------------------------------------------------------------------
void CSphereCamera::UpdateValues()
//-----------------------------------------------------------------------------
{
    // Set Pitch and Yaw so we start looking where we want
    // Note +Z in GL is -Y in math
    m_Yaw = atan2(-m_Position[2] - m_LookAt[2], m_Position[0] - m_LookAt[0]);
    m_Yaw += PI; // Since really looking other way from point
    m_Yaw -= PI_DIV_2;   // Since camera wants to start looking down -Z or 90 degrees
    if(m_Yaw < 0.0f)
        m_Yaw += PI_MUL_2;

    float YawDegrees = m_Yaw * TO_DEGREES;

    // For Yaw, we need to start with the hypotenuse in XZ plane
    float TempX = m_Position[0] - m_LookAt[0];
    float TempZ = m_Position[2] - m_LookAt[2];

    float BaseLength = sqrt(TempX * TempX + TempZ * TempZ);
    m_Pitch = atan2(m_LookAt[1] - m_Position[1], BaseLength);

    float PitchDegrees = m_Pitch * TO_DEGREES;

    // Distance
    m_Distance = length(m_Position - m_LookAt);
}


//-----------------------------------------------------------------------------
void CSphereCamera::GetViewMatrix(glm::mat4 &RetMatrix)
//-----------------------------------------------------------------------------
{
#ifdef KILL_THIS
    glm::vec3 LocalRight = glm::vec3( cosf(m_Yaw), 0.0f, sinf(m_Yaw) );
    glm::vec3 LocalForward = glm::vec3( cosf(m_Yaw + PI_DIV_2), sinf(m_Pitch), -sinf(m_Yaw + PI_DIV_2) );
    LocalForward = normalize(LocalForward);
    glm::vec3 LocalUp = cross(LocalRight, LocalForward);
#endif // KILL_THIS

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    glm::vec3 LocalUp = glm::vec3(0.0f, 1.0f, 0.0f);
    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 

    // Rebuild the view matrix based on changes to the camera
    m_ViewMatrix = glm::lookAtRH(m_Position, m_LookAt, LocalUp);

    RetMatrix = m_ViewMatrix;
}

