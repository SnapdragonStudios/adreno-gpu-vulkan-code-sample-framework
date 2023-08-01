//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once
#include "main/applicationHelperBase.hpp"
#include "materials.hpp"
#include "vulkan/MeshObject.h"
#include "vulkan/commandBuffer.hpp"
#include "vulkan/TextureFuncts.h"
#include "vulkanRT/vulkanRT.hpp"
#include "system/glm_common.hpp"
#include "camera/camera.hpp"
#include "memory/vulkan/bufferObject.hpp"
#include "memory/vulkan/uniform.hpp"
#include "shadow/shadowVulkan.hpp"
#include "shadow/shadowVsm.hpp"
#include "shadow/shadowVulkan.hpp"
#include "system/Worker.h"

#include <memory>
#include <map>
#include <string>
#include <span>

// Forward declarations
//class TextureVulkan;
class MaterialPass;
class Material;
class Computable;
class Drawable;
class DrawablePass;
template<typename T_GFXAPI> class DrawIndirectBuffer;
using DrawIndirectBufferObject = DrawIndirectBuffer<Vulkan>;
class Gui;
class SceneRTCullable;
class SceneRTCulled;
class TestMeshAnimator;
class TimerPoolSimple;

enum RENDER_PASS
{
    RP_GBUFFER = 0,
    RP_RAYSHADOW,
    RP_RASTERSHADOW,
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
    int     PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat>) override;
    void    PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& config) override;
    bool    Initialize(uintptr_t windowHandle, uintptr_t instanceHandle) override;
    void    Destroy() override;
    void    Render(float fltDiffTime) override;
    void    LogFps() override;

    // Application
    bool    LoadMeshObjects();
    bool    LoadRayTracingObjects();
    bool    LoadTextures();
    bool    CreateRenderTargets();
    bool    InitShadowMap();
    bool    InitLighting();
    bool    LoadShaders();
    bool    InitUniforms();
    bool    UpdateUniforms(uint32_t WhichBuffer, float ElapsedTime);
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
    void    UpdateSceneRTCulled(SceneRTCulled& sceneRTCulled, glm::vec3 lightPosition, float lightRadius);
    void    UpdateProfiling(uint32_t WhichFrame);

    bool    ChangeSurfaceFormat(SurfaceFormat newSurfaceFormat);

    struct RenderPassData
    {
        static constexpr uint32_t cnMaxPassSemaphores = 2;
        std::string                                 Desc;
        uint32_t                                    WhichBuffer;
        RENDER_PASS                                 WhichPass;
        Wrap_VkCommandBuffer&                       CmdBuff;
        VkSubpassContents                           SubpassContents;
        std::vector<VkSemaphore>                    WaitSemaphores{};
        std::vector<VkPipelineStageFlags>           WaitDstStageMasks{};
        std::vector<VkSemaphore>                    SignalSemaphores{};
    };
    std::vector<RenderPassData> m_RenderPassSubmitData[NUM_VULKAN_BUFFERS];

    RenderPassData& BeginRenderPass(RENDER_PASS WhichPass, uint32_t WhichBuffer, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores, VkSubpassContents SubpassContents);
    RenderPassData& BeginCommandBuffer(RENDER_PASS WhichPass, uint32_t WhichBuffer, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores, VkSubpassContents SubpassContents);
    RenderPassData& BeginRenderPass(Application::RenderPassData&);
    void BeginRenderPass(Wrap_VkCommandBuffer& CmdBuf, RENDER_PASS WhichPass, uint32_t WhichBuffer, VkSubpassContents SubpassContents);

    void    AddPassCommandBuffers(const RenderPassData& RenderPassData);
    void    AddPassCommandBuffers(const RenderPassData& RenderPassData, std::span<VkCommandBuffer> SubCommandBuffers);
    void    EndRenderPass(const RenderPassData& RenderPassData);
    // Submits the queued up m_RenderPassSubmitData, Returns semaphore(s) signalled by the final pass.
    std::span<VkSemaphore> SubmitGameThreadWork( uint32_t WhichBuffer, VkFence CompletionFence );

