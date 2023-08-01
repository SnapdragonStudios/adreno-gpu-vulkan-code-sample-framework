//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"
#include "system/math_common.hpp"
#include "system/os_common.h"


//=============================================================================
// CONSTANTS
//=============================================================================
#define MIN_CAMERA_PITCH        (-7.0f * PI / 16.0f)
#define MAX_CAMERA_PITCH        ( 7.0f * PI / 16.0f)

#define CAMERA_ROTATE_SPEED     PI_DIV_2    // PI
#define CAMERA_ZOOM_SPEED       5.0f

//=============================================================================
// TYPEDEFS
//=============================================================================

//=============================================================================
// CSphereCamera
//=============================================================================
class CSphereCamera
{
    // -----------------------
    // Constructors
    // -----------------------
public:
    CSphereCamera();
    ~CSphereCamera();

    // -----------------------
    // Implementation
    // -----------------------
public:
    void    SetScreenSize(int iWidth, int iHeight) { m_iScreenWidth = iWidth; m_iScreenHeight = iHeight; }

    void    SetPosition(glm::vec3 &Position);
    void    SetLookAt(glm::vec3&LookAt);

    void    UpdateValues();

    // void    SetPosition(glm::vec3 &Position) { m_Position = Position; }
    void    SetDistance(float NewDist) { m_Distance = NewDist; }
    void    SetMinDistance(float NewDist) { m_MinDistance = NewDist; }
    void    SetMaxDistance(float NewDist) { m_MaxDistance = NewDist; }

    float   GetMoveVelocity() { return m_MoveVelocity; }
    void    SetMoveVelocity(float NewVelocity) { m_MoveVelocity = NewVelocity; }

    void    GetPosition(glm::vec3&RetPosition) { RetPosition = m_Position; }

    float   GetPitch() { return m_Pitch; }
    void    SetPitch(float NewPitch) { m_Pitch = NewPitch; }
    void    SetMinPitch(float NewPitch) { m_MinPitch = NewPitch; }
    void    SetMaxPitch(float NewPitch) { m_MaxPitch = NewPitch; }

    float   GetYaw() { return m_Yaw; }
    void    SetYaw(float NewYaw) { m_Yaw = NewYaw; }

    void    TouchDownEvent(int iPointerID, float xPos, float yPos);
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos);
    void    TouchUpEvent(int iPointerID, float xPos, float yPos);
    void    Tick(float ElapsedTime);

    void        SetAspect(float aspect) { m_Aspect = aspect; }
    void        SetFov(float fov) { m_FOV = fov; }
    void        SetClipPlanes(float NewNear, float NewFar) { m_NearClip = NewNear; m_FarClip = NewFar; }

    void        GetViewMatrix(glm::mat4 &RetMatrix);
    glm::mat4   ViewMatrix() const { return m_ViewMatrix; }
    glm::vec3   Position() const { return m_Position; }
    glm::mat4   ProjectionMatrix() const { return m_ProjectionMatrix; }
    glm::mat4   InverseViewProjection() const { return m_InverseViewProjection; }///<@returns the inverse of the view projection matrix


private:

    // -----------------------
    // Attributes
    // -----------------------

public:

    // Screen Size
    int             m_iScreenWidth;
    int             m_iScreenHeight;

    // Sphere Camera
    glm::vec3       m_Position;
    float           m_Distance;
    float           m_MinDistance;
    float           m_MaxDistance;
    glm::vec3       m_LookAt;
    float           m_Pitch;
    float           m_MinPitch;
    float           m_MaxPitch;
    float           m_Yaw;
    int             m_TouchID;

    float           m_MoveVelocity;
    bool            m_TouchDisabled;

    // Time Variables
    uint32_t        m_uiElapsedTime;
    uint32_t        m_uiLastTime;

    // Momentum camera
    uint32_t        m_uiLastTouchTime;
    glm::vec3       m_LastTouchPos;
    glm::vec3       m_CurrentTouchDelta;

    // Camera Frustum Values
    float               m_Aspect;
    float               m_FOV;
    float               m_NearClip;
    float               m_FarClip;

    // The Projection and View matrix updated in Tick()
    glm::mat4       m_ProjectionMatrix;
    glm::mat4       m_ViewMatrix;
    glm::mat4       m_InverseViewProjection;
};
