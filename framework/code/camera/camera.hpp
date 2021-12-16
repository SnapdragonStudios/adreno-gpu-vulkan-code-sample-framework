// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

/// @defgroup Camera
/// Game Camera functionality.

#include "system/glm_common.hpp"

// Forward declarations
class CameraController;

/// Perspective camera.
/// @ingroup Camera
class Camera
{
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
public:
    Camera();

    /// Set camera aspect ratio
    void SetAspect(float aspect);

    /// Set camera field of view
    void SetFov(float fov);

    /// Set camera clip distances
    void SetClipPlanes(float near, float far);

    /// Set camera postion (if using UpdateController, call SetPosition for initial camera setup).
    void SetPosition(const glm::vec3& Position, const glm::quat& Rotation);

    /// Update the given (templated) camera controller and have it update the camera position/rotation
    template<class T_CameraController>
    void UpdateController(float ElapsedTimeSeconds, T_CameraController& CameraController)
    {
        CameraController.Update(ElapsedTimeSeconds, m_CurrentCameraPos, m_CurrentCameraRot);
    }

    /// Update camera matrices (based on current rotation/position)
    void UpdateMatrices();

    // Accessors
    glm::vec3           Position() const { return m_CurrentCameraPos; }         ///<@returns the current camera postion
    glm::quat           Rotation() const { return m_CurrentCameraRot; }         ///<@returns the current camera rotation
    glm::mat4           ProjectionMatrix() const { return m_ProjectionMatrix; } ///<@returns the camera projection matrix (as computed by UpdateMatrices)
    glm::mat4           ViewMatrix() const { return m_ViewMatrix; }             ///<@returns the camera view matrix (as computed by UpdateMatrices)
    glm::vec3           ViewDirection() const { return m_ViewMatrix[2]; }       ///<@returns the current camera direction (view along the z axis)

protected:
    // Camera parameters
    float               m_Aspect;
    float               m_FOV;
    float               m_NearClip;
    float               m_FarClip;

    // Positions
    glm::vec3           m_CurrentCameraPos;
    glm::quat           m_CurrentCameraRot;

    // Matrices
    glm::mat4           m_ProjectionMatrix;
    glm::mat4           m_ViewMatrix;
};
