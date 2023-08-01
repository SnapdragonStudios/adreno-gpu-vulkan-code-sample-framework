//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "cameraControllerAnim.hpp"
#include "animation/animation.hpp"
#include <glm/gtx/quaternion.hpp>
#include <limits>

static const float cMouseRotSpeed = 0.1f;
static const float cMouseMoveSpeeed = 0.001f;

// Helpers
constexpr glm::vec3 cVecUp = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 cVecRight = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 cVecForward = glm::vec3(0.0f, 0.0f, -1.0f);

//-----------------------------------------------------------------------------

CameraControllerAnim::CameraControllerAnim()
    : CameraControllerBase()
{
}

//-----------------------------------------------------------------------------

bool CameraControllerAnim::Initialize( uint32_t width, uint32_t height )
{
    return CameraControllerBase::Initialize( width, height );
}

//-----------------------------------------------------------------------------

void CameraControllerAnim::SetPathAnimation(const Animation * pAnimation, int nodeIdx)
{
    m_Animation = pAnimation;
    m_CameraAnimationNodeIdx = nodeIdx;
}

//-----------------------------------------------------------------------------

/// Update the camera controller and modify the output position/rotation accordingly
/// @param frameTime time in seconds since last Update.
/// @param position in - current camera position, out - camera position after applying controller
/// @param rot in - current camera rotation, out - camera rotation after applying controller
void CameraControllerAnim::Update(float frameTime, glm::vec3& position, glm::quat& rot)
{
    m_CameraAnimationTime += frameTime * m_CameraAnimationSpeed;
    m_CameraAnimationTime = std::fmodf(m_CameraAnimationTime, m_Animation->GetEndTime());
    if (m_CameraAnimationTime < 0.0f)
        m_CameraAnimationTime += m_Animation->GetEndTime();

    position = m_Animation->CalcLocalTranslation(m_CameraAnimationNodeIdx, m_CameraAnimationTime, m_CameraAnimationFrameHint);
    rot = m_Animation->CalcLocalRotation( m_CameraAnimationNodeIdx, m_CameraAnimationTime, m_CameraAnimationFrameHint );
    glm::vec3 scale = m_Animation->CalcLocalScale( m_CameraAnimationNodeIdx, m_CameraAnimationTime, m_CameraAnimationFrameHint );

    glm::mat4 Matrix = glm::translate( position );
    Matrix = Matrix * glm::toMat4( rot );
    Matrix = Matrix * glm::scale( scale );
    Matrix = m_PreTransform * Matrix * m_PostTransform;
    rot = glm::toQuat(Matrix);
    position = glm::vec3( Matrix[3][0], Matrix[3][1], Matrix[3][2] );
}

//-----------------------------------------------------------------------------

CameraControllerAnimControllable::CameraControllerAnimControllable()
    : CameraControllerAnim()
    , m_LastMovementTouchPosition( 0.0f )
    , m_CurrentMovementTouchPosition( 0.0f )
    , m_LastLookaroundTouchPosition( 0.0f )
    , m_CurrentLookaroundTouchPosition( 0.0f )
    , m_MovementTouchId( -1 )
    , m_LookaroundTouchId( -1 )
{
}

//-----------------------------------------------------------------------------

void CameraControllerAnimControllable::TouchDownEvent( int iPointerID, float xPos, float yPos )
{
    if (xPos >= m_ScreenSize.x * 0.66f && m_LookaroundTouchId == -1)
    {
        m_LookaroundTouchId = iPointerID;
        m_LastLookaroundTouchPosition = { xPos, yPos };
        m_CurrentLookaroundTouchPosition = m_LastLookaroundTouchPosition;
    }
    else if (xPos < m_ScreenSize.x * 0.66f && m_MovementTouchId == -1)
    {
        m_MovementTouchId = iPointerID;
        m_LastMovementTouchPosition = { xPos, yPos };
        m_CurrentMovementTouchPosition = m_LastMovementTouchPosition;
    }
}

//-----------------------------------------------------------------------------

void CameraControllerAnimControllable::TouchMoveEvent( int iPointerID, float xPos, float yPos )
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

void CameraControllerAnimControllable::TouchUpEvent( int iPointerID, float xPos, float yPos )
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

