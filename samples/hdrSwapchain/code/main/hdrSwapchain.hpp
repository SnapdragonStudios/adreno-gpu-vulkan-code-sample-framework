//===========================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//===========================================================================================
#pragma once

///
/// @file hdrSwapchain.hpp
/// @brief Application showing how to change between different swap-chain (HDR) formats.
/// Provides a user selectable dropdown populated with populated with available formats (for the current display device).
/// Also demonstrates the use of VK_QCOM_render_pass_transform Vulkan extension to rotate the buffers to the device native orientation.
/// 

#include "main/applicationHelperBase.hpp"
#include "materials.hpp"
#include "vulkan/MeshObject.h"
#include "vulkan/vulkan_support.hpp"
#include "vulkan/TextureFuncts.h"
#include "system/glm_common.hpp"
#include "camera/camera.hpp"
#include "shadow/shadow.hpp"

#include <memory>
#include <map>
#include <string>
#include "tcb/span.hpp" // replace with c++20 <span>

// Forward declarations
class ShaderManager;
class MaterialManager;
class VulkanTexInfo;
class MaterialPass;
class Material;
class Computable;
class Drawable;
class DrawablePass;
class Gui;

enum RENDER_PASS
{
    RP_GBUFFER = 0,
    RP_SHADOW,
    RP_LIGHT,
    RP_HUD,
    RP_BLIT,
    NUM_RENDER_PASSES
};

// Class
class Application : public ApplicationHelperBase
{
public:
    Application();
    ~Application() override;

    // Override FrameworkApplicationBase
    int     PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR>) override;
    bool    Initialize(uintptr_t windowHandle) override;
    void    Destroy() override;
    void    Render(float fltDiffTime) override;

    // Application
    bool    LoadMeshObjects();
    bool    LoadTextures();
    bool    CreateRenderTargets();
    bool    InitShadowMap();
    bool    InitLighting();
    bool    LoadShaders();
    bool    InitUniforms();
    bool    UpdateUniforms(uint32_t WhichBuffer);
    bool    InitMaterials();
    bool    InitCommandBuffers();
    bool    InitAllRenderPasses();
    bool    InitDrawables();
    bool    InitHdr();
    bool    InitGui(uintptr_t windowHandle);

    bool    BuildCmdBuffers();
    bool    InitLocalSemaphores();

    const char* GetPassName(uint32_t WhichPass);

    void    UpdateGui();
    void    UpdateCamera(float ElapsedTime);
    void    UpdateLighting(float ElapsedTime);
    void    UpdateShadowMap(float ElapsedTime);

    bool    ChangeSurfaceFormat(VkSurfaceFormatKHR newSurfaceFormat);

    void    BeginRenderPass(RENDER_PASS WhichPass);
    void    AddPassCommandBuffers(RENDER_PASS WhichPass);
    void    AddPassCommandBuffers(RENDER_PASS WhichPass, tcb::span<VkCommandBuffer> SubCommandBuffers);
    void    AddPassCommandBuffer(RENDER_PASS WhichPass, Wrap_VkCommandBuffer& SubCommandBuffer)
    {
        AddPassCommandBuffers(WhichPass, {&SubCommandBuffer.m_VkCommandBuffer, 1});
    }
    void    EndRenderPass(RENDER_PASS WhichPass);
    void    SubmitRenderPass(RENDER_PASS WhichPass, const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, VkFence CompletionFence = VK_NULL_HANDLE);

    VulkanTexInfo* GetOrLoadTexture(const char* textureName);

