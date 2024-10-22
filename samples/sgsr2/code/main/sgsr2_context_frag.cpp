//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "sgsr2_context_frag.hpp"
#include "material/drawable.hpp"
#include "material/materialManager.hpp"
#include "material/shaderDescription.hpp"
#include "material/shaderManager.hpp"
#include "mesh/meshHelper.hpp"
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

SGSR2_Frag::Context::Context()
{
}

bool SGSR2_Frag::Context::Initialize(
    GraphicsApiBase& gfxApi, 
    const ShaderManager& shaderManager, 
    const MaterialManager& materialManager,
    const UpscalerConfiguration& upscaler_configuration,
    const InputImages& input_images )
{
    auto& vulkan    = static_cast<Vulkan&>(gfxApi);
    m_configuration = upscaler_configuration;

    //////////////////////
    // RETRIEVE SHADERS //
    //////////////////////

    const auto* sgsr2_shader = shaderManager.GetShader("sgsr2_frag");
    if (!sgsr2_shader)
    {
        return false;
    }

    /////////////////////////
    // POINT SAMPLE INPUTS //
    /////////////////////////

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

    // Intermediate textures

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
                uiWidth, 
                uiHeight,
                1,
                1,
                1,
                Format, 
                TEXTURE_TYPE::TT_COMPUTE_TARGET,
                TEXTURE_FLAGS::None,
                pName,
                1,
                samplerFilter,
                samplerAddressMode,
                false,
            })));
    };

    //
    // Render passes
    //
    std::array<VkRenderPass, 2> renderPasses{};
    std::array< TextureFormat, 1> convertPassOutputFormats{TextureFormat::R16G16B16A16_SFLOAT};
    if (!vulkan.CreateRenderPass( convertPassOutputFormats, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, RenderPassInputUsage::DontCare, RenderPassOutputUsage::StoreReadOnly, false, RenderPassOutputUsage::Discard, &renderPasses[0] ))
    {
        return false;
    }
    std::array< TextureFormat, 1> upscalePassOutputFormats{TextureFormat::R16G16B16A16_SFLOAT};
    if (!vulkan.CreateRenderPass( upscalePassOutputFormats, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, RenderPassInputUsage::DontCare, RenderPassOutputUsage::StoreReadOnly, false, RenderPassOutputUsage::Discard, &renderPasses[1] ))
    {
        return false;
    }

    if (!m_motion_depth_clip_alpha_buffer.Initialize( &vulkan,
                                                      upscaler_configuration.render_size.x,
                                                      upscaler_configuration.render_size.y,
                                                      convertPassOutputFormats,
                                                      TextureFormat::UNDEFINED,
                                                      renderPasses[0],
                                                      VK_NULL_HANDLE,
                                                      {},
                                                      "motion_depth_clip_alpha_buffer" ))
    {
        return false;
    }

    if (!m_scene_color_output.Initialize( &vulkan,
                                          upscaler_configuration.display_size.x,
                                          upscaler_configuration.display_size.y,
                                          upscalePassOutputFormats,
                                          TextureFormat::UNDEFINED,
                                          renderPasses[1],
                                          VK_NULL_HANDLE,
                                          {},
                                          "scene_color_output" ))
    {
        return false;
    }

    //
    // Fullscreen drawable
    //
    Mesh<Vulkan> quadMesh{};
    if (!MeshHelper::CreateMesh<Vulkan>( vulkan.GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, sgsr2_shader->m_shaderDescription->m_vertexFormats, &quadMesh))
    {
        return false;
    }

    // Upscale material
    auto material = materialManager.CreateMaterial(
        gfxApi, 
        *sgsr2_shader,
        vulkan.m_SwapchainImageCount * 2/*so we always have an even number of descriptors, which makes ping-ponging between history buffers work! */,
        [&](const std::string& texName) -> MaterialPass::tPerFrameTexInfo 
        {
            if (texName == "IN_Depth")
                return {m_input_depth_ref.get()};
            else if (texName == "IN_Velocity")
                return {m_input_velocity_ref.get()};
            else if (texName == "IN_Color")
                return {m_input_color_ref.get()};

            else if (texName == "VAR_MotionDepthClipAlphaBuffer")
                return {&m_motion_depth_clip_alpha_buffer[0].m_ColorAttachments[0]};
            else if (texName == "VAR_PrevOutput")
                return {&m_scene_color_output[1].m_ColorAttachments[0], &m_scene_color_output[0].m_ColorAttachments[0]};

            assert(0);
            return {};
        }, 
        [&](const std::string& bufferName) -> tPerFrameVkBuffer
        {
            if (bufferName == "ShaderData")
            {
                return {m_upscaler_uniform.vkBuffers.begin(), m_upscaler_uniform.vkBuffers.begin() + vulkan.m_SwapchainImageCount};
            }
            assert(0);
            return {};
        });

    //
    // Create the drawable to execute the fragment shader passes
    //

    auto drawable = std::make_unique<Drawable>(vulkan, std::move(material));

    static const char* const passNames[] {"CONVERT", "UPSCALE"};
    if (!drawable->Init(renderPasses, passNames, 0x3, std::move(quadMesh)))
    {
        LOGE("Error Creating SGSR computables...");
    }
    m_drawable = std::move(drawable);

    return true;
}

