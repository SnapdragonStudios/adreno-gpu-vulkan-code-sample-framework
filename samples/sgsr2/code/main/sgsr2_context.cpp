//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "sgsr2_context.hpp"
#include "material/vulkan/computable.hpp"
#include "material/vulkan/materialManager.hpp"
#include "material/vulkan/shaderManager.hpp"
#include "vulkan/commandBuffer.hpp"
#include "memory/vulkan/uniform.hpp"
#include <cassert>

namespace
{
    constexpr uint32_t DivideRoundUp( uint32_t dividend, uint32_t divisor )
    {
        return (dividend + divisor - 1) / divisor;
    }

    consteval float Halton( int32_t Index, int32_t Base )
    {
        float Result = 0.0f;
        float InvBase = 1.0f / Base;
        float Fraction = InvBase;
        while (Index > 0) {
            Result += (Index % Base) * Fraction;
            Index /= Base;
            Fraction *= InvBase;
        }
        return Result;
    }
    template<int32_t T_SIZE> std::array<glm::vec2, T_SIZE> consteval MakeHaltonArray( int32_t BaseX, int32_t BaseY )
    {
        std::array<glm::vec2, T_SIZE> Array;
        int count = 0;
        for (int32_t Index = 0; Index < T_SIZE; ++Index)
            Array[Index] = {Halton( Index + 1, BaseX ) - 0.5f, Halton( Index + 1, BaseY ) - 0.5f};
        return Array;
    }
    constexpr std::array<glm::vec2, 32> cJitterPositions = MakeHaltonArray<32>( 2, 3 );
}

SGSR2::Context::Context()
{
}

