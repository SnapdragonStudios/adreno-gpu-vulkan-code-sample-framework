// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "camera.hpp"
#include "system/math_common.hpp"

Camera::Camera()
    : m_Aspect(1.0)
    , m_FOV(PI*0.25f)
    , m_NearClip(1.0f)
    , m_FarClip(100.0f)
    , m_CurrentCameraPos()
    , m_CurrentCameraRot()
    , m_ProjectionMatrix()
    , m_ViewMatrix()
{
    UpdateMatrices();   // set sensible proj/view matrices
}

//-----------------------------------------------------------------------------
void Camera::SetAspect(float aspect)
//-----------------------------------------------------------------------------
{
    m_Aspect = aspect;
}

//-----------------------------------------------------------------------------
void Camera::SetFov(float fovRadians)
//-----------------------------------------------------------------------------
{
    m_FOV = fovRadians;
}

//-----------------------------------------------------------------------------
void Camera::SetClipPlanes(float near, float far)
//-----------------------------------------------------------------------------
{
    m_NearClip = near;
    m_FarClip = far;
}

//-----------------------------------------------------------------------------
void Camera::SetPosition(const glm::vec3& position, const glm::quat& rotation)
//-----------------------------------------------------------------------------
{
    m_CurrentCameraPos = position;
    m_CurrentCameraRot = rotation;
}

//-----------------------------------------------------------------------------
void Camera::UpdateMatrices()
//-----------------------------------------------------------------------------
{
    // Update camera position, view matrix, etc.
    m_ProjectionMatrix = glm::perspectiveRH(m_FOV, m_Aspect, m_NearClip, m_FarClip);
    m_ViewMatrix = glm::mat4_cast(m_CurrentCameraRot);
    m_ViewMatrix = glm::translate(m_ViewMatrix, -m_CurrentCameraPos);
}