void SGSR2_Frag::Context::Release(GraphicsApiBase& gfxApi)
{
    auto& vulkan = static_cast<Vulkan&>(gfxApi);

    m_drawable.reset();

    m_scene_color_output.Release();
    m_motion_depth_clip_alpha_buffer.Release();

    ReleaseUniformBuffer(&vulkan, m_upscaler_uniform);
}

void SGSR2_Frag::Context::UpdateUniforms(
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

    m_upscaler_uniform_data.renderSize                  = m_configuration.render_size;
    m_upscaler_uniform_data.outputSize                  = m_configuration.display_size;
    m_upscaler_uniform_data.renderSizeRcp               = {
        1.0f / float(m_configuration.render_size.x),
        1.0f / float(m_configuration.render_size.y)};
    m_upscaler_uniform_data.outputSizeRcp               = {
        1.0f / float(m_configuration.display_size.x),
        1.0f / float(m_configuration.display_size.y)};
    m_upscaler_uniform_data.jitterOffset                = GetJitter();
    m_upscaler_uniform_data.scaleRatio                  = {
        float(m_configuration.display_size.x) / float(m_configuration.render_size.x),
        std::min(20.0f, pow( float(m_configuration.display_size.x * m_configuration.display_size.y) / float(m_configuration.render_size.x * m_configuration.render_size.y), 3.0f ))};
    m_upscaler_uniform_data.clipToPrevClip              = mt;
    m_upscaler_uniform_data.cameraFovAngleHor           = camera_fov_angle_horizontal;
    m_upscaler_uniform_data.minLerpContribution         = m_camera_still_frame_count > 5 ? 0.3f : 0.0f;
    m_upscaler_uniform_data.bSameCamera                 = is_camera_still ? 1 : 0;

    UpdateUniformBuffer(&vulkan, m_upscaler_uniform, m_upscaler_uniform_data, m_buffer_index % vulkan.m_SwapchainImageCount);
}

void SGSR2_Frag::Context::Dispatch(
    Vulkan&                      vulkan,
    CommandListVulkan&           command_list) 
{
    command_list.BeginRenderPass( m_motion_depth_clip_alpha_buffer[0], m_motion_depth_clip_alpha_buffer.m_RenderPass, VK_SUBPASS_CONTENTS_INLINE );
    m_drawable->DrawPass( command_list.m_VkCommandBuffer, m_drawable->GetDrawablePasses()[0], m_buffer_index );
    command_list.EndRenderPass();

    command_list.BeginRenderPass( m_scene_color_output[m_buffer_index&1], m_scene_color_output.m_RenderPass, VK_SUBPASS_CONTENTS_INLINE );
    m_drawable->DrawPass( command_list.m_VkCommandBuffer, m_drawable->GetDrawablePasses()[1], m_buffer_index );
    command_list.EndRenderPass();

    m_buffer_index = (m_buffer_index + 1) % (vulkan.m_SwapchainImageCount * 2);
    m_jitter_index++;
}

glm::vec2 SGSR2_Frag::Context::GetJitter() const
{
    return cJitterPositions[m_jitter_index % cJitterPositions.size()];
}
