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
#include "camera/camera.hpp"
#include "memory/vulkan/uniform.hpp"
#include "system/glm_common.hpp"
#include "system/Worker.h"
#include "vulkan/MeshObject.h"
#include "vulkan/commandBuffer.hpp"
#include "vulkan/TextureFuncts.h"
#include "vulkanRT/vulkanRT.hpp"

#include <memory>
#include <map>
#include <string>
#include <span>

#include "SphereCamera.h"

// Forward declarations
class ShaderManager;
class MaterialManager;
//class TextureVulkan;
class MaterialPass;
class Material;
class Computable;
class Drawable;
class Traceable;
class Gui;
class SceneRTCullable;
class SceneRTCulled;
template<typename T_GFXAPI> class BufferT;
using BufferVulkan = BufferT<Vulkan>;

//class VertexBufferObject;

enum RENDER_PASS
{
    RP_GBUFFER = 0,
    RP_RAYTRACE,
    RP_LIGHT,
    RP_HUD,
    RP_BLIT,
    NUM_RENDER_PASSES
};

// The vertex data passed to ray trace shader
struct RayVertex
{
    // Here is validation error if no padding:
    //      Vulkan Debug : (Error)DEVICE = > Validation: Validation Error : [UNASSIGNED - CoreValidation - Shader - InconsistentSpirv] Object 0 : 
    //      handle = 0x20b2557b2a8, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x6bbb14 | SPIR - V module not valid: Structure id 322 decorated 
    //      as BufferBlock for variable in Uniform storage class must follow relaxed storage buffer layout rules : member 0 contains an array with 
    //      stride 72 not satisfying alignment to 16

    // Here is the validation error if elements are on odd boundaries:
    //      Vulkan Debug : (Error)DEVICE = > Validation: Validation Error : [UNASSIGNED - CoreValidation - Shader - InconsistentSpirv] Object 0 : 
    //      handle = 0x1fa92f98f48, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x6bbb14 | SPIR - V module not valid: Structure id 320 decorated 
    //      as BufferBlock for variable in Uniform storage class must follow relaxed storage buffer layout rules : member 1 is an improperly 
    //      straddling vector at offset 12
    //      VertData = OpTypeStruct v3float v3float v4float v2float v3float v3float v2float

    // Here is the validation error if an element is not 16 bytes:
    //      Vulkan Debug: (Error) DEVICE => Validation: Validation Error: [ UNASSIGNED-CoreValidation-Shader-InconsistentSpirv ] Object 0: 
    //      handle = 0x1df48f285f8, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x6bbb14 | SPIR-V module not valid: Structure id 320 decorated 
    //      as BufferBlock for variable in Uniform storage class must follow relaxed storage buffer layout rules: member 4 is an improperly 
    //      straddling vector at offset 56
    //      VertData = OpTypeStruct % v4float % v4float % v4float % v2float % v4float % v4float % v2float
    float position[4];
    float normal[4];
    float uv0[4];
};

typedef std::vector<RayVertex> RayObject;

typedef std::vector<uint32_t> RayObjectIndices;

// Class
class Application : public ApplicationHelperBase
{
public:
    Application();
    ~Application() override;

