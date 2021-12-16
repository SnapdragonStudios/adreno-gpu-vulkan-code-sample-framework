// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

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

void Shadow::SetEyeClipPlanes(float eyeCameraFov, float eyeNearPlane, float farPlane)
{
    m_eyeCameraFov = eyeCameraFov;
    m_eyeNearPlane = eyeNearPlane;
    m_farPlane = farPlane;
}

void Shadow::Update(const glm::mat4& eyeViewMatrix)
{
    // Shadows are faded out some distance from the 'eye' camera (attempt to control fustrum sizes)
    glm::mat4 cameraProjection = glm::perspectiveRH(m_eyeCameraFov, m_shadowCameraAspect, m_eyeNearPlane, m_farPlane);

    // Now get the un-projection matrix that goes from clip space to shadow view space
    glm::mat4 cameraProjView = cameraProjection * eyeViewMatrix;
    glm::mat4 unprojectView = glm::inverse(cameraProjView);
    glm::mat4 unprojectToLightView = m_ShadowView * unprojectView;

    // Calculate the (eye) camera's projection vertices (8 points)
    glm::vec4 boxExtents[8] = { glm::vec4(1.0f, 1.0f,  1.0f, 1.0f), glm::vec4(-1.0f, 1.0f,  1.0f, 1.0f), glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),  glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f),
                                glm::vec4(1.0f, 1.0f,  0.0f, 1.0f), glm::vec4(-1.0f, 1.0f,  0.0f, 1.0f), glm::vec4(1.0f, -1.0f,  0.0f, 1.0f), glm::vec4(-1.0f, -1.0f,  0.0f, 1.0f) };
    glm::vec4 worldExtents[8];
    for (int i = 0; i < 8; ++i)
    {
        auto t = unprojectToLightView/*unprojectView*/ * boxExtents[i];
        worldExtents[i] = t / t.w;
    }

    glm::vec4 worldMax = worldExtents[0];
    glm::vec4 worldMin = worldExtents[0];
    for (int i = 1; i < 8; ++i)
    {
        worldMin = glm::min(worldMin, worldExtents[i]);
        worldMax = glm::max(worldMax, worldExtents[i]);
    }

    //LOGI("orthwidth %f->%f , %f->%f , %f->%f", worldMin.x, worldMax.x, worldMin.y, worldMax.y, worldMin.z, worldMax.z);

    // Update the shadow map shader parameters
    //m_ShadowProj = glm::ortho(-gOrthoWidth, +gOrthoWidth, -gOrthoHeight, gOrthoHeight, gOrthoNear, gOrthoFar);
    //m_ShadowProj = glm::perspectiveRH(PI_DIV_4, (float)gShadowMapWidth / (float)gShadowMapHeight, gOrthoNear, gOrthoFar);
    m_ShadowProj = glm::ortho(worldMin.x, worldMax.x, worldMin.y, worldMax.y, -worldMax.z, -worldMin.z);

    // Need this so we can map from [0, 1] depth values
    m_LightFrustumDepth = worldMax.z - worldMin.z;// gOrthoFar - gOrthoNear;

    // GLfloat fltAspect = (GLfloat) gRenderWidth / (GLfloat) gRenderHeight;
    //m_ShadowProj = glm::mat4::perspective(m_FOV, fltAspect, m_Near, m_Far);
    // m_ShadowProj = glm::mat4::perspective(m_FOV, 1.0f, gOrthoNear, gOrthoFar);

    m_ShadowViewProj = m_ShadowProj * m_ShadowView;

    //m_ShadowGenMVP = m_ShadowProj * m_ShadowView;

    //m_ShadowMVP = m_ShadowViewProj;
}

bool Shadow::Initialize(Vulkan& vulkan, uint32_t shadowMapWidth, uint32_t shadowMapHeight, bool addColorTarget)
{
    m_Viewport.width = (float) shadowMapWidth;
    m_Viewport.height = (float) shadowMapHeight;
    m_Viewport.minDepth = 0.0f;
    m_Viewport.maxDepth = 1.0f;
    m_Viewport.x = 0.0f;
    m_Viewport.y = 0.0f;
    m_Scissor.offset.x = 0;
    m_Scissor.offset.y = 0;
    m_Scissor.extent.width = shadowMapWidth;
    m_Scissor.extent.height = shadowMapHeight;

    // Shadow Color is multiplied so this is a "darkening"
    m_ShadowColor = glm::vec4(0.65f, 0.65f, 0.65f, 1.0f);

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
    const VkFormat colorFormat[]{ VK_FORMAT_R8G8B8A8_UNORM };
    const tcb::span<const VkFormat> emptyColorFormat {};
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    if (!m_ShadowMapRT.Initialize(&vulkan, shadowMapWidth, shadowMapHeight, addColorTarget ? colorFormat : emptyColorFormat, depthFormat, VK_SAMPLE_COUNT_1_BIT, "Shadow RT"))
    {
        LOGE("Unable to create shadow render target");
        return false;
    }

    //VkRenderPass renderPass;
    //if (!vulkan.CreateRenderPass(colorType[0], depthFormat, false, true, true, &renderPass))
    //{
    //    m_ShadowMapRT.Release();
    //    return false;
    //}
    //m_renderPass = renderPass;
    return true;
}
