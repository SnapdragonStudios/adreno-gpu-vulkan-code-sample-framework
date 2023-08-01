//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"

/// Simple projected shadow.
/// Platform agnostic base class.
class Shadow
{
    Shadow(const Shadow&) = delete;
    Shadow& operator=(const Shadow&) = delete;
public:
    Shadow();

    bool Initialize(uint32_t shadowMapWidth, uint32_t shadowMapHeight, bool addColorTarget);

    void SetLightPos(const glm::vec3& lightPos, const glm::vec3& lightTarget);
    glm::vec3 GetLightPos() const;
    void SetEyeClipPlanes(float eyeCameraFov, float cameraAspect, float eyeNearPlane, float shadowFarPlane);

    void Update(const glm::mat4& eyeViewMatrix);

    const auto& GetViewProj() const { return m_ShadowViewProj; }
    const auto& GetProjection() const { return m_ShadowProj; }
    const auto& GetView() const { return m_ShadowView; }

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
};

// Template definition for Shadow.  Implementation should specialize based on the graphics api.
template<typename T_GFXAPI>
class ShadowT : public Shadow
{
    ~ShadowT() noexcept = delete;    // Enforce specialization of this class
    static_assert(sizeof(ShadowT<T_GFXAPI>) != sizeof(Shadow));   // Ensure this class template is specialized (and not used as-is)
};
