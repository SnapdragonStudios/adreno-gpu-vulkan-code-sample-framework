//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shadow.hpp"

Shadow::Shadow() : m_ShadowLightPos(0.0f, 1.0f, 0.1f), m_ShadowLightTarget()
{
}

void Shadow::SetLightPos(const glm::vec3& lightPos, const glm::vec3& lightTarget)
{
    m_ShadowLightPos = lightPos;
    m_ShadowLightTarget = lightTarget;
    glm::vec3 TempUp(0.0f, 1.0f, 0.0f);
    m_ShadowView = glm::lookAtRH(m_ShadowLightPos, m_ShadowLightTarget, TempUp);
}

glm::vec3 Shadow::GetLightPos() const
{
    return m_ShadowLightPos;
}

void Shadow::SetEyeClipPlanes(float eyeCameraFov, float eyeCameraAspect, float eyeNearPlane, float shadowFarPlane)
{
    m_eyeCameraFov = eyeCameraFov;
    m_eyeCameraAspect = eyeCameraAspect;
    m_eyeNearPlane = eyeNearPlane;
    m_farPlane = shadowFarPlane;
}

void Shadow::Update(const glm::mat4& eyeViewMatrix)
{
    // Shadows are faded out some distance from the 'eye' camera (attempt to control fustrum sizes)
    glm::mat4 cameraProjection = glm::perspectiveRH(m_eyeCameraFov, m_eyeCameraAspect, m_eyeNearPlane, m_farPlane);

    // Now get the un-projection matrix that goes from clip space to shadow view space
    glm::mat4 cameraProjView = cameraProjection * eyeViewMatrix;
    glm::mat4 unprojectView = glm::inverse(cameraProjView);
    glm::mat4 unprojectToLightView = m_ShadowView * unprojectView;

    // Calculate the (eye) camera's projection vertices (8 points)
    constexpr glm::vec4 boxExtents[8] = { glm::vec4(1.0f, 1.0f,  1.0f, 1.0f), glm::vec4(-1.0f, 1.0f,  1.0f, 1.0f), glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),  glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f),
                                          glm::vec4(1.0f, 1.0f,  0.0f, 1.0f), glm::vec4(-1.0f, 1.0f,  0.0f, 1.0f), glm::vec4(1.0f, -1.0f,  0.0f, 1.0f), glm::vec4(-1.0f, -1.0f,  0.0f, 1.0f) };
    glm::vec4 orthoExtents[8];
    for (int i = 0; i < 8; ++i)
    {
        auto t = unprojectToLightView * boxExtents[i];
        orthoExtents[i] = t / t.w;
    }

    glm::vec4 orthoMax = orthoExtents[0];
    glm::vec4 orthoMin = orthoExtents[0];
    for (int i = 1; i < 8; ++i)
    {
        orthoMin = glm::min(orthoMin, orthoExtents[i]);
        orthoMax = glm::max(orthoMax, orthoExtents[i]);
    }
    // Push size out a little to compensate for any filtering artifacts on edge
    glm::vec4 orthoSize = orthoMax - orthoMin;
    orthoMax += orthoSize / 16.0f;
    orthoMin -= orthoSize / 16.0f;

    //LOGI("orthwidth %f->%f , %f->%f , %f->%f", worldMin.x, worldMax.x, worldMin.y, worldMax.y, worldMin.z, worldMax.z);

    // Update the shadow map shader parameters
    m_ShadowProj = glm::ortho(orthoMin.x, orthoMax.x, orthoMin.y, orthoMax.y, -orthoMax.z, -orthoMin.z);

    m_ShadowViewProj = m_ShadowProj * m_ShadowView;
}

bool Shadow::Initialize(uint32_t shadowMapWidth, uint32_t shadowMapHeight, bool addColorTarget)
{
    // This is matrix transform every coordinate x,y,z
    // x = x* 0.5 + 0.5 
    // y = y* 0.5 + 0.5 
    // z = z* 0.5 + 0.5 
    // Moving from unit cube [-1,1] to [0,1]  
    m_ShadowBiasScale = glm::mat4(glm::vec4(0.5f, 0.0f, 0.0f, 0.0f),
                                  glm::vec4(0.0f, 0.5f, 0.0f, 0.0f),
                                  glm::vec4(0.0f, 0.0f, 0.5f, 0.0f),
                                  glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
    m_shadowCameraAspect = (float)shadowMapWidth / (float)shadowMapHeight;
    return true;
}
