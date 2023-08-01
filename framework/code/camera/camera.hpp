//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @defgroup Camera
/// Game Camera functionality.

#include "system/glm_common.hpp"

// Forward declarations
class CameraController;
struct CameraData;

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

    /// Set camera (position and orientation etc) from CameraData
    void Set(const CameraData&);

    /// Set Jitter amount (applied in UpdateMatrices)
    void SetJitter(const glm::vec2 Jitter);

    /// Return current projection matrix with the given jitter amount added
    glm::mat4 GetProjectionWithJitter(const glm::vec3 jitter) const;

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
    glm::mat4           ProjectionMatrixNoJitter() const { return m_ProjectionMatrixNoJitter; } ///<@returns the camera projection matrix without any Jitter applied
    glm::mat4           ViewMatrix() const { return m_ViewMatrix; }             ///<@returns the camera view matrix (as computed by UpdateMatrices)
    glm::vec3           ViewDirection() const { return m_ViewMatrix[2]; }       ///<@returns the current camera direction (view along the z axis)
    glm::mat4           InverseViewProjection() const { return m_InverseViewProjection; }///<@returns the inverse of the view projection matrix
    float               NearClip() const { return m_NearClip; }                 ///<@returns the camera near clip distance
    float               FarClip() const { return m_FarClip; }                   ///<@returns the camera far clip distance
    float               Fov() const { return m_FOV; }                           ///<@returns the camera field of view
    float               Aspect() const { return m_Aspect; }                     ///<@returns the camera aspect ratio

protected:
    // Camera parameters
    bool                m_Orthographic;
    float               m_Aspect;
    float               m_FOV;
    float               m_NearClip;
    float               m_FarClip;

    // Positions
    glm::vec3           m_CurrentCameraPos;
    glm::quat           m_CurrentCameraRot;

    // Sub-pixel Jitter
    glm::vec2           m_Jitter;

    // Matrices
    glm::mat4           m_ProjectionMatrixNoJitter;
    glm::mat4           m_ProjectionMatrix;
    glm::mat4           m_ViewMatrix;
    glm::mat4           m_InverseViewProjection;
};