bool SGSR2::Context::Initialize(
    GraphicsApiBase& gfxApi, 
    const ShaderManager<Vulkan>& shaderManager, 
    const MaterialManager<Vulkan>& materialManager,
    const UpscalerConfiguration& upscaler_configuration,
    const InputImages& input_images )
{
    auto& vulkan    = static_cast<Vulkan&>(gfxApi);
    m_configuration = upscaler_configuration;

    //////////////////////
    // RETRIEVE SHADERS //
    //////////////////////

    const auto* sgsr2_shader = shaderManager.GetShader("sgsr2");
    if (!sgsr2_shader)
    {
        return false;
    }

    /////////////////////////
    // POINT SAMPLE INPUTS //
    /////////////////////////

    m_input_opaque_color_ref = std::make_unique<TextureVulkan>( std::move( CreateTextureObjectView( vulkan, *input_images.opaque_color, input_images.opaque_color->Format ) ) );
    m_input_opaque_color_ref->Sampler = CreateSampler( vulkan, SamplerAddressMode::ClampBorder, SamplerFilter::Nearest, SamplerBorderColor::TransparentBlackFloat, 0.0f );
    m_input_color_ref = std::make_unique<TextureVulkan>( std::move( CreateTextureObjectView( vulkan, *input_images.color, input_images.color->Format ) ) );
    m_input_color_ref->Sampler = CreateSampler( vulkan, SamplerAddressMode::ClampBorder, SamplerFilter::Nearest, SamplerBorderColor::TransparentBlackFloat, 0.0f );
    m_input_depth_ref = std::make_unique<TextureVulkan>( std::move( CreateTextureObjectView( vulkan, *input_images.depth, input_images.depth->Format ) ) );
    m_input_depth_ref->Sampler = CreateSampler( vulkan, SamplerAddressMode::ClampBorder, SamplerFilter::Nearest, SamplerBorderColor::TransparentBlackFloat, 0.0f );
    m_input_velocity_ref = std::make_unique<TextureVulkan>( std::move( CreateTextureObjectView( vulkan, *input_images.velocity, input_images.velocity->Format ) ) );
    m_input_velocity_ref->Sampler = CreateSampler( vulkan, SamplerAddressMode::ClampBorder, SamplerFilter::Nearest, SamplerBorderColor::TransparentBlackFloat, 0.0f );

    /////////////////////////
    // SHADER DATA UNIFORM //
    /////////////////////////

    if (!CreateUniformBuffer(&vulkan, m_upscaler_uniform))
    {
        return false;
    }

    ///////////////////////////
    // INTERMEDIATE TEXTURES //
    ///////////////////////////

    enum class SGSRSamplerType
    {
        NEAREST_CLAMP,
        LINEAR_REPEAT,
        LINEAR_CLAMP,
    };

    auto CreateTextureWithSampler = [&](
        auto& target_unique_ptr,
        uint32_t uiWidth, 
        uint32_t uiHeight, 
        TextureFormat Format, 
        const char* pName, 
        SGSRSamplerType samplerType)
    {
        SamplerFilter samplerFilter = SamplerFilter::Undefined;
        SamplerAddressMode samplerAddressMode = SamplerAddressMode::Undefined;
        switch (samplerType)
        {
            case SGSRSamplerType::NEAREST_CLAMP:
            {
                samplerFilter = SamplerFilter::Nearest;
                samplerAddressMode = SamplerAddressMode::ClampEdge;
                break;
            }
            case SGSRSamplerType::LINEAR_REPEAT:
            {
                samplerFilter = SamplerFilter::Linear;
                samplerAddressMode = SamplerAddressMode::Repeat;
                break;
            }
            case SGSRSamplerType::LINEAR_CLAMP:
            {
                samplerFilter = SamplerFilter::Linear;
                samplerAddressMode = SamplerAddressMode::ClampEdge;
                break;
            }
        }

        target_unique_ptr = std::make_unique<TextureVulkan>( std::move(CreateTextureObject(
            vulkan, 
            CreateTexObjectInfo{
                .uiWidth = uiWidth,
                .uiHeight = uiHeight,
                .Format = Format,
                .TexType = TEXTURE_TYPE::TT_COMPUTE_TARGET,
                .pName = pName,
                .FilterMode = samplerFilter,
                .SamplerMode = samplerAddressMode
            })));
    };

    // Depth -> NEAREST_CLAMP
    
    CreateTextureWithSampler(
        m_motion_depth_alpha_buffer,
        upscaler_configuration.render_size.x,
        upscaler_configuration.render_size.y,
        TextureFormat::R16G16B16A16_SFLOAT,
        "motion depth alpha buffer", 
        SGSRSamplerType::NEAREST_CLAMP);

    CreateTextureWithSampler(
        m_color_YCoCg,
        upscaler_configuration.render_size.x,
        upscaler_configuration.render_size.y,
        TextureFormat::R32_UINT,
        "color luma", 
        SGSRSamplerType::NEAREST_CLAMP);

    CreateTextureWithSampler(
        m_motion_depth_clip_alpha_buffer,
        upscaler_configuration.render_size.x,
        upscaler_configuration.render_size.y,
        TextureFormat::R16G16B16A16_SFLOAT,
        "motion depth clip alpha buffer", 
        SGSRSamplerType::LINEAR_CLAMP);

    CreateTextureWithSampler(
        m_luma,
        upscaler_configuration.render_size.x,
        upscaler_configuration.render_size.y,
        TextureFormat::R32_UINT,
        "luma", 
        SGSRSamplerType::NEAREST_CLAMP);

    CreateTextureWithSampler(
        m_luma_history,
        upscaler_configuration.render_size.x,
        upscaler_configuration.render_size.y,
        TextureFormat::R32_UINT,
        "luma history", 
        SGSRSamplerType::NEAREST_CLAMP);

    CreateTextureWithSampler(
        m_scene_feedback,
        upscaler_configuration.display_size.x,
        upscaler_configuration.display_size.y,
        TextureFormat::R16G16B16A16_SFLOAT,
        "prev history", 
        SGSRSamplerType::LINEAR_CLAMP);

    CreateTextureWithSampler(
        m_scene_feedback_history,
        upscaler_configuration.display_size.x,
        upscaler_configuration.display_size.y,
        TextureFormat::R16G16B16A16_SFLOAT,
        "history output", 
        SGSRSamplerType::LINEAR_CLAMP);

    ////////////////////
    // OUTPUT TEXTURE //
    ////////////////////
                
    m_scene_color_output = std::make_unique<TextureVulkan>(std::move(CreateTextureObject(
        vulkan, 
        upscaler_configuration.display_size.x, 
        upscaler_configuration.display_size.y,
        TextureFormat::R16G16B16A16_SFLOAT, 
        TEXTURE_TYPE::TT_COMPUTE_TARGET,
        "scene color output")));
    
    ///////////////////////
    // COMPUTE MATERIALS //
    ///////////////////////

    auto material = materialManager.CreateMaterial(
        *sgsr2_shader,
        vulkan.m_SwapchainImageCount * 2/*so we always have an even number of descriptors, which makes ping-ponging between history buffers work! */,
        [&](const std::string& texName) -> MaterialManager<Vulkan>::tPerFrameTexInfo
        {
            // Inputs
            if (texName == "IN_OpaqueColor")
                return {m_input_opaque_color_ref.get()};
            else if (texName == "IN_Color")
                return {m_input_color_ref.get()};
            else if (texName == "IN_Depth")
                return {m_input_depth_ref.get()};
            else if (texName == "IN_Velocity")
                return {m_input_velocity_ref.get()};

            // Outputs
            else if (texName == "VAR_YCoCgColor")
                return { m_color_YCoCg.get() };
            else if (texName == "VAR_MotionDepthAlphaBuffer")
                return { m_motion_depth_alpha_buffer.get() };
            else if (texName == "VAR_PrevLumaHistory")
                return {m_luma_history.get(), m_luma.get()};
            else if (texName == "VAR_MotionDepthAlphaBuffer")
                return { m_motion_depth_alpha_buffer.get() };
            else if (texName == "VAR_YCoCgColor")
                return {m_color_YCoCg.get()};
            else if (texName == "VAR_MotionDepthClipAlphaBuffer")
                return {m_motion_depth_clip_alpha_buffer.get()};
            else if (texName == "VAR_LumaHistory")
                return {m_luma.get(), m_luma_history.get()};
            else if (texName == "VAR_PrevHistory")
                return {m_scene_feedback_history.get(), m_scene_feedback.get()};
            else if (texName == "VAR_MotionDepthAlphaBuffer")
                return { m_motion_depth_alpha_buffer.get() };
            else if (texName == "VAR_YCoCgColor")
                return {m_color_YCoCg.get()};
            else if (texName == "OUT_HistoryOutput")
                return {m_scene_feedback.get(), m_scene_feedback_history.get()};
            else if (texName == "OUT_SceneColor")
                return {m_scene_color_output.get()};
            assert(0);
            return {};
        }, 
        [&](const std::string& bufferName) -> PerFrameBuffer<Vulkan>
        {
            if (bufferName == "ShaderData")
            {
                return {m_upscaler_uniform.bufferHandles.begin(), m_upscaler_uniform.bufferHandles.begin() + vulkan.m_SwapchainImageCount};
            }
            assert(0);
            return {};
        });

    ////////////////////////
    // CREATE COMPUTABLES //
    ////////////////////////

    // Create the computable to execute the material
    auto computable = std::make_unique<Computable<Vulkan>>(vulkan, std::move(material));
    if (!computable->Init())
    {
        LOGE("Error Creating SGSR computables...");
    }
    else
    {
        // Calculate dispatch size
        uint32_t dx = DivideRoundUp(upscaler_configuration.render_size.x, 8);
        uint32_t dy = DivideRoundUp(upscaler_configuration.render_size.y, 8);

        computable->SetDispatchGroupCount(0, {dx, dy, 1});  // convert
        computable->SetDispatchGroupCount(1, {dx, dy, 1});  // activate

        dx = DivideRoundUp(upscaler_configuration.display_size.x, 8);
        dy = DivideRoundUp(upscaler_configuration.display_size.y, 8);
        computable->SetDispatchGroupCount(2, {dx, dy, 1});  // upscale
    }
    m_computable = std::move(computable);

    return true;
}

