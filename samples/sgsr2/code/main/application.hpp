//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

///
/// Sample app demonstrating the loading of a .gltf file (hello world)
///
#pragma once

#include "main/applicationHelperBase.hpp"
#include "memory/vulkan/uniform.hpp"
#include "vulkan/commandBuffer.hpp"
#include "vulkan/renderPass.hpp"
#include <unordered_map>

#define NUM_SPOT_LIGHTS 4

namespace SGSR2
{
    class Context;
};
namespace SGSR2_Frag
{
    class Context;
};

enum RENDER_PASS
{
    RP_SCENE = 0,
    RP_HUD,
    RP_BLIT,
    NUM_RENDER_PASSES
};

// **********************
// Uniform Buffers
// **********************
struct ObjectVertUB
{
    glm::mat4   MVPMatrix;
    glm::mat4   ModelMatrix;
    glm::mat4   ShadowMatrix;
};

struct ObjectFragUB
{
    glm::vec4   Color;
    glm::vec4   ORM;
};

/*
* Data used by all SGSR2 passes
*/
struct UpscalerUB
{
    glm::vec4   Color;
    glm::vec4   ORM;
};

struct LightUB
{
    glm::mat4 ProjectionInv;
    glm::mat4 ViewInv;
    glm::mat4 ViewProjectionInv; // ViewInv * ProjectionInv
    glm::vec4 ProjectionInvW;    // w components of ProjectionInv
    glm::vec4 CameraPos;

    glm::vec4 LightDirection = glm::vec4(-0.564000f, 0.826000f, 0.000000f, 0.0f);
    glm::vec4 LightColor = glm::vec4(1.000000f, 1.000000f, 1.000000f, 1.000000);

    glm::vec4 SpotLights_pos[NUM_SPOT_LIGHTS];
    glm::vec4 SpotLights_dir[NUM_SPOT_LIGHTS];
    glm::vec4 SpotLights_color[NUM_SPOT_LIGHTS];

    glm::vec4 AmbientColor = glm::vec4(0.340000f, 0.340000f, 0.340000f, 0.0f);

    int Width;
    int Height;
};

// **********************
// Render Pass
// **********************
struct RenderPassSetupInfo
{
    RenderPassInputUsage    ColorInputUsage;
    bool                    ClearDepthRenderPass;
    RenderPassOutputUsage   ColorOutputUsage;
    RenderPassOutputUsage   DepthOutputUsage;
    glm::vec4               ClearColor;
};

struct RenderPassData
{
    // Pass internal data
    RenderPassSetupInfo                 RenderPassSetup;
    std::vector<RenderContext<Vulkan>>  RenderContext;  // context per framebuffer (some passes might all point to the same framebuffers)

    // Recorded objects that are set to be drawn on this pass (secondary)
    std::vector<CommandListVulkan>      ObjectsCmdBuffer;

    // Render targed used by the underlying render pass
    // note: The blit pass uses the backbuffer targets directly instead this RT
    RenderTarget<Vulkan>                RenderTarget;
};

// **********************
// Application
// **********************
class Application : public ApplicationHelperBase
{
    struct ObjectMaterialParameters
    {
        UniformT<ObjectFragUB>  objectFragUniform;
        ObjectFragUB            objectFragUniformData;

        std::size_t GetHash() const
        {
            auto hash_combine = [](std::size_t seed, const float& v) -> std::size_t
            {
                std::hash<float> hasher;
                seed ^= hasher(v) + 0x9e3228b9 + (seed << 6) + (seed >> 2);
                return seed;
            };

            std::size_t result = 0;
            result = hash_combine(result, objectFragUniformData.Color.x);
            result = hash_combine(result, objectFragUniformData.Color.y);
            result = hash_combine(result, objectFragUniformData.Color.z);
            result = hash_combine(result, objectFragUniformData.Color.w);
            result = hash_combine(result, objectFragUniformData.ORM.r);
            result = hash_combine(result, objectFragUniformData.ORM.g);
            result = hash_combine(result, objectFragUniformData.ORM.b);
            result = hash_combine(result, objectFragUniformData.ORM.a);

            return result;
        };
    };

public:
    Application();
    ~Application() override;

    // ApplicationHelperBase
    virtual void PreInitializeSetVulkanConfiguration( Vulkan::AppConfiguration& ) override;
    virtual bool Initialize(uintptr_t windowHandle, uintptr_t hInstance) override;
    virtual void Destroy() override;
    virtual void Render(float fltDiffTime) override;

private:

    // Application - Initialization
    bool InitializeLights();
    bool InitializeCamera();
    bool LoadShaders();
    bool CreateRenderTargets();
    bool InitUniforms();
    bool InitAllRenderPasses();
    bool InitSGSR2Context();
    bool InitGui(uintptr_t windowHandle);
    bool LoadMeshObjects();
    bool InitCommandBuffers();
    bool BuildCmdBuffers();



private:

    // Application - Frame
    void BeginRenderPass(uint32_t WhichBuffer, RENDER_PASS WhichPass, uint32_t WhichSwapchainImage);
    void AddPassCommandBuffer(uint32_t WhichBuffer, RENDER_PASS WhichPass);
    void EndRenderPass(uint32_t WhichBuffer, RENDER_PASS WhichPass);
    void SubmitRenderPass(uint32_t WhichBuffer, RENDER_PASS WhichPass, VkSemaphore WaitSemaphores = VK_NULL_HANDLE, VkPipelineStageFlags WaitDstStageMasks = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VkSemaphore SignalSemaphores = VK_NULL_HANDLE, VkFence CompletionFence = (VkFence)nullptr);
    void UpdateGui();
    bool UpdateUniforms(uint32_t WhichBuffer);

private:

    std::unique_ptr<SGSR2::Context>                           m_sgsr2_context;
    std::unique_ptr<SGSR2_Frag::Context>                      m_sgsr2_context_frag;

    // Render passes
    RenderPass                                                m_ObjectRenderPass;

    std::array<RenderPassData, NUM_RENDER_PASSES>             m_RenderPassData;

    // Command lists
    std::array<CommandListVulkan, NUM_VULKAN_BUFFERS>         m_UpscaleAndPostCommandList;

    // UBOs
    UniformArrayT<ObjectVertUB, NUM_VULKAN_BUFFERS>           m_ObjectVertUniform;
    ObjectVertUB                                              m_ObjectVertUniformData;
    UniformArrayT<LightUB, NUM_VULKAN_BUFFERS>                m_LightUniform;
    LightUB                                                   m_LightUniformData;
    std::unordered_map<std::size_t, ObjectMaterialParameters> m_ObjectFragUniforms;

    // Drawables
    std::vector<Drawable>                                     m_SceneDrawables;
    std::unique_ptr<Drawable>                                 m_BlitQuadDrawable;
};