private:
    // Private variable to hold which "swapchain" we are on
    uint32_t            m_CurrentVulkanBuffer;

    // Metrics
    uint32_t            m_TotalDrawCalls;
    uint32_t            m_TotalTriangles;

    // Requested surface format (for changing formats)
    VkSurfaceFormatKHR  m_RequestedSurfaceFormat;
    // sRGB output (done in blit shader) on/off
    bool                m_bEncodeSRGB;

    // Drawables
    std::vector<Drawable>       m_SceneObject;
    std::unique_ptr<Drawable>   m_LightDrawable;
    std::unique_ptr<Drawable>   m_BlitDrawable;
    // Computables
    std::unique_ptr<Computable> m_VsmComputable;
    std::unique_ptr<Computable> m_ComputableTest;
    std::unique_ptr<Computable> m_BloomComputable;
    std::unique_ptr<Computable> m_NNAOComputable;

    // Textures
    std::map<std::string, VulkanTexInfo> m_loadedTextures;
    VulkanTexInfo                        m_TexWhite;
    VulkanTexInfo                        m_DefaultNormal;

    // Shaders
    std::unique_ptr<ShaderManager> m_shaderManager;

    // Materials
    std::unique_ptr<MaterialManager> m_MaterialManager;

    // Light Stuff
    glm::vec4               m_LightColor;

    // Shadow Map stuff
    static const int        cNumShadows = 1;
    std::array<Shadow, cNumShadows> m_Shadows;

    // **********************
    // The Object
    // **********************
    float                   m_ObjectScale;
    glm::vec3               m_ObjectWorldPos;

    UniformT<ObjectVertUB>  m_ObjectVertUniform[NUM_RENDER_PASSES][NUM_VULKAN_BUFFERS];
    ObjectVertUB            m_ObjectVertUniformData;
    UniformT<ObjectFragUB>  m_ObjectFragUniform[NUM_RENDER_PASSES][NUM_VULKAN_BUFFERS];
    ObjectFragUB            m_ObjectFragUniformData;

    Wrap_VkCommandBuffer    m_ObjectCmdBuffer[NUM_VULKAN_BUFFERS][NUM_RENDER_PASSES];

    // **********************
    // Deferred Lighting
    // **********************
    Wrap_VkCommandBuffer    m_LightCmdBuffer[NUM_VULKAN_BUFFERS];
    struct LightFragCtrl {
        static const int cNUM_LIGHTS = 5;
        glm::mat4 ProjectionInv;
        glm::mat4 ViewInv;
        glm::mat4 ViewProjectionInv; // ViewInv * ProjectionInv
        glm::mat4 WorldToShadow;
        glm::vec4 ProjectionInvW; // W components of ProjectionInv
        glm::vec4 CameraPos;

        glm::vec4 LightPositions[cNUM_LIGHTS];  // w is intensity (inv square falloff)
        glm::vec4 LightColors[cNUM_LIGHTS];

        glm::vec4 AmbientColor;

        float AmbientOcclusionScale;
        int Width;
        int Height;

    } m_LightFragUniformData;
    UniformT<LightFragCtrl>  m_LightFragUniform[NUM_VULKAN_BUFFERS];

    // **********************
    // Post/Blit
    // **********************
    struct BlitFragCtrl {
        float Bloom = 2.0f;
        float Diffuse = 1.0f;   // 0 to 2 range (dark to white)
        int sRGB = 0;           // 1 - apply srgb conversion in output blit shader, 0 passthrough color
    } m_BlitFragUniformData;
    UniformT<BlitFragCtrl>  m_BlitFragUniform[NUM_VULKAN_BUFFERS];
    Wrap_VkCommandBuffer    m_BlitCmdBuffer[NUM_VULKAN_BUFFERS];

    // **********************
    // Compute
    // **********************
    struct ComputeCtrl {
        int width, height;
    } m_ComputeCtrl;
    struct NNAOCtrl {
        // Output texture size
        int Width;
        int Height;
        int _align[2];
        // X: Screen Width
        // Y: Screen Height
        // Z: One Width Pixel
        // W: One Height Pixel
        glm::vec4   ScreenSize;
        // Camera projection
        glm::mat4   CameraProjection;
        // Camera inverse projection
        glm::mat4   CameraInvProjection;
    } m_NNAOCtrl;
    UniformT<ComputeCtrl>   m_ComputeCtrlUniform;
    UniformT<ComputeCtrl>   m_ComputeCtrlUniformHalf;
    UniformT<ComputeCtrl>   m_ComputeCtrlUniformQuarter;
    UniformT<NNAOCtrl>      m_NNAOCtrlUniform[NUM_RENDER_PASSES];
    VulkanTexInfo           m_VsmTarget;
    VulkanTexInfo           m_ComputeIntermediateHalfTarget;
    VulkanTexInfo           m_ComputeIntermediateHalf2Target;
    VulkanTexInfo           m_ComputeIntermediateQuarterTarget;
    VulkanTexInfo           m_ComputeIntermediateQuarter2Target;
    VulkanTexInfo           m_ComputeRenderTarget;
    VulkanTexInfo           m_BloomRenderTarget;
    VulkanTexInfo           m_NNAORenderTarget;
    VulkanTexInfo           m_NNAOTempTarget;

    Wrap_VkCommandBuffer    m_VsmComputeCmdBuffer[NUM_VULKAN_BUFFERS];      // Command buffer to run VSM compute commands on the (regular) graphics queue 
    Wrap_VkCommandBuffer    m_VsmAsyncComputeCmdBuffer[NUM_VULKAN_BUFFERS]; // Command buffer for VSM on Async Compute queue (needs separate command buffer since command pool is tied to the destination queue)
    Wrap_VkCommandBuffer    m_DiffractionDownsampleCmdBuffer[NUM_VULKAN_BUFFERS];
    Wrap_VkCommandBuffer    m_BloomComputeCmdBuffer[NUM_VULKAN_BUFFERS];
    Wrap_VkCommandBuffer    m_BloomAsyncComputeCmdBuffer[NUM_VULKAN_BUFFERS];
    Wrap_VkCommandBuffer    m_NNAOComputeCmdBuffer[NUM_VULKAN_BUFFERS];
    Wrap_VkCommandBuffer    m_NNAOAsyncComputeCmdBuffer[NUM_VULKAN_BUFFERS];


    // **********************
    // Pass Stuff
    // **********************
    struct PassSetup {

        std::vector<VkFormat>   ColorFormats;
        VkFormat                DepthFormat;
        RenderPassInputUsage    ColorInputUsage;
        bool                    ClearDepthRenderPass;
        RenderPassOutputUsage   ColorOutputUsage;
        RenderPassOutputUsage   DepthOutputUsage;
        glm::vec4               ClearColor;
        uint32_t                TargetWidth;
        uint32_t                TargetHeight;

    } m_PassSetup[NUM_RENDER_PASSES];

    Wrap_VkCommandBuffer    m_PassCmdBuffer[NUM_VULKAN_BUFFERS][NUM_RENDER_PASSES];
    std::array<VkSemaphore, NUM_RENDER_PASSES>  m_PassCompleteSemaphore;
    VkSemaphore             m_VsmAsyncComputeCanStartSemaphore;
    VkSemaphore             m_VsmAsyncComputeCompleteSemaphore;
    VkSemaphore             m_BloomAsyncComputeCanStartSemaphore;
    VkSemaphore             m_BloomAsyncComputeCompleteSemaphore;
    VkSemaphore             m_NNAOAsyncComputeCanStartSemaphore;
    VkSemaphore             m_NNAOAsyncComputeCompleteSemaphore;

    // Don't actually need a render pass for each Vulkan Buffer, just one per pass
    // These are NOT the same vkrenderpass's as in the CRenderTargetArray's but should be compatible with them (are allowed dfferent load/clear parameters on the atachments)
    std::array<VkRenderPass, NUM_RENDER_PASSES> m_RenderPass;

    // Render targets for each pass
    CRenderTargetArray<1>  m_GBufferRT;
    CRenderTargetArray<1>  m_MainRT;
    CRenderTargetArray<1>  m_HudRT;
};
