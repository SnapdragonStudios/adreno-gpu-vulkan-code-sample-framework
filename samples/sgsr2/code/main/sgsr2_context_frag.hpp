//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "main/applicationHelperBase.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/commandBuffer.hpp"
#include "material/computable.hpp"
#include "memory/vulkan/uniform.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <array>
#include <vector>
#include <optional>

class GraphicsApiBase;
class Drawable;
class MaterialManager;
class ShaderManager;
class Shadow;
class Texture;
class CommandList;

namespace SGSR2_Frag
{

struct UpscalerConfiguration 
{
    glm::uvec2                  render_size;
    glm::uvec2                  display_size;
};

struct UpscalerShaderData
{
    glm::mat4                   clipToPrevClip;
    glm::vec2                   renderSize;
    glm::vec2                   outputSize;
    glm::vec2                   renderSizeRcp;
    glm::vec2                   outputSizeRcp;
    glm::vec2                   jitterOffset;
    glm::vec2                   scaleRatio;
    float                       cameraFovAngleHor;
    float                       minLerpContribution;
    float                       reset;
    uint32_t                    bSameCamera;
};

struct InputImages
{
    TextureVulkan* opaque_color;
    TextureVulkan* color;
    TextureVulkan* depth;
    TextureVulkan* velocity;
};

/// SGSR2 upscaler context.
class Context
{
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
public:
    Context();

    bool Initialize(
        GraphicsApiBase&, 
        const ShaderManager&, 
        const MaterialManager&, 
        const UpscalerConfiguration&,
        const InputImages&);
    void Release(GraphicsApiBase&);

    inline const Texture* const GetSceneColorOutput() const 
    {
        return &m_scene_color_output[(m_buffer_index & 1)^1].m_ColorAttachments[0];
    }

    void UpdateUniforms(
        Vulkan&             vulkan,
        const Camera&       camera,
        const glm::mat4&    prev_view_proj_matrix);

    void Dispatch(
        Vulkan&,
        CommandListVulkan&);

    /// @return Current camera jitter (in pixel offsets)
    glm::vec2 GetJitter() const;

protected:

    std::unique_ptr<Drawable>           m_drawable;
    UpscalerConfiguration               m_configuration{};

    UniformArrayT<Vulkan, UpscalerShaderData, NUM_VULKAN_BUFFERS> m_upscaler_uniform;
    UpscalerShaderData                  m_upscaler_uniform_data{};

    // References to input textures (with point sampling explicitly enabled)
    std::unique_ptr<TextureVulkan>      m_input_color_ref;
    std::unique_ptr<TextureVulkan>      m_input_depth_ref;
    std::unique_ptr<TextureVulkan>      m_input_velocity_ref;

    // Convert output render target
    CRenderTargetArray<1>               m_motion_depth_clip_alpha_buffer;
    // Upscale Render target (and history color)
    CRenderTargetArray<2>               m_scene_color_output;

    int                                 m_jitter_index             = 0;
    int                                 m_buffer_index             = 0;
    uint32_t                            m_camera_still_frame_count = 0;
};

};