/// Update the camera controller and modify the output position/rotation accordingly
/// @param frameTime time in seconds since last Update.
/// @param position in - current camera position, out - camera position after applying controller
/// @param rot in - current camera rotation, out - camera rotation after applying controller
void CameraControllerAnimControllable::Update( float frameTime, glm::vec3& position, glm::quat& rot )
{
    if (m_LookaroundTouchId != -1)
    {
        auto mouseDiff = m_LastLookaroundTouchPosition - m_CurrentLookaroundTouchPosition;
        auto angleChange = mouseDiff * frameTime * m_RotateSpeed;

        m_LastLookaroundTouchPosition = m_CurrentLookaroundTouchPosition;

        m_AnimationRestartTimer = m_AnimationRestartInterval;

        m_CameraZoomVelocity += std::min( 1.0f, 1.0f * frameTime ) * m_CameraZoomAccleration * (angleChange.y - m_CameraZoomVelocity);
        m_CameraZoomVelocity = std::min( m_CameraZoomMaxVelocity, std::max( m_CameraZoomVelocity, -m_CameraZoomMaxVelocity ) );
    }
    else
        m_CameraZoomVelocity *= 1.0f - std::min( 1.0f, 1.0f * frameTime );

    m_CameraZoom += m_CameraZoomVelocity;
    if (m_CameraZoom > m_CameraZoomMax)
        m_CameraZoom *= std::expf( 0.1f * (m_CameraZoomMax - m_CameraZoom) );
    else if (m_CameraZoom < m_CameraZoomMin)
        m_CameraZoom *= std::expf( 0.1f * (m_CameraZoom - m_CameraZoomMin) );

    if (m_MovementTouchId != -1)
    {
        const auto mouseDiff = m_LastMovementTouchPosition - m_CurrentMovementTouchPosition;
        const auto directionChange = mouseDiff * frameTime * m_AnimationMoveSpeed;

        m_directionVelocity += std::min( 1.0f, 1.0f * frameTime ) * m_directionAccleration * (directionChange - m_directionVelocity);
        m_directionVelocity = glm::min( m_directionMaxVelocity, glm::max( m_directionVelocity, -m_directionMaxVelocity ) );

        m_animationLastRotationDirection = std::copysignf( 1.0f, m_directionVelocity.x );
        m_AnimationRestartTimer = m_AnimationRestartInterval;
    }
    else
        m_directionVelocity *= 1.0f - std::min( 1.0f, 1.0f * frameTime );

    m_CameraAnimationTime += m_directionVelocity.x;

    m_CameraElevationRotation += m_directionVelocity.y;
    if (m_CameraElevationRotation > m_CameraElevationRotationMax)
        m_CameraElevationRotation *= std::expf( 0.1f * (m_CameraElevationRotationMax - m_CameraElevationRotation) );
    else if (m_CameraElevationRotation < m_CameraElevationRotationMin)
        m_CameraElevationRotation *= std::expf( 0.1f * (m_CameraElevationRotation - m_CameraElevationRotationMin) );

    // Countdown the timer until the camera automatically moves
    m_AnimationRestartTimer -= frameTime;
    // Wait before resuming 'automatic' rotation (and blending in to this rotation)
    float animationSpeedLerp = 1.0f;
    if (m_AnimationRestartTimer < 0.0f)
    {
        if (m_AnimationRestartTimer <= -1.0f)
            m_AnimationRestartTimer = -1.0f;
        else
            animationSpeedLerp = -m_AnimationRestartTimer;
        // slowly reset any elevation rotation the user added
        m_CameraElevationRotation *= 1.0f - std::min( 1.0f, 1.0f * frameTime * std::abs( m_CameraElevationRotation ) * animationSpeedLerp * 0.1f );
    }
    else
        animationSpeedLerp = 0.0f;

    m_CameraAnimationTime += frameTime * m_CameraAnimationSpeed * animationSpeedLerp * m_animationLastRotationDirection;
    m_CameraAnimationTime = std::fmodf( m_CameraAnimationTime, m_Animation->GetEndTime() );
    if (m_CameraAnimationTime < 0.0f)
        m_CameraAnimationTime += m_Animation->GetEndTime();

    position = m_Animation->CalcLocalTranslation( m_CameraAnimationNodeIdx, m_CameraAnimationTime, m_CameraAnimationFrameHint );
    rot = m_Animation->CalcLocalRotation( m_CameraAnimationNodeIdx, m_CameraAnimationTime, m_CameraAnimationFrameHint );

    position -= rot * glm::vec3( 0.0f, 0.0f, 10.0f );

    glm::quat extraRotation = glm::quat( glm::vec3( m_CameraElevationRotation, 0.0f, 0.0f ) );
    rot = rot * extraRotation;

    position += rot * glm::vec3( 0.0f, 0.0f, 10.0f - m_CameraZoom );
}


