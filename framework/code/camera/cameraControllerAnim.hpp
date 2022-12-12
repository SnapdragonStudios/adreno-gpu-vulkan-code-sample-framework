//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "camera/cameraController.hpp"
#include "system/glm_common.hpp"
#include <memory>

class Animation;

///
/// Camera controls that move the camera along an animation path.
/// @ingroup Camera
/// 
class CameraControllerAnim : public CameraControllerBase
{
    CameraControllerAnim(const CameraControllerAnim&) = delete;
    CameraControllerAnim& operator=(const CameraControllerAnim&) = delete;
public:
    CameraControllerAnim();

    bool Initialize(uint32_t width, uint32_t height) override;

    /// Set the animation to use for this camera (clamp the camera postion to the animation path)
    void    SetPathAnimation(const Animation* pAnimation, int nodeIdx);
    /// Set the playback speed of the camera animation
    void    SetCameraAnimationSpeed( float cameraAnimationSpeed ) { m_CameraAnimationSpeed = cameraAnimationSpeed; }
    /// Set the pre animation transform matrix (applied to the 'output' camera position/rotation before the animation by @Update)
    void    SetPreTransform( const glm::mat4& parentTransform ) { m_PreTransform = parentTransform; }
    /// Set the post animation transform matrix (applied to the 'output' camera position/rotation after the animation by @Update)
    void    SetPostTransform( const glm::mat4& parentTransform ) { m_PostTransform = parentTransform; }

    /// Update the camera controller and modify the output position/rotation accordingly
    /// @param frameTime time in seconds since last Update.
    /// @param position in - current camera position, out - camera position after applying controller
    /// @param rot in - current camera rotation, out - camera rotation after applying controller
    void Update(float frameTime, glm::vec3& position, glm::quat& rot) override;

protected:
    const Animation* m_Animation = nullptr;                         ///< pointer to the animation controlling this camera postion (not owned by us)
    uint32_t    m_CameraAnimationNodeIdx = 0;                       ///< node (in m_Animation) that drives the camera position
    float       m_CameraAnimationSpeed = 1.0f;                      ///< speed that the animation is being played back at         
    uint32_t    m_CameraAnimationFrameHint = 0;
    float       m_CameraAnimationTime = 0.0f;
    glm::mat4   m_PreTransform = glm::identity<glm::mat4>();        ///< matrix for the parent of the camera animiated node (or identity if no parent)
    glm::mat4   m_PostTransform = glm::identity<glm::mat4>();       ///< matrix for the 'child' of the camera animiated node - used when the animation does not animate the camera node directly (but animates a child node), which is common in Blender exported gltf (Blender adds a post-animation node for y-up)
};

///
/// Camera controls that move the camera along an animation path.
/// @ingroup Camera
/// 
class CameraControllerAnimControllable : public CameraControllerAnim
{
    CameraControllerAnimControllable( const CameraControllerAnimControllable& ) = delete;
    CameraControllerAnimControllable& operator=( const CameraControllerAnimControllable& ) = delete;
public:
    CameraControllerAnimControllable();

    // Inputs
    void    TouchDownEvent( int iPointerID, float xPos, float yPos ) override;
    void    TouchMoveEvent( int iPointerID, float xPos, float yPos ) override;
    void    TouchUpEvent( int iPointerID, float xPos, float yPos ) override;

    /// Update the camera controller and modify the output position/rotation accordingly
    /// @param frameTime time in seconds since last Update.
    /// @param position in - current camera position, out - camera position after applying controller
    /// @param rot in - current camera rotation, out - camera rotation after applying controller
    void Update( float frameTime, glm::vec3& position, glm::quat& rot ) override;

protected:
    glm::vec2   m_LastMovementTouchPosition;
    glm::vec2   m_CurrentMovementTouchPosition;
    glm::vec2   m_LastLookaroundTouchPosition;
    glm::vec2   m_CurrentLookaroundTouchPosition;

    int         m_MovementTouchId;
    int         m_LookaroundTouchId;

    float       m_AnimationRestartTimer = 0.0f;
    float       m_AnimationMoveSpeed = 0.01f;

    glm::vec2   m_directionVelocity = glm::vec2( 0.0f );
    glm::vec2   m_directionAccleration = glm::vec2( 5.0f );
    float       m_CameraElevationRotation = 0.0f;
    float       m_animationLastRotationDirection = 1.0f;// 1.0 or -1.0

    float       m_CameraZoomVelocity = 0.0f;
    float       m_CameraZoomAccleration = 10.0f;
    float       m_CameraZoom = 0.0f;

    // camera control tuning/limits
    glm::vec2   m_directionMaxVelocity = glm::vec2( 0.1f );
    float       m_CameraElevationRotationMax = 0.6f;    // this is the limit of the (user controlled) camera rotation below the camera path (ie camera looking up)
    float       m_CameraElevationRotationMin = -1.0f;   // this is the limit of the (user controlled) camera rotation above the camera path (ie camera looking down)
    float       m_AnimationRestartInterval = 5.0f;
    float       m_CameraZoomMaxVelocity = 0.3f;
    float       m_CameraZoomMin = -4.5f;
    float       m_CameraZoomMax = 2.5f;
};

