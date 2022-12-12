//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"
#include "vulkan/renderTarget.hpp"

/// Simple projected shadow
class Shadow
{
    Shadow(const Shadow&) = delete;
    Shadow& operator=(const Shadow&) = delete;
public:
    Shadow();

    bool Initialize(Vulkan& vulkan, uint32_t shadowMapWidth, uint32_t shadowMapHeight, bool addColorTarget);

    void SetLightPos(const glm::vec3& lightPos, const glm::vec3& lightTarget);
    glm::vec3 GetLightPos() const;
    void SetEyeClipPlanes(float eyeCameraFov, float cameraAspect, float eyeNearPlane, float shadowFarPlane);

    void Update(const glm::mat4& eyeViewMatrix);

    const auto& GetViewProj() const { return m_ShadowViewProj; }
    VkRenderPass GetRenderPass() const
    {
        return m_ShadowMapRT.m_RenderPass;
    }
    VkFramebuffer GetFramebuffer(uint32_t idx = 0) const
    {
        return m_ShadowMapRT[idx].m_FrameBuffer;
    }
    void GetTargetSize(uint32_t& width, uint32_t& height) const
    {
        width = m_ShadowMapRT[0].m_Width;
        height = m_ShadowMapRT[0].m_Height;
    }
    const VkViewport& GetViewport() const
    {
        return m_Viewport;
    }
    const VkRect2D& GetScissor() const
    {
        return m_Scissor;
    }

    VkFormat GetDepthFormat(uint32_t frameBufferIndex = 0) const
    {
        return m_ShadowMapRT[frameBufferIndex].m_DepthAttachment.Format;
    }
    const VulkanTexInfo& GetDepthTexture(uint32_t frameBufferIndex = 0) const
    {
        return m_ShadowMapRT[frameBufferIndex].m_DepthAttachment;
    }
    const VulkanTexInfo* GetColorTexture(uint32_t frameBufferIndex = 0) const
    {
        if (frameBufferIndex < m_ShadowMapRT[frameBufferIndex].m_ColorAttachments.size())
            return &m_ShadowMapRT[frameBufferIndex].m_ColorAttachments[frameBufferIndex];
        return nullptr;
    }
    const CRenderTarget& GetRenderTarget(uint32_t frameBufferIndex = 0) const
    {
        return m_ShadowMapRT[frameBufferIndex];
    }

protected:
    float           m_eyeCameraFov = 1.0f;// fov of the scene (not the shadow) camera
    float           m_eyeCameraAspect = 1.0f;// aspect of the scene (not the shadow) camera
    float           m_eyeNearPlane = 1.0f;// near plane of the scene camera
    float           m_farPlane = 1000.0f; // farthest distance we want to see shadows (from the scene camera's origin - can be lower than the scenes draw distance)
    float           m_shadowCameraAspect = 1.0f;    // Aspect ratio of shadow camera (if we have a non-square shadow map)

    glm::vec3       m_ShadowLightPos;
    glm::vec3       m_ShadowLightTarget;
    glm::mat4       m_ShadowProj;
    glm::mat4       m_ShadowView;
    glm::mat4       m_ShadowViewProj;
    glm::mat4       m_ShadowBiasScale;

    VkViewport      m_Viewport = {};
    VkRect2D        m_Scissor = {};

    // Render targets and renderpass
    CRenderTargetArray<1> m_ShadowMapRT;
};
