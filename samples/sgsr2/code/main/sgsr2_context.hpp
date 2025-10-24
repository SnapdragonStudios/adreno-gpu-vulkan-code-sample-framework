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

template<typename T> class Computable;
template<typename T> class MaterialManager;
template<typename T> class ShaderManager;
class TextureBase;

class GraphicsApiBase;
class Shadow;

namespace SGSR2
{

constexpr float JitterScale = 2.0f;

struct UpscalerConfiguration 
{
    glm::uvec2                  render_size;
    glm::uvec2                  display_size;
};

struct UpscalerShaderData 
{
    glm::uvec2                  render_size;
    glm::uvec2                  display_size;
    glm::vec2                   render_size_rcp;
    glm::vec2                   display_size_rcp;
    glm::vec2                   jitter_offset;
    glm::vec2                   padding;
    glm::mat4                   clip_to_prev_clip;
    float                       pre_exposure;
    float                       camera_fov_angle_horizontal;
    float                       camera_near;
    float                       min_lerp_contribution;
    uint32_t                    is_same_camera;
    uint32_t                    reset;
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
        const ShaderManager<Vulkan>&, 
        const MaterialManager<Vulkan>&,
        const UpscalerConfiguration&,
        const InputImages&);
    void Release(GraphicsApiBase&);

    const TextureBase* const GetSceneColorOutput() const;

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
    std::unique_ptr<Computable<Vulkan>> m_computable;

    UpscalerConfiguration               m_configuration;

    UniformArrayT<Vulkan, UpscalerShaderData, NUM_VULKAN_BUFFERS> m_upscaler_uniform;
    UpscalerShaderData                  m_upscaler_uniform_data;

    // References to input textures (with point sampling explicitly enabled)
    std::unique_ptr<TextureVulkan>      m_input_opaque_color_ref;
    std::unique_ptr<TextureVulkan>      m_input_color_ref;
    std::unique_ptr<TextureVulkan>      m_input_depth_ref;
    std::unique_ptr<TextureVulkan>      m_input_velocity_ref;

    // Intermediate texture buffers
    std::unique_ptr<TextureVulkan>      m_motion_depth_alpha_buffer;
    std::unique_ptr<TextureVulkan>      m_color_YCoCg;
    std::unique_ptr<TextureVulkan>      m_motion_depth_clip_alpha_buffer;

    std::unique_ptr<TextureVulkan>      m_luma;
    std::unique_ptr<TextureVulkan>      m_luma_history;

    std::unique_ptr<TextureVulkan>      m_scene_feedback;
    std::unique_ptr<TextureVulkan>      m_scene_feedback_history;

    std::unique_ptr<TextureVulkan>      m_scene_color_output;

    int                                 m_jitter_index             = 0;
    int                                 m_buffer_index             = 0;
    uint32_t                            m_camera_still_frame_count = 0;
};

};