protected:
    // Threading data
    struct GameThreadInputParam
    {
        // Input
        Vulkan::BufferIndexAndFence CurrentBuffer;
        float           ElapsedTime;
    };

    struct GameThreadOutputParam
    {
        // Output
        int             WhichFrame = -1;
        int             SwapchainPresentIndx;
        VkFence         Fence;
    } m_CompletedThreadOutput;

    CWorker             m_GameThreadWorker;

    // Game thread entry point.
    void                RunGameThread( const GameThreadInputParam& rThreadParam, GameThreadOutputParam& rThreadOutput);

private:

    // Metrics
    uint32_t                    m_TotalDrawCalls;
    uint32_t                    m_TotalTriangles;

    // Requested surface format (for changing formats)
    SurfaceFormat               m_RequestedSurfaceFormat;
    // sRGB output (done in blit shader) on/off
    bool                        m_bEncodeSRGB;

    // Drawables
    std::unique_ptr<Drawable>   m_SkyboxDrawable;
    std::vector<Drawable>       m_SceneDrawables;
    std::unique_ptr<Drawable>   m_ShadowRasterizedDrawable;
    std::unique_ptr<Drawable>   m_ShadowPointLightRQDrawable;
    std::unique_ptr<Drawable>   m_ShadowDirectionalLightRQDrawable;
    std::unique_ptr<Drawable>   m_LightDrawable;
    std::unique_ptr<Drawable>   m_LightRasterizedShadowDrawable;
    std::unique_ptr<Drawable>   m_BlitDrawable;
    // Computables
    std::unique_ptr<Computable> m_ShadowPointLightRQComputable;
    std::unique_ptr<Computable> m_ShadowDirectionalLightRQComputable;
    std::unique_ptr<Computable> m_ShadowRasterizedCullingComputable;

    // Ray tracing
    VulkanRT                    m_vulkanRT;
    std::unique_ptr<SceneRTCullable> m_sceneRayTracable;
    std::unique_ptr<SceneRTCulled> m_sceneRayTracableCulled;

    // Ray trace vertex buffer update (animation)
    std::vector<TestMeshAnimator>       m_sceneAnimatedMeshObjects;
    std::vector<Computable>             m_sceneAnimatedMeshComputables;
    MeshAnimatorUB                      m_sceneAnimatedMeshUniformData;
    UniformArrayT<MeshAnimatorUB, NUM_VULKAN_BUFFERS> m_sceneAnimatedMeshUniform;

    std::map<uint64_t, size_t>          m_sceneObjectTriangleCounts;    // RT objectId to triangle count (for one instance).

    // Textures
    const Texture*                      m_TexWhite;
    const Texture*                      m_DefaultNormal;

    // Light Stuff
    glm::vec4                           m_LightColor;

    // **********************
    // The Objects
    // **********************
    ObjectVertUB                        m_ObjectVertUniformData;
    UniformArrayT<ObjectVertUB, NUM_VULKAN_BUFFERS> m_ObjectVertUniform[NUM_RENDER_PASSES];
    ObjectFragUB                        m_ObjectFragUniformData;
    UniformArrayT<ObjectFragUB, NUM_VULKAN_BUFFERS> m_ObjectFragUniform[NUM_RENDER_PASSES];
    Wrap_VkCommandBuffer                m_ObjectCmdBuffer[NUM_VULKAN_BUFFERS][NUM_RENDER_PASSES];

    // **********************
    // The Skybox
    // **********************
    float                               m_SkyboxScale;
    UniformArrayT<SkyboxVertUB, NUM_VULKAN_BUFFERS> m_SkyboxVertUniform[NUM_RENDER_PASSES];
    SkyboxVertUB                        m_SkyboxVertUniformData;

    // **********************
    // Deferred Lighting Fullscreen pass
    // **********************
    Wrap_VkCommandBuffer                m_LightCmdBuffer[NUM_VULKAN_BUFFERS];
    LightFragCtrl                       m_LightFragUniformData;
    UniformArrayT<LightFragCtrl, NUM_VULKAN_BUFFERS> m_LightFragUniform;

    // **********************
    // Additional deferred pass light objects (additional lights in scene)
    // **********************
    Wrap_VkCommandBuffer                m_AdditionalLightsCmdBuffer[NUM_VULKAN_BUFFERS];
    struct PointLightInstance{
        glm::vec3   Position;
        float       Radius;
        float       Intensity;
    };
    std::vector<PointLightInstance>     m_AdditionalDeferredLightsData;         // w is light intensity
    std::vector<Drawable>               m_AdditionalDeferredLightDrawables;
    std::vector<SceneRTCulled>          m_AdditionalDeferredLightShadowRQ;
    std::vector<UniformArrayT<PointLightUB, NUM_VULKAN_BUFFERS>> m_AdditionalDeferredLightsSharedUniform;

    // **********************
    // Post/Blit
    // **********************
    BlitFragCtrl                        m_BlitFragUniformData;
    UniformT<BlitFragCtrl>              m_BlitFragUniform;
    Wrap_VkCommandBuffer                m_BlitCmdBuffer[NUM_VULKAN_BUFFERS];

    // **********************
    // Compute Ray Queried shadow)
    // **********************
    ShadowRQCtrl                        m_ShadowRQCtrl;
    UniformArrayT<ShadowRQCtrl, NUM_VULKAN_BUFFERS> m_ShadowRQCtrlUniform;
    TextureVulkan                       m_ShadowRayQueryComputeOutput;

    // **********************
    // Pass Stuff
    // **********************
    struct PassSetup {

        std::vector<TextureFormat> ColorFormats;
        TextureFormat           DepthFormat;
        RenderPassInputUsage    ColorInputUsage;
        bool                    ClearDepthRenderPass;
        RenderPassOutputUsage   ColorOutputUsage;
        RenderPassOutputUsage   DepthOutputUsage;
        glm::vec4               ClearColor;
        uint32_t                TargetWidth;
        uint32_t                TargetHeight;

    } m_PassSetup[NUM_RENDER_PASSES];

    Wrap_VkCommandBuffer        m_PassCmdBuffer[NUM_VULKAN_BUFFERS][NUM_RENDER_PASSES];
    VkSemaphore                 m_BlitCompleteSemaphore = VK_NULL_HANDLE;

    // Don't actually need a render pass for each Vulkan Buffer, just one per pass
    // These are NOT the same vkrenderpass's as in the CRenderTargetArray's but should be compatible with them (are allowed dfferent load/clear parameters on the atachments)
    std::array<VkRenderPass, NUM_RENDER_PASSES> m_RenderPass;

    // Render targets for each pass
    CRenderTargetArray<1>       m_GBufferRT;
    CRenderTargetArray<1>       m_ShadowRT;
    CRenderTargetArray<1>       m_MainRT;
    CRenderTargetArray<1>       m_HudRT;

    // Shadow Map stuff
    static const int                            cNumShadows = 1;
    std::array<ShadowVulkan, cNumShadows>       m_Shadows;
    ShadowVSM                                   m_ShadowVSM;
    struct ShadowCullCameraCtrl {
        glm::mat4               MVPMatrix;                  ///< model view projection matrix for entire meshlet object
        glm::vec4               CullPlanes[6];              ///< camera culling planes
    };
    UniformArrayT<ShadowCullCameraCtrl, NUM_VULKAN_BUFFERS> m_ShadowInstanceCullingCameraUniform;
    std::unique_ptr<DrawIndirectBufferObject> m_ShadowInstanceUnculledDrawIndirectBuffer;
    UniformVulkan                       m_ShadowInstanceCullingBuffer;
    BufferT<Vulkan>                      m_ShadowInstanceCulledData;

    // Profiling
    std::unique_ptr<TimerPoolSimple> m_GpuTimerPool;
    std::mutex                  m_GpuTimerPoolMutex;
};