    // Override ApplicationHelperBase
    int     PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat>) override;
    void    PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& config) override;
    bool    Initialize(uintptr_t windowHandle, uintptr_t hInstance) override;
    void    Destroy() override;
    void    Render(float fltDiffTime) override;

    void    KeyDownEvent(uint32_t key) override;
    void    KeyRepeatEvent(uint32_t key) override;
    void    KeyUpEvent(uint32_t key) override;
    void    TouchDownEvent(int iPointerID, float xPos, float yPos) override;
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos) override;
    void    TouchUpEvent(int iPointerID, float xPos, float yPos) override;

    // Application
    bool        LoadMeshObjects();
    bool        LoadRayTracingObjects();
    bool        LoadTextures();
    bool        CreateRayInputs();
    bool        CreateRenderTargets();
    bool        InitShadowMap();
    bool        InitLighting();
    bool        LoadConstantShaders();
    bool        LoadDynamicShaders();
    bool        LoadDynamicRayTracingObjects();
    bool        InitUniforms();
    glm::mat4   CreateRandomRotation();
    bool        UpdateUniforms(uint32_t WhichBuffer);
    bool        InitMaterials();
    bool        InitCommandBuffers();
    bool        InitAllRenderPasses();
    bool        InitDrawables();
    bool        InitSceneData();
    bool        InitHdr();
    bool        InitGui(uintptr_t windowHandle);

    bool        BuildCmdBuffers();
    bool        InitLocalSemaphores();

    const char* GetPassName(uint32_t WhichPass);

    void    UpdateGui();
    void    UpdateCamera(float ElapsedTime);
    void    UpdateLighting(float ElapsedTime);
    void    UpdateShadowMap(float ElapsedTime);
    void    UpdateSceneRTCulled( SceneRTCulled& sceneRTCulled );

    bool    ChangeSurfaceFormat(SurfaceFormat newSurfaceFormat);

    struct RenderPassData
    {
        static constexpr uint32_t cnMaxPassSemaphores = 2;
        std::string                                 Desc;
        uint32_t                                    WhichBuffer;
        RENDER_PASS                                 WhichPass;
        Wrap_VkCommandBuffer&                       CmdBuff;
        std::vector<VkSemaphore>                    WaitSemaphores{};
        std::vector<VkPipelineStageFlags>           WaitDstStageMasks{};
        std::vector<VkSemaphore>                    SignalSemaphores{};
    };
    std::vector<RenderPassData> m_RenderPassSubmitData[NUM_VULKAN_BUFFERS];

    RenderPassData& BeginRenderPass(RENDER_PASS WhichPass, uint32_t WhichBuffer, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores);
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
    std::unique_ptr<Drawable>   m_LightDrawable;
    std::unique_ptr<Drawable>   m_BlitDrawable;

    // Ray tracing
    VulkanRT                            m_vulkanRT;
    std::unique_ptr<SceneRTCullable>    m_sceneRayTracable;
    std::unique_ptr<SceneRTCulled>      m_sceneRayTracableCulled;

    // Textures
    std::map<std::string, TextureVulkan>    m_loadedTextures;
    TextureVulkan                           m_TexWhite;
    TextureVulkan                           m_DefaultNormal;
    // TextureVulkan                           m_TexBunnySplash;

    // Materials
    
    // Light Stuff
    glm::vec4                           m_LightColor;

    // **********************
    // The Objects
    // **********************
    ObjectVertUB                        m_ObjectVertUniformData;
    UniformT<ObjectVertUB>              m_ObjectVertUniform[NUM_RENDER_PASSES][NUM_VULKAN_BUFFERS];

    ObjectFragUB                        m_ObjectFragUniformData;
    UniformT<ObjectFragUB>              m_SphereFragUniform[NUM_RENDER_PASSES][NUM_VULKAN_BUFFERS];
    UniformT<ObjectFragUB>              m_FloorFragUniform[NUM_RENDER_PASSES][NUM_VULKAN_BUFFERS];

    Wrap_VkCommandBuffer                m_ObjectCmdBuffer[NUM_VULKAN_BUFFERS][NUM_RENDER_PASSES];

    // **********************
    // The Skybox
    // **********************
    float                               m_SkyboxScale;
    UniformT<SkyboxVertUB>              m_SkyboxVertUniform[NUM_RENDER_PASSES][NUM_VULKAN_BUFFERS];
    SkyboxVertUB                        m_SkyboxVertUniformData;

    // **********************
    // Deferred Lighting Fullscreen pass
    // **********************
    Wrap_VkCommandBuffer                m_LightCmdBuffer[NUM_VULKAN_BUFFERS];
    LightFragCtrl                       m_LightFragUniformData;
    UniformT<LightFragCtrl>             m_LightFragUniform[NUM_VULKAN_BUFFERS];

    // **********************
    // Post/Blit
    // **********************
    BlitFragCtrl                        m_BlitFragUniformData;
    UniformT<BlitFragCtrl>              m_BlitFragUniform[NUM_VULKAN_BUFFERS];
    Wrap_VkCommandBuffer                m_BlitCmdBuffer[NUM_VULKAN_BUFFERS];

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
    std::array<VkSemaphore, NUM_RENDER_PASSES>  m_PassCompleteSemaphore;

    // Don't actually need a render pass for each Vulkan Buffer, just one per pass
    // These are NOT the same vkrenderpass's as in the CRenderTargetArray's but should be compatible with them (are allowed dfferent load/clear parameters on the atachments)
    std::array<VkRenderPass, NUM_RENDER_PASSES> m_RenderPass;

    // Render targets for each pass
    CRenderTargetArray<1>       m_GBufferRT;
    CRenderTargetArray<1>       m_MainRT;
    CRenderTargetArray<1>       m_HudRT;

    // Scene information
    std::vector<SceneData>          m_SceneDataArray;
    std::vector<SceneDataInternal>  m_SceneDataInternalArray;
    UniformVulkan                   m_SceneDataUniform;

    // Texture array passed to shader
    // std::vector<VkDescriptorImageInfo>  m_SceneTextures;
    std::vector<const Texture*>         m_SceneTextures;

    // Mesh objects passed to the shader
    std::vector <RayObject>             m_RayMeshArray;
    std::vector <BufferVulkan *>        m_RayMeshBuffers;
    std::vector <VkBuffer>              m_RayMeshVkBuffers;

    std::vector <RayObjectIndices>      m_RayIndicesArray;
    std::vector <BufferVulkan *>        m_RayIndiceBuffers;
    std::vector <VkBuffer>              m_RayIndiceVkBuffers;

    // **********************
    // Misc.
    // **********************

    // Change over time parameters
    uint32_t                    m_LastUpdateTime;
    bool                        m_LightChangePaused;

    uint32_t                    m_LightDirUpdateTime;
    bool                        m_LightDirReverse;

    uint32_t                    m_LightColorUpdateTime;
    bool                        m_LightColorReverse;

    // Our local camera
    CSphereCamera               m_SphereCamera;

};