void SGSR2::Context::Release(GraphicsApiBase& gfxApi)
{
    m_computable.reset();
    
#define ReleaseTexture(x) if (x) x->Release(&gfxApi); x.reset();

    ReleaseTexture(m_motion_depth_alpha_buffer);
    ReleaseTexture(m_color_YCoCg);
    ReleaseTexture(m_motion_depth_clip_alpha_buffer);
    ReleaseTexture(m_luma);
    ReleaseTexture(m_luma_history);
    ReleaseTexture(m_scene_feedback);
    ReleaseTexture(m_scene_feedback_history);
    ReleaseTexture(m_scene_color_output);

#undef ReleaseTexture

    ReleaseUniformBuffer(&static_cast<Vulkan&>(gfxApi), m_upscaler_uniform);
}

const TextureBase* const SGSR2::Context::GetSceneColorOutput() const
{
    return m_scene_color_output.get();
}

void SGSR2::Context::UpdateUniforms(
    Vulkan&          vulkan, 
    const Camera&    camera,
    const glm::mat4& prev_view_proj_matrix)
{
    auto IsCameraStill = [](const glm::mat4& curVP, const glm::mat4& prevVP, float threshold=1e-5)
    {
        float vpDiff = 0;
        for(int i = 0; i < 4; i++)
        {
            for(int j = 0; j < 4; j++)
            {
                vpDiff += abs(curVP[i][j] - prevVP[i][j]);
            }
        }
        if (vpDiff < threshold) 
        {
            return true;
        }
        return false;
    };

    const auto camera_view                  = camera.ViewMatrix();
    const auto camera_proj                  = camera.ProjectionMatrixNoJitter();
    const auto curr_view_proj_matrix        = camera_proj * camera_view;
    const auto inv_current_view_proj_matrix = glm::inverse( camera_view ) * glm::inverse( camera_proj );
    const auto mt                           = prev_view_proj_matrix * inv_current_view_proj_matrix;

    const bool is_camera_still              = IsCameraStill(curr_view_proj_matrix, prev_view_proj_matrix, 1e-5);

    m_camera_still_frame_count              = is_camera_still ? m_camera_still_frame_count + 1 : 0;

    const float camera_fov_angle_horizontal = glm::tan( camera.Fov() / 2.0f ) * float( m_configuration.render_size.x ) / float( m_configuration.render_size.y );

    m_upscaler_uniform_data.render_size                 = m_configuration.render_size;
    m_upscaler_uniform_data.display_size                = m_configuration.display_size;
    m_upscaler_uniform_data.render_size_rcp             = {
        float(1.0) / float(m_configuration.render_size.x),
        float(1.0) / float(m_configuration.render_size.y)};
    m_upscaler_uniform_data.display_size_rcp            = {
        float(1.0) / float(m_configuration.display_size.x),
        float(1.0) / float(m_configuration.display_size.y)};
    m_upscaler_uniform_data.jitter_offset               = GetJitter();
    m_upscaler_uniform_data.clip_to_prev_clip           = mt;
    m_upscaler_uniform_data.pre_exposure                = 1.0f;
    m_upscaler_uniform_data.camera_fov_angle_horizontal = camera_fov_angle_horizontal;
    m_upscaler_uniform_data.camera_near                 = camera.NearClip();
    m_upscaler_uniform_data.min_lerp_contribution       = m_camera_still_frame_count > 5 ? 0.3f : 0.0f;
    m_upscaler_uniform_data.is_same_camera              = is_camera_still ? 1 : 0;
    m_upscaler_uniform_data.reset                       = false;

    UpdateUniformBuffer(&vulkan, m_upscaler_uniform, m_upscaler_uniform_data, m_buffer_index % vulkan.m_SwapchainImageCount);
}

void SGSR2::Context::Dispatch(
    Vulkan&                      vulkan,
    CommandListVulkan&           command_list) 
{
    for (const auto& computablePass : m_computable->GetPasses())
    {
        m_computable->DispatchPass( command_list, computablePass, m_buffer_index );
    }

    m_buffer_index = (m_buffer_index + 1) % (vulkan.m_SwapchainImageCount * 2);
    m_jitter_index++;
}

glm::vec2 SGSR2::Context::GetJitter() const
{
    return cJitterPositions[m_jitter_index % cJitterPositions.size()];
}
