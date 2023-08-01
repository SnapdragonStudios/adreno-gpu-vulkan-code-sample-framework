//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "rayQueryShadows.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "gui/imguiVulkan.hpp"
#include "main/applicationEntrypoint.hpp"
#include "material/computable.hpp"
#include "material/drawable.hpp"
#include "material/material.hpp"
#include "material/materialManager.hpp"
#include "material/shader.hpp"
#include "material/shaderDescription.hpp"
#include "material/shaderManager.hpp"
#include "memory/memoryManager.hpp"
#include "memory/vulkan/bufferObject.hpp"
#include "memory/vulkan/drawIndirectBufferObject.hpp"
#include "memory/vulkan/indexBufferObject.hpp"
#include "memory/vulkan/vertexBufferObject.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/instanceGenerator.hpp"
#include "system/math_common.hpp"
#include "texture/textureManager.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/extensionHelpers.hpp"
#include "vulkan/TextureFuncts.h"
#include "vulkan/timerSimple.hpp"
#include "vulkanRT/vulkanRT.hpp"
#include "vulkanRT/sceneRT.hpp"
#include "vulkanRT/meshUpdateRT.hpp"
#include "imgui.h"

#include <cassert>

#include "vulkanRT/meshObjectRT.hpp"


// Global Variables From Config File
VAR(bool,       gRenderHud, true, kVariableNonpersistent);
VAR(bool,       gRenderShadow, true, kVariableNonpersistent);
VAR(bool,       gAdvancedMode, true, kVariableNonpersistent);                     // Show full gui and enable stat output

VAR(glm::vec3, gCameraStartPos, glm::vec3(-0.0f, 15.0f, 40.0f), kVariableNonpersistent);
VAR(glm::vec3, gCameraStartRot, glm::vec3(0.0f, 0.0f, 0.0f), kVariableNonpersistent);    // in degrees

// VAR(glm::vec3, gShadowPosition, glm::vec3(0.f, 40.0f, 60.0f), kVariableNonpersistent); // Position of 'main' ray queried shadow
// VAR(glm::vec3, gShadowPosition, glm::vec3(0.35f, 0.0f, 70.0f), kVariableNonpersistent); // Position of 'main' ray queried shadow
VAR(glm::vec3, gShadowPosition, glm::vec3(22.35f, 0.0f, 70.0f), kVariableNonpersistent); // Position of 'main' ray queried shadow

VAR(float,     gShadowRadius, 300.0f, kVariableNonpersistent);                  // Calculated Radius of the shadow(debug)
VAR(float,     gShadowRadiusCutoff, 0.5f, kVariableNonpersistent);              // Amount to multiply shadow radius by when generating the acceleration structure containing possible shadow casters (reduces the complexity of the scene we are tracing through)
VAR(bool,      gShadowDirectionalLight, false, kVariableNonpersistent);         // Use a directional light when running ray query (uses same light direction as the gRasterizedShadow option)
VAR(float,     gLightEmitterRadius, 300.0f, kVariableNonpersistent);            // Size of the light emitter
VAR(float,     gLightAttenuationCutoff, 0.3f, kVariableNonpersistent);          // Minimum Light intensity (used to calculate light/shadow radius)
VAR( bool,     gCreateCulledAccelerationStructure, true, kVariableNonpersistent ); // Enable/disable creation of the acceleration structure that is culled (can only trace the entire scene when off)
VAR(bool,      gDisableAccelerationStructureCull, false, kVariableNonpersistent );  // Enable/disable culling based on radius (just trace against entire scene when off)
VAR(bool,      gForceAccelerationStructureRegen, false, kVariableNonpersistent );   // Regenerate the top level acceleration structure(s) every frame when enabled (do only on chenges if disabled)
VAR(bool,      gUpdateAccelerationStructureInPlace, true, kVariableNonpersistent);  // For updated Acceleration Structures determine if they should be updated in-place (single AS) or ping-pong between two AS
VAR(bool,      gAdditionalShadows, true, kVariableNonpersistent);              // Enable lights/shadows run in additional fragment shader passes (renders a sphere for each 'additional' light and raytraces shadows for all gbuffer pixels touched by the light)
VAR(bool,      gRayQueryFragmentShader, false, kVariableNonpersistent);         // Run the main light ray query on fragment shader
VAR(bool,      gAnimateBLAS, true, kVariableNonpersistent);                     // Run compute animation that updates some of the bottom level acceleration structures every frame
VAR(bool,      gBatchUpdateBLAS, false, kVariableNonpersistent);                // Batch the updating of the BLAS into a single cmd (rather than cmd per updated object).  Needs gAnimateBLAS=true

VAR(float,     gFOV, PI_DIV_4, kVariableNonpersistent);
VAR(float,     gNearPlane, 1.0f, kVariableNonpersistent);
VAR(float,     gFarPlane, 1050.0f, kVariableNonpersistent);

VAR(bool,      gRasterizedShadow, false, kVariableNonpersistent);                                   // Use rasterized (depth buffer) rather than ray queried shadows
VAR(bool,      gRasterizedShadowCulled, true, kVariableNonpersistent);                             // 
VAR(glm::vec3, gRasterizedShadowPosition, glm::vec3(300.f, 1047.f, 900.f), kVariableNonpersistent);
VAR(glm::vec3, gRasterizedShadowTarget, glm::vec3(200.f, 24.f, 330.f), kVariableNonpersistent);
VAR(float,     gRasterizedShadowFarPlane, 2500.f, kVariablePermanent);

VAR(float, gSpecularExponent, 50.0f, kVariableNonpersistent);
VAR(float, gSpecularScale, 1.2f, kVariableNonpersistent);
VAR(float, gIrradianceAmount, 0.2f, kVariableNonpersistent);
VAR(float, gIrradianceMip, 1.0f, kVariableNonpersistent);
VAR(float, gNormalAmount, 1.0f, kVariableNonpersistent);
VAR(float, gNormalMirrorReflectAmount, 0.05f, kVariableNonpersistent);

VAR(glm::vec4, gClearColor, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), kVariableNonpersistent);

VAR(char*, gSceneObjectName, "Media/Meshes/ThaiCarving.gltf", kVariableNonpersistent);
VAR(char*, gAnimatedMaterialName, "Foliage_Paris_Flowers_BaseColor", kVariableNonpersistent);

static const char* const sRenderPassNames[NUM_RENDER_PASSES] = { "RP_GBUFFER", "RP_RAYSHADOW", "RP_RASTERSHADOW", "RP_LIGHT", "RP_HUD", "RP_BLIT" };
static_assert(RP_GBUFFER == 0, "Check order of sRenderPassNames");
static_assert(RP_BLIT == 5, "Check order of sRenderPassNames");
static_assert(NUM_RENDER_PASSES == sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0]), "mismatched sRenderPassNames");

// Helper
class TestMeshAnimator final : public MeshUpdateRT
{
    TestMeshAnimator(const TestMeshAnimator&) = delete;
    TestMeshAnimator& operator=(const TestMeshAnimator&) = delete;
public:
    TestMeshAnimator(TestMeshAnimator&&) noexcept;
    TestMeshAnimator();

    bool Create(Vulkan& vulkan, const MeshObjectIntermediate& meshObject, const AccelerationStructureUpdateable* blas, bool createScratch);
    VkBuffer GetOriginalVertexVkBuffer() const { return m_OriginalVertices.GetVkBuffer(); }

    void UpdateAS(VulkanRT& vulkanRT, VkCommandBuffer cmdBuffer) const { MeshUpdateRT::UpdateAS(vulkanRT, cmdBuffer, *m_BLAS); }
    void PrepareUpdateASData(VkAccelerationStructureBuildGeometryInfoKHR& asBuildGeometryInfo, VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo) const { MeshUpdateRT::PrepareUpdateASData(asBuildGeometryInfo, asGeometry, asBuildRangeInfo, *m_BLAS); }

protected:
    BufferT<Vulkan>                          m_OriginalVertices;
    const AccelerationStructureUpdateable*  m_BLAS = nullptr;
};

TestMeshAnimator::TestMeshAnimator() : MeshUpdateRT()
{
}

TestMeshAnimator::TestMeshAnimator(TestMeshAnimator&& other) noexcept
    : MeshUpdateRT(std::move(other))
    , m_OriginalVertices(std::move(other.m_OriginalVertices))
    , m_BLAS(other.m_BLAS)
{
    other.m_BLAS = nullptr;
}

bool TestMeshAnimator::Create(Vulkan& vulkan, const MeshObjectIntermediate& meshObject, const AccelerationStructureUpdateable* blas, bool createScratch)
{
    if (!MeshUpdateRT::Create(vulkan, meshObject, createScratch ? blas->GetUpdateScratchSize() : 0))
        return false;

    m_BLAS = blas;
    m_OriginalVertices = MeshObjectRT::CreateRtVertexBuffer(vulkan.GetMemoryManager(), meshObject, BufferUsageFlags::Storage);
    return true;
}


//
// Implementation of the Application entrypoint (called by the framework)
// Construct the Application class
//
//-----------------------------------------------------------------------------
FrameworkApplicationBase* Application_ConstructApplication()
//-----------------------------------------------------------------------------
{
    return new Application();
}

//-----------------------------------------------------------------------------
Application::Application()
//-----------------------------------------------------------------------------
    : ApplicationHelperBase()
    , m_bEncodeSRGB(false)
    , m_TotalDrawCalls(0)
    , m_TotalTriangles(0)
    , m_vulkanRT(*GetVulkan())
{
    // Render passes
    m_RenderPass.fill(VK_NULL_HANDLE);
}

//-----------------------------------------------------------------------------
Application::~Application()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    // Semaphores
    if (m_BlitCompleteSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(pVulkan->m_VulkanDevice, m_BlitCompleteSemaphore, NULL);
    m_BlitCompleteSemaphore = VK_NULL_HANDLE;

    // Passes
    m_RenderPass[RP_RASTERSHADOW] = VK_NULL_HANDLE; // shadow pass owned by m_Shadows.
    for (auto& pass : m_RenderPass)
    {
        if (pass != VK_NULL_HANDLE)
            vkDestroyRenderPass(pVulkan->m_VulkanDevice, pass, nullptr);
        pass = VK_NULL_HANDLE;
    }

    // Compute

    // Textures
    ReleaseTexture(*pVulkan, &m_ShadowRayQueryComputeOutput);

    for (auto& uniform : m_AdditionalDeferredLightsSharedUniform)
        ReleaseUniformBuffer(pVulkan, uniform);

    for(auto& updatable : m_sceneAnimatedMeshObjects)
        updatable.Destroy(*pVulkan, m_vulkanRT);
}

//-----------------------------------------------------------------------------
int Application::PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat> formats)
//-----------------------------------------------------------------------------
{
    // On Snapdragon if the surfaceflinger has to do the rotation to the display native orientation then it will do it at 8bit colordepth.
    // To avoid this we need to enable the 'pre-rotation' of the display (and the use of VK_QCOM_render_pass_transform so we dont have to rotate our buffers/passes manually).
    GetVulkan()->m_UseRenderPassTransform = true;

    // We want to select a SRGB output format (if one exists) unless running on hlm (does not support srgb output)
    TextureFormat idealFormat = gRunOnHLM ? TextureFormat::B8G8R8A8_UNORM : TextureFormat::B8G8R8A8_SRGB;
    int index = 0;
    for (const auto& format : formats)
    {
        if (format.format == idealFormat)
            return index;
        ++index;
    }
    return -1;
}

//-----------------------------------------------------------------------------
void Application::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& appConfig)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::PreInitializeSetVulkanConfiguration( appConfig );
    m_vulkanRT.RegisterRequiredVulkanLayerExtensions(appConfig, true/*query only*/);
    appConfig.RequiredExtension<ExtensionHelper::Ext_VK_EXT_host_query_reset>();
    appConfig.RequiredExtension<ExtensionHelper::Ext_VK_KHR_8bit_storage>();
    appConfig.RequiredExtension<ExtensionHelper::Ext_VK_KHR_draw_indirect_count>();
}

//-----------------------------------------------------------------------------
bool Application::Initialize(uintptr_t windowHandle, uintptr_t instanceHandle)
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize(windowHandle, instanceHandle))
    {
        return false;
    }
    auto* pVulkan = GetVulkan();

    // Camera
    InitCamera();
    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    // The Skybox
    m_SkyboxScale = gFarPlane * 0.95f;

    // Set the current surface format
    m_RequestedSurfaceFormat = {pVulkan->m_SurfaceFormat, pVulkan->m_SurfaceColorSpace};
    m_bEncodeSRGB = !FormatIsSrgb(pVulkan->m_SurfaceFormat);   // if we have an srgb buffer then the output doesnt need to be encoded (hardware will do it for us)

    // Profiling
    static constexpr uint32_t cMaxTimersPerFrame = 16;
    m_GpuTimerPool = std::make_unique<TimerPoolSimple>(*pVulkan);
    m_GpuTimerPool->Initialize(cMaxTimersPerFrame);

    if (!LoadShaders())
        return false;

    if (!LoadTextures())
        return false;

    if (!CreateRenderTargets())
        return false;

    if (!InitShadowMap())
        return false;

    if (!InitLighting())
        return false;

    if (!InitUniforms())
        return false;

    if (!InitAllRenderPasses())
        return false;

    if (!InitMaterials())
        return false;

    if (!InitGui(windowHandle))
        return false;

    if (!LoadMeshObjects())
        return false;

    if (!VULKAN_RT_USE_STUB && !LoadRayTracingObjects())
        return false;

    if (!InitCommandBuffers())
        return false;

    if (!InitDrawables())
        return false;

    InitHdr();

    if (!InitLocalSemaphores())
        return false;

    if (!BuildCmdBuffers())
        return false;

    LOGE("Initalize workers!!");

    m_GameThreadWorker.Initialize("GameThreadWorker", 1);

    m_CompletedThreadOutput = {};

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();
    m_ShadowRayQueryComputeOutput = CreateTextureObject(*pVulkan, gRenderWidth, gRenderHeight, TextureFormat::R8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "ShadowRQDest");
    
    //
    // Create the 'Light' drawable mesh.
    // Fullscreen quad that does the gbuffer lighting pass (fullscreen pass of gbuffer, outputting a lit color buffer).
    LOGI("Creating Light mesh...");

    // Lambda to populate textures requested by the material.
    const auto textureLoader =
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo {
        if (texName == "Albedo") {
            return { &m_GBufferRT[0].m_ColorAttachments[0] };
        }
        else if (texName == "Normal") {
            return { &m_GBufferRT[0].m_ColorAttachments[1] };
        }
        else if (texName == "Depth") {
            return { &m_GBufferRT[0].m_DepthAttachment };
        }
        else if (texName == "ShadowDepth") {
            return { &m_Shadows[0].GetDepthTexture() };
        }
        else if (texName == "ShadowVSM") {
            return { m_ShadowVSM.GetVSMTexture() };
        }
        else if (texName == "ShadowRT") {
            if (gRayQueryFragmentShader)
                return { &m_ShadowRT[0].m_ColorAttachments[0] };
            else
                return { &m_ShadowRayQueryComputeOutput };
        }
        else if (texName == "Environment" || texName == "Irradiance")
        {
            auto texture = m_TextureManager->GetTexture( texName );
            if (texture)
                return { texture };
            // File not loaded, use default
            return { m_TexWhite };
        }
        assert(0);
        return {};
    };

    // Light pass using Ray Queried shadow texture
    {
        const auto* pLightShader = m_ShaderManager->GetShader("Light");
        assert(pLightShader);
        auto lightShaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pLightShader, NUM_VULKAN_BUFFERS, textureLoader,
            [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    //LightFragCtrl
                    return { m_LightFragUniform.vkBuffers };
        });

        Mesh<Vulkan> lightMesh;
        MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pLightShader->m_shaderDescription->m_vertexFormats, &lightMesh);

        m_LightDrawable = std::make_unique<Drawable>(*pVulkan, std::move(lightShaderMaterial));
        if (!m_LightDrawable->Init(m_RenderPass[RP_LIGHT], sRenderPassNames[RP_LIGHT], std::move(lightMesh)))
        {
            LOGE("Error Creating Light drawable...");
        }
    }

    // Light pass using 'traditional' rasterized shadow
    {
        const auto* pLightShader = m_ShaderManager->GetShader("LightRasterizedShadows");
        assert(pLightShader);
        auto lightShaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pLightShader, NUM_VULKAN_BUFFERS, textureLoader,
            [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    //LightFragCtrl
                    return { m_LightFragUniform.vkBuffers };
        });

        MeshObject lightMesh;
        MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pLightShader->m_shaderDescription->m_vertexFormats, &lightMesh);

        m_LightRasterizedShadowDrawable = std::make_unique<Drawable>(*pVulkan, std::move(lightShaderMaterial));
        if (!m_LightRasterizedShadowDrawable->Init(m_RenderPass[RP_LIGHT], sRenderPassNames[RP_LIGHT], std::move(lightMesh)))
        {
            LOGE("Error Creating Light drawable...");
        }
    }

    //
    // Create the 'Blit' drawable mesh.
    // Fullscreen quad that does the final composite (hud and scene) for output.
    LOGI("Creating Blit mesh...");

    const auto* pBlitShader = m_ShaderManager->GetShader("Blit");
    assert(pBlitShader);
    auto blitShaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pBlitShader, NUM_VULKAN_BUFFERS,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo {
            if (texName == "Diffuse") {
                return { &m_MainRT[0].m_ColorAttachments[0] };
            }
            else if (texName == "Overlay") {
                return { &m_HudRT[0].m_ColorAttachments[0] };
            }
            else if (texName == "Bloom") {
                return { m_TexWhite };
            }
            assert(0);
            return {};
        },
        [this](const std::string& bufferName) -> tPerFrameVkBuffer {
            //BlitFragCB
            return { m_BlitFragUniform.buf.GetVkBuffer() };
        }
        );

    MeshObject blitMesh;
    MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pBlitShader->m_shaderDescription->m_vertexFormats, &blitMesh);

    m_BlitDrawable = std::make_unique<Drawable>(*pVulkan, std::move(blitShaderMaterial));
    if (!m_BlitDrawable->Init( m_RenderPass[RP_BLIT], sRenderPassNames[RP_BLIT], std::move(blitMesh)))
    {
        LOGE("Error Creating Blit drawable...");
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadRayTracingObjects()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // Load the Geometry for Ray Tracing! and populate m_sceneRayTracable
    if (!m_vulkanRT.Init())
    {
        LOGE("Vulkan Ray Tracing Extension was not available...exiting");
        return false;
    }

    std::vector<MeshObjectIntermediate> candidateMeshes = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gSceneObjectName);
    if( candidateMeshes.empty() )
    {
        LOGE("Could not load the Ray Tracing mesh object: %s", gSceneObjectName);
        return false;
    }

    // (optionally) go through and find meshes that match each other (automated instancing).
    auto instancedMeshes = MeshInstanceGenerator::FindInstances( std::move(candidateMeshes) );
    candidateMeshes.clear();

    auto sceneRayTracable = std::make_unique<SceneRTCullable>(*pVulkan, m_vulkanRT);

    m_sceneObjectTriangleCounts.clear();

    for (const auto& instancedMesh : instancedMeshes)
    {
        if( !instancedMesh.mesh.m_Materials.empty() &&
            (instancedMesh.mesh.m_Materials[0].transparent || !instancedMesh.mesh.m_Materials[0].emissiveFilename.empty()) )
            continue;

        // Find animated meshes (flowers!)
        const bool updateable = instancedMesh.mesh.m_Materials[0].diffuseFilename.find(gAnimatedMaterialName) != std::string::npos;
        auto updateMode = MeshObjectRT::UpdateMode::NotUpdatable;
        if (updateable && gUpdateAccelerationStructureInPlace)
            updateMode = MeshObjectRT::UpdateMode::InPlace;
        else if (updateable && !gUpdateAccelerationStructureInPlace)
            updateMode = MeshObjectRT::UpdateMode::PingPong;

        MeshObjectRT meshRayTracable;
        if (!meshRayTracable.Create(*pVulkan, m_vulkanRT, instancedMesh.mesh, updateMode))
            return false;
        const auto& id = sceneRayTracable->AddObject(std::move(meshRayTracable));
        if (!id)
            return false;
        for (const auto& instance : instancedMesh.instances)
        {
            sceneRayTracable->AddInstance(id, instance.transform);
        }
        m_sceneObjectTriangleCounts.emplace( id, instancedMesh.mesh.CalcNumTriangles() );

        // Create the object that does the animation/update
        if (updateable)
        {
            const MeshObjectRT* pMeshObjectRT = sceneRayTracable->FindObject(id);
            assert(pMeshObjectRT);

            auto& updatableObject = m_sceneAnimatedMeshObjects.emplace_back();
            if (!updatableObject.Create(*pVulkan, instancedMesh.mesh, pMeshObjectRT, true/*always create a scratch per updateable, may not be used if we are batching updates together*/))
            {
                assert(0);
                m_sceneAnimatedMeshObjects.pop_back();
            }
        }
    }

    const auto blasUpdateMode = gUpdateAccelerationStructureInPlace ? MeshObjectRT::UpdateMode::InPlace : MeshObjectRT::UpdateMode::PingPong;

    if (gCreateCulledAccelerationStructure)
    {
        // Create the acceleration structure for Ray Tracing (this top level acceleration structure is updated depending on cpu culling).
        auto sceneRayTracableCulled = std::make_unique<SceneRTCulled>( *pVulkan, m_vulkanRT );
        if (!sceneRayTracableCulled->CreateAccelerationStructure( blasUpdateMode, sceneRayTracable->GetNumKnownInstances() ))
        {
            LOGE( "Error Creating Scene AccelerationStructure..." );
            return false;
        }
        m_sceneRayTracableCulled = std::move( sceneRayTracableCulled );
    }
    else
    {
        // Create the top level acceleration structure for just ray tracing the entire scene (no cpu culling)
        sceneRayTracable->CreateAccelerationStructure();
    }

    m_sceneRayTracable = std::move(sceneRayTracable);

    // Create the Computable that runs the ShadowRQ shader (which does the ray queried shadows on the scene acceleration structure).
    if (true)// ShadowRQ
    {
        const auto* pComputeShader = m_ShaderManager->GetShader("ShadowRQ");
        assert(pComputeShader);

        for (auto directionalLight : { false, true })   // make a computable for both light types
        {
            auto material = m_MaterialManager->CreateMaterial(*pVulkan, *pComputeShader, NUM_VULKAN_BUFFERS,
                [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
                        if (texName == "Output")
                            return { &m_ShadowRayQueryComputeOutput };
                        else if (texName == "Depth")
                            return { &m_GBufferRT[0].m_DepthAttachment };
                        else if (texName == "Normal")
                            return { &m_GBufferRT[0].m_ColorAttachments[1] };
                        else {
                            assert(0);
                            return {};
                        }
                },
                [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    return { m_ShadowRQCtrlUniform.vkBuffers };
                },
                nullptr,
                [this](const std::string& accelStructureName) -> MaterialPass::tPerFrameVkAccelerationStructure {
                    return { m_sceneRayTracableCulled ? m_sceneRayTracableCulled->GetVkAccelerationStructure() : m_sceneRayTracable->GetVkAccelerationStructure() };
                },
                [directionalLight](const std::string& specializationConstantName) -> const VertexElementData {
                    return VertexElementData{ VertexFormat::Element::ElementType::t::Boolean, directionalLight };
                });

            auto computable = std::make_unique<Computable>(*pVulkan, std::move(material));
            if (!computable->Init())
            {
                LOGE("Error Creating ShadowRQ computable...");
                computable.reset();
            }
            else
            {
                computable->SetDispatchGroupCount(0, { (m_ShadowRayQueryComputeOutput.Width + 31) / 32, (m_ShadowRayQueryComputeOutput.Height + 31) / 32,1 });
            }
            if (directionalLight)
                m_ShadowDirectionalLightRQComputable = std::move(computable);
            else
                m_ShadowPointLightRQComputable = std::move(computable);
        }
    }

    // Create the fullscreen drawable for ShadowRQFrag shader (which does the ray queried shadows on the scene acceleration structure).
    if (true)
    {
        const auto* pShader = m_ShaderManager->GetShader("ShadowRQFrag");
        assert(pShader);

        for (auto directionalLight : { false, true })   // make a drawable for both light types
        {
            auto material = m_MaterialManager->CreateMaterial(*pVulkan, *pShader, NUM_VULKAN_BUFFERS,
            [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
                if (texName == "Output")
                    return { &m_ShadowRayQueryComputeOutput };
                else if (texName == "Depth")
                    return { &m_GBufferRT[0].m_DepthAttachment };
                else if (texName == "Normal")
                    return { &m_GBufferRT[0].m_ColorAttachments[1] };
                else {
                    assert(0);
                    return {};
                }
            },
            [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                return { m_ShadowRQCtrlUniform.vkBuffers };
            },
            nullptr,
            [this](const std::string& accelStructureName) -> MaterialPass::tPerFrameVkAccelerationStructure {
                return { m_sceneRayTracableCulled ? m_sceneRayTracableCulled->GetVkAccelerationStructure() : m_sceneRayTracable->GetVkAccelerationStructure() };
            },
            [this, directionalLight](const std::string& specializationConstantName) -> const VertexElementData {
                return VertexElementData{ VertexFormat::Element::ElementType::t::Boolean, directionalLight };
            });

            MeshObject fullscreenMesh;
            MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pShader->m_shaderDescription->m_vertexFormats, &fullscreenMesh);

            auto drawable = std::make_unique<Drawable>(*pVulkan, std::move(material));
            if (!drawable->Init(m_RenderPass[RP_RAYSHADOW], sRenderPassNames[RP_RAYSHADOW], std::move(fullscreenMesh)))
            {
                LOGE("Error Creating Shadow Ray Query drawable...");
            }
            else
            {
                if (directionalLight)
                    m_ShadowDirectionalLightRQDrawable = std::move(drawable);
                else
                    m_ShadowPointLightRQDrawable = std::move(drawable);
            }
        }
    }

    //
    // Setup the 'additional' lights (with Ray Traced shadows)
    //
    const size_t NumAdditionalLights = m_AdditionalDeferredLightsData.size();

    const auto* pPointLightShader = m_ShaderManager->GetShader("PointLight");
    assert(pPointLightShader);

    m_AdditionalDeferredLightsSharedUniform.resize(NumAdditionalLights);
    for( auto& uniformArray : m_AdditionalDeferredLightsSharedUniform )
    {
        PointLightUB uniformData{};
        if (!CreateUniformBuffer(pVulkan, uniformArray, &uniformData))
        {
            LOGE("Error Creating AdditionalDeferredLightsSharedUniform buffer...");
            return false;
        }
    }

    m_AdditionalDeferredLightDrawables.reserve(NumAdditionalLights);
    m_AdditionalDeferredLightShadowRQ.reserve(NumAdditionalLights);

    for( int WhichLight=0; WhichLight<NumAdditionalLights; ++WhichLight )
    {
        auto& lightShadowRQ = m_AdditionalDeferredLightShadowRQ.emplace_back( *pVulkan, m_vulkanRT );
        if (!lightShadowRQ.CreateAccelerationStructure( blasUpdateMode, m_sceneRayTracable->GetNumKnownInstances() ))
        {
            LOGE( "Error Creating light shadow AccelerationStructure..." );
            return false;
        }

        auto deferredLightMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pPointLightShader, NUM_VULKAN_BUFFERS,
            [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
                if (texName == "Albedo") {
                    return { &m_GBufferRT[0].m_ColorAttachments[0] };
                }
                else if (texName == "Normal") {
                    return { &m_GBufferRT[0].m_ColorAttachments[1] };
                }
                else if (texName == "Depth") {
                    return { &m_GBufferRT[0].m_DepthAttachment };
                }
                else {
                    assert(0);
                    return {};
                }
            },
            [this, WhichLight](const std::string& bufferName) -> tPerFrameVkBuffer {
                return { m_AdditionalDeferredLightsSharedUniform[WhichLight].vkBuffers };
            },
            nullptr,
            [this, WhichLight](const std::string& accelStructureName) -> MaterialPass::tPerFrameVkAccelerationStructure {
                return { m_AdditionalDeferredLightShadowRQ[WhichLight].GetVkAccelerationStructure() };
            }
        );
        auto& drawable = m_AdditionalDeferredLightDrawables.emplace_back( *pVulkan, std::move(deferredLightMaterial) );

        const auto sphereMeshObjectIntermediate = MeshObjectIntermediate::LoadGLTF( *m_AssetManager, "./Media/Meshes/Skybox_Separate.gltf" );
        if( sphereMeshObjectIntermediate.empty() )
        {
            LOGE( "Error loading light sphere..." );
            return false;
        }

        MeshObject sphereMesh;
        if (!MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), sphereMeshObjectIntermediate[0], 0, pPointLightShader->m_shaderDescription->m_vertexFormats, &sphereMesh))
        {
            LOGE( "Error creating light sphere..." );
            return false;
        }

        if( !drawable.Init( m_RenderPass[RP_LIGHT], sRenderPassNames[RP_LIGHT], std::move( sphereMesh ) ) )
        {
            LOGE( "Error Initializing AdditionalDeferredLightDrawables..." );
            return false;
        }
    }

    // Create the computables that do the acceleration structure vertex updates (animated vertices).
    {
        const auto* pAnimateShader = m_ShaderManager->GetShader("AnimateBuffer");
        assert(pAnimateShader);

        m_sceneAnimatedMeshComputables.reserve(m_sceneAnimatedMeshObjects.size());

        for (const auto& updatableObject : m_sceneAnimatedMeshObjects)
        {
            auto material = m_MaterialManager->CreateMaterial(*pVulkan, *pAnimateShader, 1,
                nullptr,
                [&updatableObject, this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    if (bufferName == "InputVertexData")
                        return { updatableObject.GetOriginalVertexVkBuffer() };
                    else if (bufferName == "OutputVertexData")
                        return { updatableObject.GetVertexVkBuffer() };
                    else if (bufferName == "Uniform")
                        return { m_sceneAnimatedMeshUniform.vkBuffers };
                    assert(0);
                    return {};
                },
                nullptr
                    );

            auto& computable = m_sceneAnimatedMeshComputables.emplace_back(*pVulkan, std::move(material));
            if (!computable.Init())
            {
                LOGE("Error Creating Scene Update computable...");
                m_sceneAnimatedMeshComputables.pop_back();
            }
            else
            {
                computable.SetDispatchThreadCount(0, { updatableObject.GetNumVertices(), 1, 1 });
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    // Threads
    m_GameThreadWorker.FinishAllWork();
    m_GameThreadWorker.Terminate();

    pVulkan->WaitUntilIdle();

    // Command buffers
    for (auto& bufferArray : m_PassCmdBuffer)
        for (auto& buffer : bufferArray)
            buffer.Release();

    // Meshes
    m_SkyboxDrawable.reset();
    m_ShadowRasterizedDrawable.reset();
    m_ShadowPointLightRQDrawable.reset();
    m_ShadowDirectionalLightRQDrawable.reset();
    m_LightDrawable.reset();
    m_LightRasterizedShadowDrawable.reset();
    m_BlitDrawable.reset();
    m_SceneDrawables.clear();
    m_ShadowPointLightRQComputable.reset();
    m_ShadowDirectionalLightRQComputable.reset();
    m_ShadowRasterizedCullingComputable.reset();
    m_sceneRayTracableCulled.reset();
    m_sceneRayTracable.reset();
    m_AdditionalDeferredLightDrawables.clear();
    m_AdditionalDeferredLightShadowRQ.clear();
    m_ShadowVSM.Release(*pVulkan);

    // Scene

    // Uniform Buffers
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        ReleaseUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass]);
        ReleaseUniformBuffer(pVulkan, m_ObjectFragUniform[WhichPass]);
        ReleaseUniformBuffer(pVulkan, m_SkyboxVertUniform[WhichPass]);
    }   // WhichPass

    ReleaseUniformBuffer(pVulkan, m_ShadowRQCtrlUniform);
    ReleaseUniformBuffer(pVulkan, &m_BlitFragUniform);
    ReleaseUniformBuffer(pVulkan, m_LightFragUniform);
    ReleaseUniformBuffer(pVulkan, m_ShadowInstanceCullingCameraUniform);
    ReleaseUniformBuffer(pVulkan, &m_ShadowInstanceCullingBuffer);
    m_ShadowInstanceCulledData.Destroy();

    for (auto& uniform : m_AdditionalDeferredLightsSharedUniform)
        ReleaseUniformBuffer(pVulkan, uniform);
    m_AdditionalDeferredLightsSharedUniform.clear();

    ReleaseUniformBuffer(pVulkan, m_sceneAnimatedMeshUniform);

    if (m_GpuTimerPool)
        m_GpuTimerPool->Destroy();
    m_GpuTimerPool.reset();

    // Finally call into base class destroy
    ApplicationHelperBase::Destroy();
}

//-----------------------------------------------------------------------------
void Application::RunGameThread(const GameThreadInputParam& rThreadParam, GameThreadOutputParam& rThreadOutput)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = GetVulkan();
    uint32_t WhichFrame = rThreadParam.CurrentBuffer.idx;

    // Clean up command buffers from last run
    m_RenderPassSubmitData[WhichFrame].clear();

    // take the gpu timer mutex (as we read back the timers and will also be modifying cpu timer data in this thread, and dont want to conflict with another thread doing logging)
    std::lock_guard<decltype(m_GpuTimerPoolMutex)> timerPoolLock(m_GpuTimerPoolMutex);

    // Read back timers
    UpdateProfiling(WhichFrame);

    // If anything changes with the lights (Position, orientation, color, brightness, etc.)
    UpdateLighting(rThreadParam.ElapsedTime);

    UpdateShadowMap(rThreadParam.ElapsedTime);

    // Update uniform buffers with latest data
    UpdateUniforms(WhichFrame, rThreadParam.ElapsedTime);

    // Set where we are reading the shadow from.
    if (gRayQueryFragmentShader)
        m_LightDrawable->GetMaterial().UpdateDescriptorSetBinding(WhichFrame, "Shadow", m_ShadowRT[0].m_ColorAttachments[0] );
    else
        m_LightDrawable->GetMaterial().UpdateDescriptorSetBinding(WhichFrame, "Shadow", m_ShadowRayQueryComputeOutput );

    m_LightCmdBuffer[WhichFrame].Reset();
    m_LightCmdBuffer[WhichFrame].Begin( m_MainRT[0].m_FrameBuffer, m_MainRT.m_RenderPass );

    VkRect2D Scissor = {};
    Scissor.extent.width = m_PassSetup[RP_LIGHT].TargetWidth;
    Scissor.extent.height = m_PassSetup[RP_LIGHT].TargetHeight;
    VkViewport Viewport = {
        .width = (float)Scissor.extent.width,
        .height = (float)Scissor.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport( m_LightCmdBuffer[WhichFrame].m_VkCommandBuffer, 0, 1, &Viewport );
    vkCmdSetScissor( m_LightCmdBuffer[WhichFrame].m_VkCommandBuffer, 0, 1, &Scissor );
    if (gRasterizedShadow)
        AddDrawableToCmdBuffers(*m_LightRasterizedShadowDrawable.get(), &m_LightCmdBuffer[WhichFrame], 1, 1, WhichFrame);
    else
        AddDrawableToCmdBuffers( *m_LightDrawable.get(), &m_LightCmdBuffer[WhichFrame], 1, 1, WhichFrame );
    m_LightCmdBuffer[WhichFrame].End();

    // First pass, waits for the back buffer to be ready.
    std::span<const VkSemaphore> pBackbufferWaitSemaphores = { &rThreadParam.CurrentBuffer.semaphore, 1 };
    // Blit pass, signals that the last submit is done
    std::span<const VkSemaphore> pBlitCompleteSemaphore = { &m_BlitCompleteSemaphore, 1 };

    static const VkPipelineStageFlags DefaultGfxWaitDstStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    int gpuFrameTimerId;
    {
        // GBuffer render pass (gbuffer 'laydown' and shadow generation).
        auto& renderPassLocal = BeginCommandBuffer( RP_GBUFFER, WhichFrame, pBackbufferWaitSemaphores, DefaultGfxWaitDstStageMasks, { }, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        gpuFrameTimerId = renderPassLocal.CmdBuff.StartGpuTimer("GPU Frame");
        BeginRenderPass( renderPassLocal );
        AddPassCommandBuffers( renderPassLocal );
        EndRenderPass( renderPassLocal );
        renderPassLocal.CmdBuff.End();
    }

    if (gRenderShadow)
    {
        if (gRasterizedShadow)
        {
            // Shadow 'depth laydown' pass
            auto& renderPassLocal = BeginCommandBuffer(RP_RASTERSHADOW, WhichFrame, {}, {}, {}, VK_SUBPASS_CONTENTS_INLINE);

            if (gRasterizedShadowCulled)
            {
                // If we are doing rasterized shadow culling then we need to add that computable!
                AddComputableToCmdBuffer(*m_ShadowRasterizedCullingComputable, renderPassLocal.CmdBuff);

                // Barrier to ensure compute finished before rasterizing the shadows.
                VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
                vkCmdPipelineBarrier(renderPassLocal.CmdBuff.m_VkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            }

            BeginRenderPass(renderPassLocal);
            gpuFrameTimerId = renderPassLocal.CmdBuff.StartGpuTimer("Rasterized Shadows");
            AddPassCommandBuffers(renderPassLocal);
            renderPassLocal.CmdBuff.StopGpuTimer(gpuFrameTimerId);
            EndRenderPass(renderPassLocal);

            // Execute shadow sampling (VSM) compute shaders
            m_ShadowVSM.AddComputableToCmdBuffer(renderPassLocal.CmdBuff);

            renderPassLocal.CmdBuff.End();
        }
        else
        {
            auto& renderPassLocal = BeginCommandBuffer(RP_RAYSHADOW, WhichFrame, {}, {}, {}, VK_SUBPASS_CONTENTS_INLINE);

            VkCommandBuffer vkCommandBuffer = renderPassLocal.CmdBuff.m_VkCommandBuffer;

            // Run any animation Computables
            bool forceASUpdate = false;
            if (gAnimateBLAS && !m_sceneAnimatedMeshComputables.empty())
            {
                for (const auto& computable : m_sceneAnimatedMeshComputables)
                {
                    AddComputableToCmdBuffer(computable, renderPassLocal.CmdBuff);
                }

                // Barrier to ensure compute has written all the buffers before building BLAS.
                VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

                // If anything is animated in the overall scene force an update of the culled Top Level AS.  Potentially this is overkill (does not take into account which objects are culled).
                forceASUpdate = true;
            }

            auto asUpdateTimerId = renderPassLocal.CmdBuff.StartGpuTimer("Acceleration Structure Update (total)");

            // Build/update all the updateable bottom level acceleration structures
            if (forceASUpdate && !m_sceneAnimatedMeshObjects.empty())
            {
                char timerName[64];
                sprintf(timerName, "Animated BLAS Updates (%d updates)", (int)m_sceneAnimatedMeshObjects.size());
                auto blasUpdateTimerId = renderPassLocal.CmdBuff.StartGpuTimer(timerName);

                if (gBatchUpdateBLAS)
                {
                    // Batch together the BLAS updates into one Vulkan command
                    std::vector< VkAccelerationStructureBuildGeometryInfoKHR> blasUpdateBuildGeometryInfos;
                    std::vector< VkAccelerationStructureGeometryKHR> blasUpdateGeometries;
                    std::vector< VkAccelerationStructureBuildRangeInfoKHR> blasUpdateBuildRangeInfos;
                    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBlasUpdateBuildRangeInfos;

                    const uint32_t numAnimatedBlas = (uint32_t)m_sceneAnimatedMeshObjects.size();
                    blasUpdateBuildGeometryInfos.reserve(numAnimatedBlas);
                    blasUpdateGeometries.reserve(numAnimatedBlas);
                    blasUpdateBuildRangeInfos.reserve(numAnimatedBlas);
                    pBlasUpdateBuildRangeInfos.reserve(numAnimatedBlas);

                    for (const auto& updateableBottomLevelObjectIt : m_sceneAnimatedMeshObjects)
                    {
                        updateableBottomLevelObjectIt.PrepareUpdateASData(blasUpdateBuildGeometryInfos.emplace_back(), blasUpdateGeometries.emplace_back(), blasUpdateBuildRangeInfos.emplace_back());
                        pBlasUpdateBuildRangeInfos.push_back(&blasUpdateBuildRangeInfos.back());
                    }
                    m_vulkanRT.vkCmdBuildAccelerationStructuresKHR(vkCommandBuffer, numAnimatedBlas, blasUpdateBuildGeometryInfos.data(), pBlasUpdateBuildRangeInfos.data());
                }
                else
                {
                    // Do each BLAS update as a seperate command
                    for (const auto& updateableBottomLevelObjectIt : m_sceneAnimatedMeshObjects)
                    {
                        updateableBottomLevelObjectIt.UpdateAS(m_vulkanRT, vkCommandBuffer);
                    }
                }

                renderPassLocal.CmdBuff.StopGpuTimer(blasUpdateTimerId);

                // Barrier to ensure BLAS are all updated before updating or building TLAS.
                VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            }

            // Build the shadow acceleration structures (if requested to).
            auto tlasUpdateTimerId = renderPassLocal.CmdBuff.StartGpuTimer("Main TLAS Update");
            if (m_sceneRayTracableCulled)
                m_sceneRayTracableCulled->UpdateAS(vkCommandBuffer, forceASUpdate);
            renderPassLocal.CmdBuff.StopGpuTimer(tlasUpdateTimerId);

            // Build the acceleration structures (if requested to) for the 'additional' lights.
            if (gAdditionalShadows)
            {
                for (uint32_t WhichLight = 0; WhichLight < m_AdditionalDeferredLightShadowRQ.size(); ++WhichLight)
                {
                    char timerName[64];
                    sprintf(timerName, "AdditionalLight %u TLAS Update", WhichLight + 1);
                    auto tlasUpdateTimerId = renderPassLocal.CmdBuff.StartGpuTimer(timerName);
                    auto& sceneRT = m_AdditionalDeferredLightShadowRQ[WhichLight];
                    sceneRT.UpdateAS(renderPassLocal.CmdBuff.m_VkCommandBuffer);
                    renderPassLocal.CmdBuff.StopGpuTimer(tlasUpdateTimerId);
                }
            }

            // Finish up the timer around all the Acceleration Structure updates
            renderPassLocal.CmdBuff.StopGpuTimer(asUpdateTimerId);

            if (gRayQueryFragmentShader)
            {
                auto timerId = renderPassLocal.CmdBuff.StartGpuTimer("ShadowRayQuery Frag");
                BeginRenderPass(renderPassLocal);
                if (gShadowDirectionalLight)
                    AddDrawableToCmdBuffers(*m_ShadowDirectionalLightRQDrawable, &renderPassLocal.CmdBuff, 1, 1, WhichFrame);
                else
                    AddDrawableToCmdBuffers(*m_ShadowPointLightRQDrawable, &renderPassLocal.CmdBuff, 1, 1, WhichFrame);
                EndRenderPass(renderPassLocal);
                renderPassLocal.CmdBuff.StopGpuTimer(timerId);
            }
            else
            {
                // Execue the Shadow Ray Tracing commands on the main (graphics) queue.
                auto timerId = renderPassLocal.CmdBuff.StartGpuTimer("ShadowRayQuery Compute");
                if (gShadowDirectionalLight)
                    AddComputableToCmdBuffer(*m_ShadowDirectionalLightRQComputable, &renderPassLocal.CmdBuff, 1, 1, m_GpuTimerPool.get());
                else
                    AddComputableToCmdBuffer(*m_ShadowPointLightRQComputable, &renderPassLocal.CmdBuff, 1, 1, m_GpuTimerPool.get());
                renderPassLocal.CmdBuff.StopGpuTimer(timerId);
            }
            renderPassLocal.CmdBuff.End();
        }
    }

    {
        // Lighting pass (Gbuffer resolve and emissives)
        auto& renderPassLocal = BeginRenderPass(RP_LIGHT, WhichFrame, {}, {}, {}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        // Add all the lights and anything else rendered to the light render pass (eg emissives)
        AddPassCommandBuffers( renderPassLocal );
        EndRenderPass( renderPassLocal );
        renderPassLocal.CmdBuff.End();
    }

    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (gRenderHud && m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = GetGui()->Render(WhichFrame, m_HudRT[0].m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            auto& renderPassLocal = BeginRenderPass(RP_HUD, WhichFrame, {}, {}, {}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            AddPassCommandBuffers( renderPassLocal, { &guiCommandBuffer,1 } );
            EndRenderPass( renderPassLocal );
            renderPassLocal.CmdBuff.End();
        }
    }

    // Blit Results to the screen
    {
        auto& renderPassLocal = BeginRenderPass(RP_BLIT, WhichFrame, {}, {}, pBlitCompleteSemaphore, VK_SUBPASS_CONTENTS_INLINE);
        AddDrawableToCmdBuffers(*m_BlitDrawable, &renderPassLocal.CmdBuff, 1, 1, WhichFrame);
        EndRenderPass( renderPassLocal );
        renderPassLocal.CmdBuff.StopGpuTimer( gpuFrameTimerId );    // end of frame
        m_GpuTimerPool->ReadResults( renderPassLocal.CmdBuff.m_VkCommandBuffer, WhichFrame );
        renderPassLocal.CmdBuff.End();
    }

    rThreadOutput.Fence = rThreadParam.CurrentBuffer.fence;
    rThreadOutput.SwapchainPresentIndx = rThreadParam.CurrentBuffer.swapchainPresentIdx;
    rThreadOutput.WhichFrame = WhichFrame;
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    // Reset Metrics
    m_TotalDrawCalls = 0;
    m_TotalTriangles = 0;

    static bool positive = false;
    if (positive)
    {
        //gShadowPosition.x += fltDiffTime * 30.0f;

        if (std::abs(gShadowPosition.x) > 40)
        {
            positive = false;
        }
    }
    else
    {
        //gShadowPosition.x -= fltDiffTime * 30.0f;

        if (std::abs(gShadowPosition.x) > 40)
        {
            positive = true;
        }
    }



    // Grab the vulkan wrapper
    Vulkan* pVulkan = GetVulkan();

    // See if we want to change backbuffer surface format
    if (m_RequestedSurfaceFormat.colorSpace != pVulkan->m_SurfaceColorSpace || m_RequestedSurfaceFormat.format != pVulkan->m_SurfaceFormat)
    {
        ChangeSurfaceFormat(m_RequestedSurfaceFormat);
    }

    // Obtain the next swap chain image for the next frame.
    auto CurrentVulkanBuffer = pVulkan->SetNextBackBuffer();

    //
    // Run any Updates that do not modify GPU resources.
    //

    UpdateCamera(fltDiffTime);

    // Wait for game thread to finish...
    m_GameThreadWorker.FinishAllWork();

    //
    // We are running single-threaded here...
    // 

    UpdateGui();

    if (gFramesToRender == 0)
    {
        //
        // Runing 'forever'.
        //
        // Do rendering 'threaded'.
        //

        // ... grab info from the last run ...
        int FrameToSubmit = m_CompletedThreadOutput.WhichFrame;
        int SwapchainPresentIndx = m_CompletedThreadOutput.SwapchainPresentIndx;
        VkFence FrameToRenderFence = m_CompletedThreadOutput.Fence;

        // Start thread updating the next frame.
        GameThreadInputParam gameThreadParam{ CurrentVulkanBuffer, fltDiffTime };

        m_GameThreadWorker.DoWork2( [](Application* pThis, Application::GameThreadInputParam ThreadParam) {
            pThis->RunGameThread(ThreadParam, pThis->m_CompletedThreadOutput);
        }, this, std::move(gameThreadParam));

        // Submit and present thread work from last frame (potentially while the thread worker is generating the next frame).
        if (FrameToSubmit >= 0)
        {
            auto finishedSemaphore = SubmitGameThreadWork(FrameToSubmit, FrameToRenderFence);
            PresentQueue(finishedSemaphore, SwapchainPresentIndx);
        }
    }
    else
    {
        //
        // Running a (pre)defined number of frames.
        //
        // Run syncronously so that we render the exact number of requested frames.
        //
        GameThreadInputParam gameThreadParam{ CurrentVulkanBuffer, fltDiffTime };
        RunGameThread(gameThreadParam, m_CompletedThreadOutput);

        // Submit and present this frame
        auto finishedSemaphore = SubmitGameThreadWork(m_CompletedThreadOutput.WhichFrame, m_CompletedThreadOutput.Fence);
        PresentQueue(finishedSemaphore, m_CompletedThreadOutput.SwapchainPresentIndx);
    }

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
bool Application::LoadTextures()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    m_TextureManager->SetDefaultFilenameManipulators(PathManipulator_ChangeExtension{ ".ktx" });

    // Load 'loose' textures
    m_TextureManager->GetOrLoadTexture("Environment" , *m_AssetManager, "./Media/Textures/simplesky_env.ktx", m_SamplerRepeat);
    m_TextureManager->GetOrLoadTexture("Irradiance", *m_AssetManager, "./Media/Textures/simplesky_irradiance.ktx", m_SamplerRepeat);
    //m_loadedTextures.emplace("Environment", *m_AssetManager, "./Media/Textures/simplesky_env.ktx", SamplerAddressMode::Repeat));
    //m_loadedTextures.emplace("Irradiance", *m_AssetManager, "./Media/Textures/.ktx", SamplerAddressMode::Repeat));

    m_TexWhite = m_TextureManager->GetOrLoadTexture("White", *m_AssetManager, "./Media/Textures/white_d.ktx", m_SamplerRepeat);
    m_DefaultNormal = m_TextureManager->GetOrLoadTexture("Normal", *m_AssetManager, "./Media/Textures/normal_default.ktx", m_SamplerRepeat);

    if (!m_TexWhite || !m_DefaultNormal)
    {
        return false;
    }

    return true;
}

bool Application::CreateRenderTargets()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Creating Render Targets...");
    LOGI("******************************");

    Vulkan* pVulkan = GetVulkan();

    const TextureFormat DesiredDepthFormat = pVulkan->GetBestSurfaceDepthFormat();

    // Setup the GBuffer
    const TextureFormat GbufferColorType[] = { TextureFormat::R8G8B8A8_SRGB/*Albedo*/, TextureFormat::R16G16B16A16_SFLOAT/*Normals*/ };

    if (!m_GBufferRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, GbufferColorType, DesiredDepthFormat, VK_SAMPLE_COUNT_1_BIT, "GBuffer RT"))
    {
        LOGE("Unable to create gbuffer render target");
    }

    // Setup the 'shadow' (ray query result) buffer
    const TextureFormat ShadowColorType[] = { TextureFormat::R8_UNORM };

    if (!m_ShadowRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, ShadowColorType, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "Shadow RT"))
    {
        LOGE("Unable to create shadow render target");
    }

    // Setup the 'main' (compositing) buffer
    const TextureFormat MainColorType[] = { TextureFormat::R16G16B16A16_SFLOAT };

    if (!m_MainRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, MainColorType, m_GBufferRT/*inherit depth*/, VK_SAMPLE_COUNT_1_BIT, "Main RT"))
    {
        LOGE("Unable to create main render target");
    }

    // Setup the HUD render target (no depth)
    const TextureFormat HudColorType[] = { TextureFormat::R8G8B8A8_UNORM };

    if (!m_HudRT.Initialize(pVulkan, gSurfaceWidth, gSurfaceHeight, HudColorType, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitShadowMap()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();
    for (uint32_t i = 0; i < cNumShadows; ++i)
    {
        if (!m_Shadows[i].Initialize(*pVulkan, gShadowMapWidth, gShadowMapHeight, false))
        {
            LOGE("Unable to create shadow (render target?)");
            return false;
        }
        if (gRasterizedShadowFarPlane > gFarPlane)
            gRasterizedShadowFarPlane = gFarPlane;
        m_Shadows[i].SetLightPos(gRasterizedShadowPosition, gRasterizedShadowTarget);
        m_Shadows[i].SetEyeClipPlanes(m_Camera.Fov(), m_Camera.NearClip(), m_Camera.Aspect(), gRasterizedShadowFarPlane);
    }

    if (!m_ShadowVSM.Initialize(*pVulkan, *m_ShaderManager, *m_MaterialManager, m_Shadows[0]))
    {
        LOGE("Unable to create shadow VSM");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitLighting()
//-----------------------------------------------------------------------------
{
    m_LightColor = { 1.0f, 0.98f, 0.73f, gSpecularExponent };

    m_AdditionalDeferredLightsData.clear();
    m_AdditionalDeferredLightsData.push_back( { glm::vec3( 768.7, 440.f, 877.4), 1.0f, 10.0f } );
    m_AdditionalDeferredLightsData.push_back( { glm::vec3( -433.9f, 440.f, -420.f), 1.0f, 10.0f } );
    //Handled by the Light.frag pass: m_AdditionalDeferredLightsData.push_back( { glm::vec3( -211.f, 440.f, 470.f ), 1.0f, 10.0f } );
    m_AdditionalDeferredLightsData.push_back( { glm::vec3( -966, 440.f, 210 ), 1.0f, 10.0f } );
    m_AdditionalDeferredLightsData.push_back( { glm::vec3( -177.0, 440.f, 1000 ), 1.0f, 10.0f } );
    m_AdditionalDeferredLightsData.push_back( { glm::vec3( 375.f, 440.f, -2130.f ), 1.0f, 10.0f } );
    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    m_ShaderManager->RegisterRenderPassNames({ sRenderPassNames, sRenderPassNames + (sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0])) });

    LOGI("Loading Shaders...");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
        { tIdAndFilename { "ObjectDeferred", "Media\\Shaders\\ObjectDeferred.json" },
          tIdAndFilename { "ObjectEmissive", "Media\\Shaders\\ObjectEmissive.json" },
          tIdAndFilename { "ObjectAnimated", "Media\\Shaders\\ObjectAnimated.json" },
          tIdAndFilename { "Skybox", "Media\\Shaders\\Skybox.json" },
          tIdAndFilename { "Light", "Media\\Shaders\\Light.json" },
          tIdAndFilename { "RasterizedShadowCull", "Media\\Shaders\\RasterizedShadowCull.json" },
          tIdAndFilename { "LightRasterizedShadows", "Media\\Shaders\\LightRasterizedShadows.json" },
          tIdAndFilename { "Blit", "Media\\Shaders\\Blit.json" },
          tIdAndFilename { "ShadowRQ", "Media\\Shaders\\ShadowRQ.json" },
          tIdAndFilename { "ShadowRQFrag", "Media\\Shaders\\ShadowRQFrag.json" },
          tIdAndFilename { "VarianceShadowMap", "Media\\Shaders\\VarianceShadowMap.json" },
          tIdAndFilename { "PointLight", "Media\\Shaders\\PointLight.json" },
          tIdAndFilename { "AnimateBuffer", "Media\\Shaders\\AnimateBuffer.json" }
        })
    {
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitUniforms()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    // These are only created here, they are not set to initial values
    LOGI("Creating uniform buffers...");

    m_ObjectVertUniformData = {};

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        CreateUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass]);
        CreateUniformBuffer(pVulkan, m_ObjectFragUniform[WhichPass]);
        CreateUniformBuffer(pVulkan, m_SkyboxVertUniform[WhichPass]);
    }

    // Streetlights (outdoor)
    m_LightFragUniformData.PointLightPosition = glm::vec4(-211.f, 440.f, 470.f, 25.0f);
    m_LightFragUniformData.PointLightColor = glm::vec4(1.0f, 0.8f, 0.7f, 1.0f);
    m_LightFragUniformData.DirectionalLightDirection = glm::vec4( glm::normalize(gRasterizedShadowTarget - gRasterizedShadowPosition), 0.0f );
    m_LightFragUniformData.DirectionalLightColor = glm::vec4(0.9f, 0.8f, 1.0f, 1.0f);
    m_LightFragUniformData.AmbientColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    m_LightFragUniformData.PointLightRadius = gLightEmitterRadius;
    m_LightFragUniformData.PointLightCutoff = gLightAttenuationCutoff;
    m_LightFragUniformData.SpecScale = gSpecularScale;
    m_LightFragUniformData.SpecPower = gSpecularExponent;
    m_LightFragUniformData.irradianceAmount = gIrradianceAmount;
    m_LightFragUniformData.irradianceMipLevel = gIrradianceMip;
    m_LightFragUniformData.AmbientOcclusionScale = 1.0f;

    CreateUniformBuffer(pVulkan, m_ShadowRQCtrlUniform, &m_ShadowRQCtrl);
    CreateUniformBuffer(pVulkan, m_BlitFragUniform, &m_BlitFragUniformData);
    CreateUniformBuffer(pVulkan, m_LightFragUniform, &m_LightFragUniformData);

    m_sceneAnimatedMeshUniformData = {};
    CreateUniformBuffer(pVulkan, m_sceneAnimatedMeshUniform, &m_sceneAnimatedMeshUniformData);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t WhichBuffer, float ElapsedTime)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    // Special View matrix for Skybox
    glm::mat4 SkyboxViewMatrix = glm::mat4_cast(m_Camera.Rotation());
    glm::mat4 SkyboxMVP = m_Camera.ProjectionMatrix() * SkyboxViewMatrix;

    // ********************************
    // Object
    // ********************************
    glm::mat4 ObjectVP = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix();

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // Skip passes which do not rasterize object geometry.
        if (WhichPass != RP_RASTERSHADOW && WhichPass != RP_GBUFFER)
            continue;

        switch (WhichPass)
        {
        case RP_RASTERSHADOW:
            m_ObjectVertUniformData.VPMatrix = m_Shadows[0].GetViewProj();
            break;
        case RP_GBUFFER:
            m_ObjectVertUniformData.VPMatrix = ObjectVP;
            break;
        default:
            LOGE("***** %s Not Handled! *****", GetPassName(WhichPass));
            break;
        }

        if (gAnimateBLAS)
        {
            m_ObjectVertUniformData.AnimationRotation.x += ElapsedTime * 4.0f;
            m_ObjectVertUniformData.AnimationRotation.y += ElapsedTime * 4.1f;
            m_ObjectVertUniformData.AnimationRotation.z += ElapsedTime * 0.5f;
            m_ObjectVertUniformData.AnimationRotation.w += ElapsedTime * 0.52f;
            if (m_ObjectVertUniformData.AnimationRotation.x > PI_MUL_2)
                m_ObjectVertUniformData.AnimationRotation.x -= PI_MUL_2;
            if (m_ObjectVertUniformData.AnimationRotation.y > PI_MUL_2)
                m_ObjectVertUniformData.AnimationRotation.y -= PI_MUL_2;
            if (m_ObjectVertUniformData.AnimationRotation.z > PI_MUL_2)
                m_ObjectVertUniformData.AnimationRotation.z -= PI_MUL_2;
            if (m_ObjectVertUniformData.AnimationRotation.w > PI_MUL_2)
                m_ObjectVertUniformData.AnimationRotation.w -= PI_MUL_2;
        }

        UpdateUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass], m_ObjectVertUniformData, WhichBuffer);

        m_ObjectFragUniformData.Color = glm::vec4(1.0f,1.0f,1.0f, 1.0f);  // White by default

        m_ObjectFragUniformData.NormalHeight = glm::vec4(gNormalAmount, gNormalMirrorReflectAmount, 0.0f, 0.0f);

        UpdateUniformBuffer(pVulkan, m_ObjectFragUniform[WhichPass], m_ObjectFragUniformData, WhichBuffer);
    }

    // ********************************
    // Rayqueried Shadow
    // ********************************
    m_ShadowRQCtrl.ScreenSize = glm::vec4((float)m_ShadowRayQueryComputeOutput.Width, (float)m_ShadowRayQueryComputeOutput.Height, 1.0f / (float)m_ShadowRayQueryComputeOutput.Width, 1.0f / (float)m_ShadowRayQueryComputeOutput.Height);
    glm::mat4 CameraProjectionInv = glm::inverse(m_Camera.ProjectionMatrix());
    m_ShadowRQCtrl.ProjectionInv = CameraProjectionInv;
    m_ShadowRQCtrl.ViewInv= glm::inverse(m_Camera.ViewMatrix());
    //m_ShadowRQCtrl.LightWorldPos = m_Shadows[0].GetLightPos();    // Rasterized shadow position
    m_ShadowRQCtrl.LightWorldPos = gShadowDirectionalLight ? glm::vec4(gRasterizedShadowPosition, 1.0) : glm::vec4(gShadowPosition, 1.0);
    m_ShadowRQCtrl.LightWorldDirection = glm::vec4(glm::normalize(gRasterizedShadowTarget - gRasterizedShadowPosition), 0.0f);

    UpdateUniformBuffer(pVulkan, m_ShadowRQCtrlUniform, m_ShadowRQCtrl, WhichBuffer);

    // ********************************
    // Animated meshes
    // ********************************
    m_sceneAnimatedMeshUniformData.AnimationRotation = m_ObjectVertUniformData.AnimationRotation;
    UpdateUniformBuffer(pVulkan, m_sceneAnimatedMeshUniform, m_sceneAnimatedMeshUniformData, WhichBuffer);

    // ********************************
    // Skybox
    // ********************************
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // No uniform buffers for HUD or BLIT since objects not in HUD and BLIT
        if (WhichPass == RP_HUD || WhichPass == RP_BLIT)
            continue;

        glm::mat4 LocalModel = glm::scale(glm::vec3(m_SkyboxScale, m_SkyboxScale, m_SkyboxScale));

        // Special View matrix for Skybox (always centered on the view position)
        SkyboxViewMatrix = glm::mat4_cast(m_Camera.Rotation());
        glm::mat4 SkyboxMVP = m_Camera.ProjectionMatrix() * SkyboxViewMatrix * LocalModel;

        m_SkyboxVertUniformData.MVPMatrix = SkyboxMVP;
        m_SkyboxVertUniformData.ModelMatrix = LocalModel;
        m_SkyboxVertUniformData.Color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);  // White by default
        UpdateUniformBuffer(pVulkan, m_SkyboxVertUniform[WhichPass], m_SkyboxVertUniformData, WhichBuffer);
    }

    // ********************************
    // Fullscreen Light pass
    // ********************************

    glm::mat4 CameraViewInv = glm::inverse(m_Camera.ViewMatrix());
    m_LightFragUniformData.ProjectionInv = CameraProjectionInv;
    m_LightFragUniformData.ViewInv = CameraViewInv;
    m_LightFragUniformData.WorldToShadow = m_Shadows[0].GetViewProj();
    m_LightFragUniformData.CameraPos = glm::vec4( m_Camera.Position(), 0.0f );

    UpdateUniformBuffer(pVulkan, m_LightFragUniform, m_LightFragUniformData, WhichBuffer);

    // ********************************
    // 'Additional' Lights
    // ********************************
    {
        glm::mat4 LocalVP = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix();
        PointLightUB pointLightUniformData{};
        for(uint32_t WhichLight=0; WhichLight<m_AdditionalDeferredLightsData.size(); ++WhichLight )
        {
            auto& lightdata = m_AdditionalDeferredLightsData[WhichLight];
            if (WhichLight >= m_AdditionalDeferredLightsSharedUniform.size())
                break;

            lightdata.Radius = gLightEmitterRadius * (sqrtf( lightdata.Intensity / gLightAttenuationCutoff ) - 1.0f);

            pointLightUniformData.MVPMatrix = LocalVP * glm::translate( glm::vec3( lightdata.Position ) ) * glm::scale( glm::vec3(lightdata.Radius, lightdata.Radius, lightdata.Radius) );
            pointLightUniformData.ProjectionInv = CameraProjectionInv;
            pointLightUniformData.ViewInv = CameraViewInv;

            pointLightUniformData.LightPosition = lightdata.Position;
            pointLightUniformData.LightIntensity = lightdata.Intensity;
            pointLightUniformData.LightCutoff = gLightAttenuationCutoff;
            pointLightUniformData.LightRadius = gLightEmitterRadius;
            pointLightUniformData.LightColor = m_LightColor;
            pointLightUniformData.SpecScale = m_LightFragUniformData.SpecScale;
            pointLightUniformData.SpecPower = m_LightFragUniformData.SpecPower;
            pointLightUniformData.WindowSize = glm::vec2( m_GBufferRT.m_RenderTargets[0].m_Width, m_GBufferRT.m_RenderTargets[0].m_Height );
            UpdateUniformBuffer(pVulkan, m_AdditionalDeferredLightsSharedUniform[WhichLight], pointLightUniformData, WhichBuffer);
        }
    }

    // ********************************
    // Rasterized shadow culling
    // ********************************
    if (gRasterizedShadow && gRasterizedShadowCulled)
    {
        ShadowCullCameraCtrl rasterizedShadowCullCtrl{};
        auto projMtx = m_Shadows[0].GetProjection();
        rasterizedShadowCullCtrl.MVPMatrix = m_Shadows[0].GetViewProj();
        const ViewFrustum cameraFrustum(projMtx, m_Shadows[0].GetView());
        memcpy(rasterizedShadowCullCtrl.CullPlanes, cameraFrustum.GetPlanes(), sizeof(rasterizedShadowCullCtrl.CullPlanes));
        UpdateUniformBuffer(pVulkan, m_ShadowInstanceCullingCameraUniform, rasterizedShadowCullCtrl, WhichBuffer);
    }


    // ********************************
    // Blit
    // ********************************

    m_BlitFragUniformData.sRGB = m_bEncodeSRGB ? 1 : 0;
    UpdateUniformBuffer(pVulkan, m_BlitFragUniform, m_BlitFragUniformData);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    LOGI("******************************");
    LOGI("Building Command Buffers...");
    LOGI("******************************");

    uint32_t TargetWidth = pVulkan->m_SurfaceWidth;
    uint32_t TargetHeight = pVulkan->m_SurfaceHeight;
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // If viewport and scissor are dynamic we need to add them to the secondary buffers.
        const uint32_t TargetWidth = m_PassSetup[WhichPass].TargetWidth;
        const uint32_t TargetHeight = m_PassSetup[WhichPass].TargetHeight;

        VkViewport Viewport = {};
        Viewport.x = 0.0f;
        Viewport.y = 0.0f;
        Viewport.width = (float)TargetWidth;
        Viewport.height = (float)TargetHeight;
        Viewport.minDepth = 0.0f;
        Viewport.maxDepth = 1.0f;

        VkRect2D Scissor = {};
        Scissor.offset.x = 0;
        Scissor.offset.y = 0;
        Scissor.extent.width = TargetWidth;
        Scissor.extent.height = TargetHeight;

        for (uint32_t WhichBuffer = 0; WhichBuffer < pVulkan->m_SwapchainImageCount; WhichBuffer++)
        {
            // Set up some values that change based on render pass
            VkRenderPass WhichRenderPass = VK_NULL_HANDLE;
            VkFramebuffer WhichFramebuffer = pVulkan->m_SwapchainBuffers[WhichBuffer].framebuffer;
            switch (WhichPass)
            {
            case RP_GBUFFER:
                WhichRenderPass = m_GBufferRT.m_RenderPass;
                WhichFramebuffer = m_GBufferRT[0].m_FrameBuffer;
                break;

            case RP_RASTERSHADOW:
                WhichRenderPass = m_Shadows[0].GetRenderPass();
                WhichFramebuffer = m_Shadows[0].GetFramebuffer();
                break;

            case RP_RAYSHADOW:
                WhichRenderPass = m_ShadowRT.m_RenderPass;
                WhichFramebuffer = m_ShadowRT[0].m_FrameBuffer;
                break;

            case RP_LIGHT:
                WhichRenderPass = m_MainRT.m_RenderPass;
                WhichFramebuffer = m_MainRT[0].m_FrameBuffer;
                break;

            case RP_HUD:
                WhichRenderPass = m_HudRT.m_RenderPass;
                WhichFramebuffer = m_HudRT[0].m_FrameBuffer;
                break;

            case RP_BLIT:
                WhichRenderPass = m_RenderPass[WhichPass];
                WhichFramebuffer = pVulkan->m_SwapchainBuffers[WhichBuffer].framebuffer;
                break;
            }

            if (WhichPass == RP_LIGHT)
            {
                // Main (primary) lighting pass applied to the gbuffer
                if (!m_LightCmdBuffer[WhichBuffer].Begin(WhichFramebuffer, WhichRenderPass))
                {
                    return false;
                }
                vkCmdSetViewport(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);

                // Additional lights applied to the gbuffer (additive after the m_LightCmdBuffer pass)
                if (!m_AdditionalLightsCmdBuffer[WhichBuffer].Begin(WhichFramebuffer, WhichRenderPass))
                {
                    return false;
                }
                vkCmdSetViewport(m_AdditionalLightsCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_AdditionalLightsCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
            }

            // Objects - can render into any pass (except Blit)
            if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].Begin(WhichFramebuffer, WhichRenderPass))
            {
                return false;
            }
            vkCmdSetViewport(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer, 0, 1, &Viewport);
            vkCmdSetScissor(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer, 0, 1, &Scissor);
        }   // Which Buffer
    }   // Which Pass

    // Run through the drawables (each one may be in multiple command buffers)
    for (const auto& drawable : m_SceneDrawables)
    {
        AddDrawableToCmdBuffers(drawable, &m_ObjectCmdBuffer[0][0], NUM_RENDER_PASSES, pVulkan->m_SwapchainImageCount);
    }
    // Add the 'combined' rasterized shadow drawable (monolithic indexed and instanced drawable)
    if (m_ShadowRasterizedDrawable)
    {
        AddDrawableToCmdBuffers(*m_ShadowRasterizedDrawable, &m_ObjectCmdBuffer[0][0], NUM_RENDER_PASSES, pVulkan->m_SwapchainImageCount);
    }
    // Add the skybox (last)
    if (m_SkyboxDrawable)
    {
        AddDrawableToCmdBuffers(*m_SkyboxDrawable, &m_ObjectCmdBuffer[0][0], NUM_RENDER_PASSES, pVulkan->m_SwapchainImageCount);
    }

    // and end their pass
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        for (uint32_t WhichBuffer = 0; WhichBuffer < pVulkan->m_SwapchainImageCount; WhichBuffer++)
        {
            if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].End())
            {
                return false;
            }
        }
    }

    // Add the main light
    AddDrawableToCmdBuffers( *m_LightDrawable.get(), m_LightCmdBuffer, 1, pVulkan->m_SwapchainImageCount );

    // Add the 'additional' lights
    for(uint32_t additionalLightIndex = 0; additionalLightIndex < (uint32_t) m_AdditionalDeferredLightDrawables.size(); ++additionalLightIndex)
    {
        const auto& drawable = m_AdditionalDeferredLightDrawables[additionalLightIndex];
        char timerName[64];
        sprintf(timerName, "AdditionalLight %u RayQuery", additionalLightIndex+1 );
        for (uint32_t WhichBuffer = 0; WhichBuffer < pVulkan->m_SwapchainImageCount; ++WhichBuffer)
        {
            auto timerId = m_AdditionalLightsCmdBuffer[WhichBuffer].StartGpuTimer(timerName);
            AddDrawableToCmdBuffers(drawable, &m_AdditionalLightsCmdBuffer[WhichBuffer], 1, 1);
            m_AdditionalLightsCmdBuffer[WhichBuffer].StopGpuTimer(timerId);
        }
    }
    for (uint32_t WhichBuffer = 0; WhichBuffer < pVulkan->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_LightCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
        if (!m_AdditionalLightsCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitLocalSemaphores()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = GetVulkan();

    VkSemaphoreCreateInfo SemaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_BlitCompleteSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
const char* Application::GetPassName(uint32_t WhichPass)
//-----------------------------------------------------------------------------
{
    if (WhichPass >= sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0]))
    {
        LOGE("GetPassName() called with unknown pass (%d)!", WhichPass);
        return "RP_UNKNOWN";
    }
    return sRenderPassNames[WhichPass];
}

//-----------------------------------------------------------------------------
bool Application::InitMaterials()
//-----------------------------------------------------------------------------
{
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    char szName[256];
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
        {
            // The Pass Command Buffer => Primary
            sprintf(szName, "Primary (%s; Buffer %d of %d)", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_PassCmdBuffer[WhichBuffer][WhichPass].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY, Vulkan::eGraphicsQueue, m_GpuTimerPool.get()))
            {
                return false;
            }

            // Model => Secondary
            sprintf(szName, "Model (%s; Buffer %d of %d)", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
            {
                return false;
            }

        }
    }

    // Light => Secondary
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        sprintf( szName, "Light (Buffer %d of %d)", WhichBuffer + 1, NUM_VULKAN_BUFFERS );
        if (!m_LightCmdBuffer[WhichBuffer].Initialize( pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY, Vulkan::eGraphicsQueue, m_GpuTimerPool.get() ))
        {
            return false;
        }
    }

    // AdditionalLight => Secondary
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        sprintf(szName, "Additional Lights (%s; Buffer %d of %d)", GetPassName(RP_LIGHT), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
        if (!m_AdditionalLightsCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY, Vulkan::eGraphicsQueue, m_GpuTimerPool.get()))
        {
            return false;
        }
    }

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitAllRenderPasses()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = GetVulkan();

    uint32_t ShadowTargetWidth, ShadowTargetHeight;
    m_Shadows[0].GetTargetSize(ShadowTargetWidth, ShadowTargetHeight);

    // Fill in the Setup data
    m_PassSetup[RP_GBUFFER] = { m_GBufferRT[0].m_pLayerFormats, m_GBufferRT[0].m_DepthFormat,      RenderPassInputUsage::Clear,   true,  RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Store,      gClearColor,m_GBufferRT[0].m_Width,   m_GBufferRT[0].m_Height };
    m_PassSetup[RP_RASTERSHADOW] = { {},                        m_Shadows[0].GetDepthFormat(),     RenderPassInputUsage::DontCare,true,  RenderPassOutputUsage::Discard,       RenderPassOutputUsage::StoreReadOnly,{},       ShadowTargetWidth,        ShadowTargetHeight };
    m_PassSetup[RP_RAYSHADOW]={ m_ShadowRT[0].m_pLayerFormats,  m_ShadowRT[0].m_DepthFormat,       RenderPassInputUsage::DontCare,false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},         m_ShadowRT[0].m_Width,    m_ShadowRT[0].m_Height };
    m_PassSetup[RP_LIGHT] =   { m_MainRT[0].m_pLayerFormats,    m_GBufferRT[0].m_DepthFormat,      RenderPassInputUsage::Clear,   false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},         m_MainRT[0].m_Width,      m_MainRT[0].m_Height };
    m_PassSetup[RP_HUD] =     { m_HudRT[0].m_pLayerFormats,     m_HudRT[0].m_DepthFormat,          RenderPassInputUsage::Clear,   false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},         m_HudRT[0].m_Width,       m_HudRT[0].m_Height };
    m_PassSetup[RP_BLIT] =    { {pVulkan->m_SurfaceFormat},     pVulkan->m_SwapchainDepth.format,  RenderPassInputUsage::DontCare,false, RenderPassOutputUsage::Present,       RenderPassOutputUsage::Discard,    {},         pVulkan->m_SurfaceWidth,  pVulkan->m_SurfaceHeight };

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        const auto& PassSetup = m_PassSetup[WhichPass];
        bool IsSwapChainRenderPass = WhichPass == RP_BLIT;

        if (WhichPass == RP_RASTERSHADOW)
            m_RenderPass[RP_RASTERSHADOW] = m_Shadows[0].GetRenderPass();
        else if (PassSetup.ColorFormats.size() > 0 || PassSetup.DepthFormat != TextureFormat::UNDEFINED)
        {
            if (!pVulkan->CreateRenderPass({PassSetup.ColorFormats},
                PassSetup.DepthFormat,
                VK_SAMPLE_COUNT_1_BIT,
                PassSetup.ColorInputUsage,
                PassSetup.ColorOutputUsage,
                PassSetup.ClearDepthRenderPass,
                PassSetup.DepthOutputUsage,
                &m_RenderPass[WhichPass]))
                return false;
        }


        // LOGI("    Render Pass (%s; Buffer %d of %d) => 0x%x", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS, m_RenderPass[WhichPass][WhichBuffer]);
    }   // Which Pass

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitDrawables()
//-----------------------------------------------------------------------------
{
    LOGI("Creating Test Drawable...");
    Vulkan* pVulkan = GetVulkan();

    const auto& bufferLoader = [&](const std::string& bufferSlotName) -> tPerFrameVkBuffer {
        if (bufferSlotName == "Vert")
        {
            return { m_ObjectVertUniform[RP_GBUFFER].vkBuffers };
        }
        else if (bufferSlotName == "VertShadow")
        {
            return { m_ObjectVertUniform[RP_RASTERSHADOW].vkBuffers };
        }
        else if (bufferSlotName == "Frag")
        {
            return { m_ObjectFragUniform[RP_GBUFFER].vkBuffers };
        }
        else
        {
            assert(0);
            return {};
        }
    };

    const auto* pOpaqueShader = m_ShaderManager->GetShader("ObjectDeferred");
    if (!pOpaqueShader)
    {
        // Error (bad shaderName)
        return false;
    }

    const auto* pEmissiveShader = m_ShaderManager->GetShader("ObjectEmissive");
    if (!pEmissiveShader)
    {
        // Error (bad shaderName)
        return false;
    }

    const auto* pAnimatedShader = m_ShaderManager->GetShader("ObjectAnimated");
    if (!pAnimatedShader)
    {
        // Error (bad shaderName)
        return false;
    }

    auto modelTextureLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef, const std::string& textureSlotName) -> const MaterialPass::tPerFrameTexInfo
    {
        if (textureSlotName == "Environment" || textureSlotName == "Irradiance")
        {
            auto* texture = m_TextureManager->GetTexture(textureSlotName);
            if (texture == nullptr)
                return { m_TexWhite };
            return { texture };
        }
        const bool normalMap = (textureSlotName == "Normal");
        const bool specMap = !normalMap && (textureSlotName == "SpecMap");
        const bool emissiveMap = !normalMap && !specMap && (textureSlotName == "Emissive");

        // See if we can get the filename from the loaded material definition.  Take a copy so we can manipulate as needed.  
        std::string textureName = specMap ? materialDef.specMapFilename : (normalMap ? materialDef.bumpFilename : (emissiveMap ? materialDef.emissiveFilename : materialDef.diffuseFilename));

        if (textureName.empty() && specMap)
        {
            textureName = materialDef.bumpFilename;
            size_t normal = textureName.find("_Normal.");
            if (normal != -1)
            {
                textureName.replace(normal, 8, "_Specular.");
            }
            else
            {
                textureName.clear();
            }
        }

        auto* texture = m_TextureManager->GetOrLoadTexture(*m_AssetManager, textureName, m_SamplerRepeat, PathManipulator_PrefixDirectory{ "Media\\" }, PathManipulator_ChangeExtension{ ".ktx" });
        if (texture==nullptr)
        {
            // File not loaded, use default
            return { (normalMap ? m_DefaultNormal : m_TexWhite) };
        }
        return { texture };
    };

    const auto& modelMaterialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef) -> std::optional<Material>
    {
        using namespace std::placeholders;
        if (materialDef.diffuseFilename.find(gAnimatedMaterialName) != std::string::npos)
        {
            return m_MaterialManager->CreateMaterial(*pVulkan, *pAnimatedShader, NUM_VULKAN_BUFFERS, std::bind(modelTextureLoader, std::cref(materialDef), _1), bufferLoader);
        }
        else if (!materialDef.emissiveFilename.empty())
        {
            return m_MaterialManager->CreateMaterial(*pVulkan, *pEmissiveShader, NUM_VULKAN_BUFFERS, std::bind(modelTextureLoader, std::cref(materialDef), _1), bufferLoader);
        }
        else
        {
            return m_MaterialManager->CreateMaterial(*pVulkan, *pOpaqueShader, NUM_VULKAN_BUFFERS, std::bind(modelTextureLoader, std::cref(materialDef), _1), bufferLoader);
        }
    };

    // Load .gltf file
    const uint32_t loaderFlags = DrawableLoader::LoaderFlags::FindInstances | DrawableLoader::LoaderFlags::IgnoreHierarchy;
    auto fatObjects = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gSceneObjectName, (loaderFlags & DrawableLoader::LoaderFlags::IgnoreHierarchy) != 0);
    // Print some debug
    DrawableLoader::PrintStatistics(fatObjects);
    // Pre-load the textures (likely to be faster than laoading one at a time, may do some threading etc)
    m_TextureManager->BatchLoad(*m_AssetManager, MeshObjectIntermediate::ExtractTextureNames(fatObjects), m_SamplerRepeat, PathManipulator_PrefixDirectory{ "Media\\" }, PathManipulator_ChangeExtension{ ".ktx" });

    // See if we can find instances, we assume there is no instance information in the gltf!
    auto instancedFatObjects = (loaderFlags & DrawableLoader::LoaderFlags::FindInstances) ? MeshInstanceGenerator::FindInstances(std::move(fatObjects)) : MeshInstanceGenerator::NullFindInstances(std::move(fatObjects));

    // Make a single model mesh for the culled shadow (single material, multiple instances)
    if (gRasterizedShadow)
    {
        auto fatObjects = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gSceneObjectName, (loaderFlags & DrawableLoader::LoaderFlags::IgnoreHierarchy) != 0);
        // See if we can find instances, we assume there is no instance information in the gltf!
        auto instancedFatObjects = (loaderFlags & DrawableLoader::LoaderFlags::FindInstances) ? MeshInstanceGenerator::FindInstances(std::move(fatObjects)) : MeshInstanceGenerator::NullFindInstances(std::move(fatObjects));

        if (!instancedFatObjects.empty())
        {
            size_t totalVerts = 0;
            size_t totalIndices = 0;
            size_t totalInstances = 0;

            for (const auto& instancedObject : instancedFatObjects)
            {
                totalVerts += instancedObject.mesh.m_VertexBuffer.size();
                totalIndices += instancedObject.mesh.CalcNumTriangles() * 3;
                totalInstances += instancedObject.instances.size();
            }

            std::vector<MeshObjectIntermediate::FatInstance::tInstanceTransform> instances;
            instances.reserve(totalInstances);
            std::vector< VkDrawIndexedIndirectCommand> indirectCommands;
            indirectCommands.reserve(totalInstances);
            MeshObjectIntermediate::tVertexBuffer vertices;
            vertices.reserve(totalVerts);
            std::vector<uint32_t> indices;
            indices.reserve(totalIndices);

            for (const auto& instancedObject : instancedFatObjects)
            {
                indirectCommands.emplace_back(VkDrawIndexedIndirectCommand{ .indexCount = (uint32_t)instancedObject.mesh.CalcNumTriangles() * 3,
                                                                            .instanceCount = (uint32_t)instancedObject.instances.size(),
                                                                            .firstIndex = (uint32_t)indices.size(),
                                                                            .vertexOffset = (int32_t)vertices.size(),
                                                                            .firstInstance = (uint32_t)instances.size() });
                // Add the instances
                std::transform(instancedObject.instances.begin(), instancedObject.instances.end(), std::back_inserter(instances), [](auto instance) { return instance.transform; });
                // Add the vertices
                vertices.insert(vertices.end(), instancedObject.mesh.m_VertexBuffer.begin(), instancedObject.mesh.m_VertexBuffer.end());
                // Add the indices (some finagling due to potentially different index buffer formats)
                const size_t startIdx = indices.size();
                std::visit([&indices, &instancedObject](auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::vector<uint32_t>>)
                    {
                        indices.insert(indices.end(), v.begin(), v.end());
                    }
                    else if constexpr (std::is_same_v<T, std::vector<uint16_t>>)
                    {
                        std::transform(v.begin(), v.end(), std::back_inserter(indices), [](auto idx) -> uint32_t { return idx; });
                    }
                    else if constexpr (!std::is_same_v <T, std::monostate>)
                    {
                        for (uint32_t i = 0; i < instancedObject.mesh.m_VertexBuffer.size(); ++i)
                            indices.push_back(i);
                    }
                }, instancedObject.mesh.m_IndexBuffer);
            }

            MeshObjectIntermediate MeshObjectIntermediate{};
            MeshObjectIntermediate.m_VertexBuffer = std::move(vertices);
            MeshObjectIntermediate.m_IndexBuffer = std::move(indices);

            // Now create the Vulkan representations
            const auto* pRasterizedShadowShader = pOpaqueShader;
            assert(pRasterizedShadowShader);
            auto material = m_MaterialManager->CreateMaterial(*pVulkan, *pRasterizedShadowShader, NUM_VULKAN_BUFFERS,
                [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
                    return { m_TexWhite };
                },
                [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    return { m_ObjectVertUniform[RP_RASTERSHADOW].vkBuffers };
                });

            MeshObject mesh;
            const auto& vertexFormats = pRasterizedShadowShader->m_shaderDescription->m_vertexFormats;
            MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate, 0, vertexFormats, &mesh);
            MeshObjectIntermediate.Release();

            const auto instanceFormatIt = std::find_if(vertexFormats.cbegin(), vertexFormats.cend(),
                                                        [](const VertexFormat& f) { return f.inputRate == VertexFormat::eInputRate::Instance; });

            VertexBufferObject vertexInstanceBuffer;
            vertexInstanceBuffer.Initialize(&pVulkan->GetMemoryManager(), instances.size(), instances.data(), false, BufferUsageFlags::Vertex);

            m_ShadowInstanceUnculledDrawIndirectBuffer = std::make_unique<DrawIndirectBufferObject>(true);
            struct {
                uint32_t count;
                uint32_t _pad[15];
            } indirectPrequelData{ .count = (uint32_t)indirectCommands.size() };    // indirect draw count, padded to 64 bytes
            m_ShadowInstanceUnculledDrawIndirectBuffer->Initialize(&pVulkan->GetMemoryManager(), indirectCommands.size(), indirectCommands.data(), &indirectPrequelData, BufferUsageFlags::Storage | BufferUsageFlags::Indirect);

            DrawIndirectBufferObject culledDrawIndirectBufferObject{ true };

            if (gRasterizedShadowCulled)
            {
                // For culling make an indirect draw buffer that we will update from a compute shader.
                size_t worseCaseIndirectDraws = indirectCommands.size() + (instances.size() - indirectCommands.size()) / 2;
                culledDrawIndirectBufferObject.Initialize<VkDrawIndexedIndirectCommand, decltype(indirectPrequelData)>(&pVulkan->GetMemoryManager(), worseCaseIndirectDraws, nullptr, nullptr, BufferUsageFlags::Storage | BufferUsageFlags::Indirect);
                // Buffer to contain the intermediate array of visible instances
                m_ShadowInstanceCulledData.Initialize(&pVulkan->GetMemoryManager(), 16/*header*/ + sizeof(uint32_t) * instances.size(), BufferUsageFlags::Storage, nullptr);

                // Create a storage buffer with instance centers and radii (in a vec4 per instance) and a 16byte header containing instance count.
                std::vector<uint32_t> instanceCullingData;
                instanceCullingData.resize(4 /* 4 byte instance count followed by pad */ + instances.size() * 4 /* vec4 per instance*/);
                instanceCullingData[0] = (uint32_t) instanceCullingData.size();
                glm::vec4* pInstanceCenterAndRadii = (glm::vec4*) &instanceCullingData[4];
                for (auto instance: instances)
                    *pInstanceCenterAndRadii++ = glm::vec4(instance[0][3], instance[1][3], instance[2][3], 10.0f/*hack*/);
                CreateUniformBuffer(pVulkan, &m_ShadowInstanceCullingBuffer, instanceCullingData.size() * sizeof(uint32_t), instanceCullingData.data(), BufferUsageFlags::Storage);

                // Camera uniform (with culling planes)
                CreateUniformBuffer(pVulkan, m_ShadowInstanceCullingCameraUniform);

                const auto* pComputeShader = m_ShaderManager->GetShader("RasterizedShadowCull");
                auto cullingMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pComputeShader, NUM_VULKAN_BUFFERS,
                    nullptr,
                    [this, &culledDrawIndirectBufferObject](const std::string& bufferName) -> tPerFrameVkBuffer {
                        if (bufferName == "CameraUniform")
                            return { m_ShadowInstanceCullingCameraUniform.vkBuffers };
                        else if (bufferName == "UnculledInstances")
                            return { m_ShadowInstanceCullingBuffer.buf.GetVkBuffer() };
                        else if (bufferName == "CulledInstances")
                            return { m_ShadowInstanceCulledData.GetVkBuffer() };
                        else if (bufferName == "UnculledIndirectDraw")
                            return { m_ShadowInstanceUnculledDrawIndirectBuffer->GetVkBuffer() };
                        else if (bufferName == "CulledIndirectDraw")
                            return { culledDrawIndirectBufferObject.GetVkBuffer() };
                        assert(0);
                        return {};
                    });

                m_ShadowRasterizedCullingComputable = std::make_unique<Computable>(*pVulkan, std::move(cullingMaterial));
                if (!m_ShadowRasterizedCullingComputable->Init())
                {
                    LOGE("Error Creating ShadowRasterizedCulling computable...");
                    m_ShadowRasterizedCullingComputable.reset();
                }
                else
                {
                    m_ShadowRasterizedCullingComputable->SetDispatchGroupCount(0, { 1,1,1 });   // reset
                    m_ShadowRasterizedCullingComputable->SetDispatchGroupCount(1, { 1, uint32_t(instances.size() + 63) / 64, 1 });   // cull instances
                    m_ShadowRasterizedCullingComputable->SetDispatchGroupCount(2, { 1, uint32_t(m_ShadowInstanceUnculledDrawIndirectBuffer->GetNumDraws() + 63) / 64, 1 });   // generate indirect
                }
            }
            else
            {
                // If not culling just use the unculled indirect buffer
                culledDrawIndirectBufferObject = std::move(*m_ShadowInstanceUnculledDrawIndirectBuffer);
                m_ShadowInstanceUnculledDrawIndirectBuffer.reset();
            }

            uint32_t shadowRenderPassBits = (1 << RP_RASTERSHADOW);// | (1 << RP_REFLECT);
            const char* renderPassNames[NUM_RENDER_PASSES]{};
            renderPassNames[RP_RASTERSHADOW] = "RP_RASTERSHADOW_CULLED";

            m_ShadowRasterizedDrawable = std::make_unique<Drawable>(*pVulkan, std::move(material));
            m_ShadowRasterizedDrawable->Init(m_RenderPass, renderPassNames, shadowRenderPassBits, std::move(mesh), std::move(vertexInstanceBuffer), std::move(culledDrawIndirectBufferObject));
        }
    }

    // Make drawables for each instanced mesh in the scene (and load the materials).  This is what we use for the gbuffer draw.
    if (!DrawableLoader::CreateDrawables(*pVulkan, std::move(instancedFatObjects), m_RenderPass, sRenderPassNames, modelMaterialLoader, m_SceneDrawables, {}, loaderFlags, {}))
    {
        LOGE("Error initializing Drawable: %s", gSceneObjectName);
        return false;
    }

    {
        MeshObject mesh;
        if (LoadGLTF("./Media/Meshes/Skybox_Separate.gltf", 0, &mesh))
        {
            const auto* pSkyboxShader = m_ShaderManager->GetShader("Skybox");
            assert(pSkyboxShader);
            auto material = m_MaterialManager->CreateMaterial(*pVulkan, *pSkyboxShader, NUM_VULKAN_BUFFERS,
                [this](const std::string& textureName) -> MaterialPass::tPerFrameTexInfo {
                    auto texture = m_TextureManager->GetTexture(textureName);
                    if (texture == nullptr)
                    {
                        assert(0);
                        return {};
                    }
                    return { texture };
                },
                [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    if (bufferName == "Vert") {
                        return { m_SkyboxVertUniform[RP_GBUFFER].vkBuffers };
                    }
                    else {
                        assert(0);
                        return {};
                    }
                });
            uint32_t skyboxRenderPassBits = (1 << RP_LIGHT);// | (1 << RP_REFLECT);
            m_SkyboxDrawable = std::make_unique<Drawable>(*pVulkan, std::move(material));
            m_SkyboxDrawable->Init(m_RenderPass, sRenderPassNames, skyboxRenderPassBits, std::move(mesh));
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitHdr()
//-----------------------------------------------------------------------------
{
    // Set the color profile
    VkHdrMetadataEXT AuthoringProfile = { VK_STRUCTURE_TYPE_HDR_METADATA_EXT };
    AuthoringProfile.displayPrimaryRed.x = 0.680f;
    AuthoringProfile.displayPrimaryRed.y = 0.320f;
    AuthoringProfile.displayPrimaryGreen.x = 0.265f;
    AuthoringProfile.displayPrimaryGreen.y = 0.690f;
    AuthoringProfile.displayPrimaryBlue.x = 0.150f;
    AuthoringProfile.displayPrimaryBlue.y = 0.060f;
    AuthoringProfile.whitePoint.x = 0.3127f;
    AuthoringProfile.whitePoint.y = 0.3290f;
    AuthoringProfile.maxLuminance = 80.0f;// 1000.f;
    AuthoringProfile.minLuminance = 0.001f;
    AuthoringProfile.maxContentLightLevel = 2000.f;
    AuthoringProfile.maxFrameAverageLightLevel = 1000.f;
    return GetVulkan()->SetSwapchainHrdMetadata(AuthoringProfile);
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    // Gui
    assert(m_RenderPass[RP_HUD] != VK_NULL_HANDLE);
    m_Gui = std::make_unique<GuiImguiGfx<Vulkan>>(*GetVulkan(), m_RenderPass[RP_HUD]);
    if (!m_Gui->Initialize(windowHandle, m_HudRT[0].m_Width, m_HudRT[0].m_Height))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
    if (gRenderHud && m_Gui)
    {
        // Update Gui
        m_Gui->Update();

        // Begin our window.
        static bool settingsOpen = true;
        if (gAdvancedMode)
        {
            ImGui::SetNextWindowSize(ImVec2((gRenderWidth * 3.0f) / 4.0f, 500.f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(gRenderWidth / 8.0f, gRenderHeight / 2.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Settings", &settingsOpen, (ImGuiWindowFlags)0))
            {
                if (ImGui::CollapsingHeader("Camera"))
                {
                    auto position = m_Camera.Position();
                    auto rotation = m_Camera.Rotation();
                    auto rotationDeg = glm::eulerAngles(rotation) * TO_DEGREES;
                    if (ImGui::InputFloat3("Position", &position.x, "%.1f"))
                    {
                        m_Camera.SetPosition(position, rotation);
                    }
                    if (ImGui::InputFloat3("Rotation", &rotationDeg.x, "%.1f"))
                    {
                        rotation = glm::quat(rotationDeg * TO_RADIANS);
                        m_Camera.SetPosition(position, rotation);
                    }
                    bool changed = ImGui::SliderFloat("Near Plane", &gNearPlane, 0.01f, gFarPlane - 0.01f);
                    changed |= ImGui::SliderFloat("Far Plane", &gFarPlane, gNearPlane + 0.02f, 15000.f);
                    if (changed)
                    {
                        m_Camera.SetClipPlanes(gNearPlane, gFarPlane);
                        m_Shadows[0].SetEyeClipPlanes(m_Camera.Fov(), m_Camera.NearClip(), m_Camera.Aspect(), gRasterizedShadowFarPlane);
                    }
                }

                if (!gRasterizedShadow)
                {
                    if (gCreateCulledAccelerationStructure)
                        ImGui::Checkbox("Disable RT Cull", &gDisableAccelerationStructureCull);
                    ImGui::Checkbox("Force RayTrace AS regen", &gForceAccelerationStructureRegen);
                    ImGui::Checkbox("Main RQ shadow use fragment shader", &gRayQueryFragmentShader);
                    ImGui::Checkbox("Main RQ shadow Directional light", &gShadowDirectionalLight);
                    ImGui::Checkbox("Enable additional shadows/lights (frag)", &gAdditionalShadows);
                    ImGui::Checkbox("Enable RT BLAS vertex animation", &gAnimateBLAS);
                    ImGui::SliderFloat3("Light Pos", &gShadowPosition.x, -100.0f, 100.0f);
                    ImGui::SliderFloat("Shadow Cull Radius", &gShadowRadius, 1.0f, 2000.0f);
                    ImGui::SliderFloat("Shadow Radius Cutoff", &gShadowRadiusCutoff, 0.01f, 1.0f);
                    ImGui::SliderFloat("Light Emitter Radius", &gLightEmitterRadius, 1.0f, 1000.0f);
                    ImGui::SliderFloat("Light Cutoff", &gLightAttenuationCutoff, 0.01f, 1.0f);

                    m_LightFragUniformData.PointLightRadius = gLightEmitterRadius;
                    m_LightFragUniformData.PointLightCutoff = gLightAttenuationCutoff;

                    float pointLightIntensity = m_LightFragUniformData.PointLightPosition.w;
                    ImGui::SliderFloat("Point Intensity", &pointLightIntensity, 0.0f, 300.0f);
                    m_LightFragUniformData.PointLightPosition.w = pointLightIntensity;
                    for (auto& d : m_AdditionalDeferredLightsData)
                        d.Intensity = pointLightIntensity;
                }
                else
                {
                    if (ImGui::CollapsingHeader("Raster Shadow"))
                    {
                        bool changed = ImGui::InputFloat3("Shadow Position", &gRasterizedShadowPosition.x, "%.1f");
                        changed |= ImGui::InputFloat3("Shadow Target", &gRasterizedShadowTarget.x, "%.1f");
                        if (changed)
                        {
                            m_Shadows[0].SetLightPos(gRasterizedShadowPosition, gRasterizedShadowTarget);
                        }
                        if (ImGui::SliderFloat("Shadow FarPlane", &gRasterizedShadowFarPlane, 0.1f, gFarPlane))
                        {
                            m_Shadows[0].SetEyeClipPlanes(m_Camera.Fov(), m_Camera.NearClip(), m_Camera.Aspect(), gRasterizedShadowFarPlane);
                        }
                    }
                }

                ImGui::SliderFloat("SpecScale", &m_LightFragUniformData.SpecScale, 0.0f, 8.0f);
                ImGui::SliderFloat("SpecPower", &m_LightFragUniformData.SpecPower, 1.0f, 100.0f);
                ImGui::SliderFloat("irradianceAmount", &m_LightFragUniformData.irradianceAmount, 0.0f, 2.0f);
                ImGui::SliderFloat("irradianceMip", &m_LightFragUniformData.irradianceMipLevel, 0.0f, 8.0f);
                ImGui::SliderFloat("Brightness", &m_BlitFragUniformData.Diffuse, 0.0f, 10.0f);

            }
            ImGui::End();
        }

        if (ImGui::Begin("FPS", (bool*)nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("FPS: %.1f", m_CurrentFPS);

            if (gAdvancedMode)
            {
                if (ImGui::TreeNode("Stats"))
                {
                    if (gRasterizedShadow)
                    {
                        ImGui::Text("Rasterized Shadows");
                    }
                    else
                    {
                        char InstancesString[256];
                        int offset = snprintf(InstancesString, sizeof(InstancesString), "RT Instances: %u", (uint32_t)(m_sceneRayTracableCulled ? m_sceneRayTracableCulled->GetNumInstances() : m_sceneRayTracable->GetNumInstances()));
                        for (const auto& rt : m_AdditionalDeferredLightShadowRQ)
                        {
                            if (offset < sizeof(InstancesString))
                                offset += snprintf(InstancesString + offset, sizeof(InstancesString) - offset, ",%u", (uint32_t)rt.GetNumInstances());
                        }
                        ImGui::Text("%s", InstancesString);

                        size_t totalTriangles = 0;
                        if (m_sceneRayTracableCulled)
                        {
                            for (const auto& instance : m_sceneRayTracableCulled->GetInstances())
                                totalTriangles += m_sceneObjectTriangleCounts[instance.first];
                        }
                        else
                        {
                            for (const auto& instance : m_sceneRayTracable->GetInstances())
                                totalTriangles += m_sceneObjectTriangleCounts[instance.first];
                        }
                        offset = snprintf(InstancesString, sizeof(InstancesString), "RT Triangles: %u", (uint32_t)totalTriangles);
                        ImGui::Text("%s", InstancesString);

                        uint32_t updatedTriangles = 0;
                        for (const auto& animatedObject : m_sceneAnimatedMeshObjects)
                            updatedTriangles += animatedObject.GetPrimitiveCount();

                        if (gAnimateBLAS)
                            ImGui::Text("RT BLAS Updates: %u (%u triangles)", (uint32_t)m_sceneAnimatedMeshObjects.size(), updatedTriangles);
                    }
                    ImGui::Text("%s", sm_BuildTimestamp);
                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();

        return;
    }
}

//-----------------------------------------------------------------------------
void Application::UpdateCamera(float ElapsedTime)
//-----------------------------------------------------------------------------
{
    m_Camera.UpdateController(ElapsedTime, *m_CameraController);
    m_Camera.UpdateMatrices();
}

//-----------------------------------------------------------------------------
void Application::UpdateLighting(float ElapsedTime)
//-----------------------------------------------------------------------------
{
    if( m_sceneRayTracable )
    {
        if( m_sceneRayTracableCulled )
        {
            // Calculate the light/shadow's radius (based on the light cutoff, radius, and intensity)
            // gShadowRadius = gLightEmitterRadius * (sqrtf( m_LightFragUniformData.PointLightPosition.w / gLightAttenuationCutoff ) - 1.0f);
            // Cull the 'main' shadow ray tracable scene based on the radius.
            UpdateSceneRTCulled( *m_sceneRayTracableCulled, gShadowPosition, gShadowRadius * gShadowRadiusCutoff );
        }

        if(gAdditionalShadows)
        {
            for( uint32_t WhichLight = 0; WhichLight < m_AdditionalDeferredLightShadowRQ.size(); ++WhichLight )
            {
                auto& sceneRT = m_AdditionalDeferredLightShadowRQ[WhichLight];
                UpdateSceneRTCulled( sceneRT, glm::vec3( m_AdditionalDeferredLightsData[WhichLight].Position ), m_AdditionalDeferredLightsData[WhichLight].Radius * gShadowRadiusCutoff);
            }
        }
    }
}

//-----------------------------------------------------------------------------
void Application::UpdateShadowMap(float ElapsedTime)
//-----------------------------------------------------------------------------
{
    m_Shadows[0].Update(m_Camera.ViewMatrix());
}

//-----------------------------------------------------------------------------
void Application::UpdateSceneRTCulled( SceneRTCulled& sceneRTCulled, glm::vec3 lightPosition, float lightRadius )
//-----------------------------------------------------------------------------
{
    BBoxTest lightBoundingBox( lightPosition, glm::vec3( lightRadius, lightRadius, lightRadius ) );
    SphereTest lightSphere( lightPosition, lightRadius );
    ViewFrustum viewFrustum( m_Camera.ProjectionMatrix(), m_Camera.ViewMatrix() );
    FrustumTest viewFrustumTest( viewFrustum );

    sceneRTCulled.SetForceRegenerateAccelerationStructure( gForceAccelerationStructureRegen );

    if( gDisableAccelerationStructureCull )
    {
        // Update the ray tracing top level Acceleration Structure without culling (add everything to the AS).
        sceneRTCulled.Update( *m_sceneRayTracable, [&lightBoundingBox, &viewFrustumTest]( const glm::vec4& center, const glm::vec4& halfSize ) {
            return OctreeBase::eQueryResult::Inside;
        } );
    }
    else
    {
        auto lightInViewspace = viewFrustumTest( lightBoundingBox.m_boxCenter, lightBoundingBox.m_boxHalfSize );
        switch( lightInViewspace ) {
        case OctreeBase::eQueryResult::Inside:
            //
            // Light area completely in view space - consider all objects in the light area (any other objects in view frustum won't be lit by this).
            //
            sceneRTCulled.Update( *m_sceneRayTracable, lightSphere );
            break;
        case OctreeBase::eQueryResult::Outside:
            //
            // Light area completely outside view - nothing we can see should be lit by this light (so no shadow occluders).
            // Lambda (should) compile out to remove the Octree test entirely.
            //
            sceneRTCulled.Update( *m_sceneRayTracable, []( const glm::vec4&, const glm::vec4& ) { return OctreeBase::eQueryResult::Outside; } );
            break;
        case OctreeBase::eQueryResult::Partial:
            if( viewFrustumTest( lightBoundingBox.m_boxCenter ) == OctreeBase::eQueryResult::Inside )
            {
                //
                // Light center (emission point) is inside the view frustum, cull against both the light area and the view frustum.
                //
                sceneRTCulled.Update( *m_sceneRayTracable, [&lightSphere, &viewFrustumTest]( const glm::vec4& center, const glm::vec4& halfSize ) {
                    // Lambda for the combined BoundingBox and View Frustum test.  Called by the update to determine visibility of ray traced objects.
                    auto bbResult = lightSphere( center, halfSize );
                    if( bbResult == OctreeBase::eQueryResult::Outside )
                        return OctreeBase::eQueryResult::Outside;
                    else
                    {
                        auto frustumResult = viewFrustumTest( center, halfSize );
                        if( frustumResult == OctreeBase::eQueryResult::Outside )
                            return OctreeBase::eQueryResult::Outside;
                        else if( frustumResult == OctreeBase::eQueryResult::Partial || bbResult == OctreeBase::eQueryResult::Partial )
                            return OctreeBase::eQueryResult::Partial;
                    }
                    return OctreeBase::eQueryResult::Inside;
                } );
            }
            else
            {
                //
                // Light center is outside the view frustum but light volume is partially inside the frustum.
                // Ideally we would cull against the area containing both, and the area between that and the center of the light area.
                // would reduce the required light area by at least half, sometimes much more.
                // For now just use the entire light volume and be done!
                sceneRTCulled.Update( *m_sceneRayTracable, lightBoundingBox );
            }
            break;
        }
    }
}

//-----------------------------------------------------------------------------
void Application::LogFps()
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::LogFps();
    if (m_GpuTimerPool)
    {
        std::scoped_lock timerPoolLock(m_GpuTimerPoolMutex);

        if (gAdvancedMode)
            m_GpuTimerPool->Log2(m_GpuTimerPool->GetResults());
        // Reset the GPU timers so average is only between 'LogFps' calls
        m_GpuTimerPool->ResetTimers(-1/*dont want to skip in-flight timers*/);
    }
}

//-----------------------------------------------------------------------------
void Application::UpdateProfiling(uint32_t WhichFrame)
//-----------------------------------------------------------------------------
{
    if (m_GpuTimerPool)
    {
        m_GpuTimerPool->UpdateResults(WhichFrame);
    }
}

//-----------------------------------------------------------------------------
bool Application::ChangeSurfaceFormat(SurfaceFormat newSurfaceFormat)
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    if (!pVulkan->ChangeSurfaceFormat(newSurfaceFormat))
    {
        return false;
    }

    // We need to modify the blit render pass (the only one that touches the output framebuffer).
    // RenderPass needs to be compatible with the framebuffer's format.
    // m_PassCmdBuffer[RP_BLIT] is good, gets rebuilt every frame
 
    vkDestroyRenderPass(pVulkan->m_VulkanDevice, m_RenderPass[RP_BLIT], nullptr);
    m_RenderPass[RP_BLIT] = VK_NULL_HANDLE;

    auto& PassSetup = m_PassSetup[RP_BLIT];
    PassSetup.ColorFormats = { pVulkan->m_SurfaceFormat };
    PassSetup.DepthFormat = { pVulkan->m_SwapchainDepth.format };

    if (!pVulkan->CreateRenderPass({ PassSetup.ColorFormats },
        PassSetup.DepthFormat,
        VK_SAMPLE_COUNT_1_BIT,
        PassSetup.ColorInputUsage,
        PassSetup.ColorOutputUsage,
        PassSetup.ClearDepthRenderPass,
        PassSetup.DepthOutputUsage,
        &m_RenderPass[RP_BLIT]))
        return false;

    if (!m_BlitDrawable->ReInit( m_RenderPass[RP_BLIT], sRenderPassNames[RP_BLIT], nullptr, nullptr))
    {
        return false;
    }

    InitHdr();

    return true;
}

//-----------------------------------------------------------------------------
Application::RenderPassData& Application::BeginRenderPass(RENDER_PASS WhichPass, uint32_t WhichBuffer, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores, VkSubpassContents SubpassContents)
//-----------------------------------------------------------------------------
{
    return BeginRenderPass(BeginCommandBuffer(WhichPass, WhichBuffer, WaitSemaphores, WaitDstStageMasks, SignalSemaphores, SubpassContents));
}

//-----------------------------------------------------------------------------
Application::RenderPassData& Application::BeginCommandBuffer(RENDER_PASS WhichPass, uint32_t WhichBuffer, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores, VkSubpassContents SubpassContents)
//-----------------------------------------------------------------------------
{
    assert( WaitSemaphores.size() == WaitDstStageMasks.size() );

    RenderPassData& renderPassData = m_RenderPassSubmitData[WhichBuffer].emplace_back( RenderPassData {
        m_PassCmdBuffer[WhichBuffer][WhichPass].m_Name,
        WhichBuffer,
        WhichPass,
        m_PassCmdBuffer[WhichBuffer][WhichPass],
        SubpassContents,
        {WaitSemaphores.begin(), WaitSemaphores.end()},
        {WaitDstStageMasks.begin(), WaitDstStageMasks.end()},
        {SignalSemaphores.begin(), SignalSemaphores.end()},
        } );

    const auto& passSetup = m_PassSetup[WhichPass];

    // Reset the primary command buffer...
    if (!renderPassData.CmdBuff.Reset())
    {
        assert(0);
    }

    // ... begin the primary command buffer ...
    if (!renderPassData.CmdBuff.Begin())
    {
        assert(0);
    }

    return renderPassData;
}

//-----------------------------------------------------------------------------
Application::RenderPassData& Application::BeginRenderPass(Application::RenderPassData& renderPassData)
//-----------------------------------------------------------------------------
{
    BeginRenderPass(renderPassData.CmdBuff, renderPassData.WhichPass, renderPassData.WhichBuffer, renderPassData.SubpassContents);

    return renderPassData;
}

//-----------------------------------------------------------------------------
void Application::BeginRenderPass(Wrap_VkCommandBuffer& CmdBuf, RENDER_PASS WhichPass, uint32_t WhichBuffer, VkSubpassContents SubpassContents)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    if( m_RenderPass[WhichPass] != VK_NULL_HANDLE )
    {
        const auto& passSetup = m_PassSetup[WhichPass];

        VkFramebuffer Framebuffer = VK_NULL_HANDLE;
        switch( WhichPass )
        {
        case RP_GBUFFER:
            Framebuffer = m_GBufferRT[0].m_FrameBuffer;
            break;
        case RP_RASTERSHADOW:
            Framebuffer = m_Shadows[0].GetFramebuffer();
            break;
        case RP_RAYSHADOW:
            Framebuffer = m_ShadowRT[0].m_FrameBuffer;
            break;
        case RP_LIGHT:
            Framebuffer = m_MainRT[0].m_FrameBuffer;
            break;
        case RP_HUD:
            Framebuffer = m_HudRT[0].m_FrameBuffer;
            break;
        case RP_BLIT:
            Framebuffer = pVulkan->m_SwapchainBuffers[WhichBuffer].framebuffer;
            break;
        default:
            assert( 0 );
        }

        VkRect2D PassArea = {};
        PassArea.offset.x = 0;
        PassArea.offset.y = 0;
        PassArea.extent.width = passSetup.TargetWidth;
        PassArea.extent.height = passSetup.TargetHeight;

        VkClearColorValue clearColor[1] {{ passSetup.ClearColor[0], passSetup.ClearColor[1], passSetup.ClearColor[2], passSetup.ClearColor[3] }};
        bool IsSwapChainRenderPass = WhichPass == RP_BLIT;

        CmdBuf.BeginRenderPass( PassArea, 0.0f, 1.0f, clearColor, (uint32_t) passSetup.ColorFormats.size(), passSetup.DepthFormat != TextureFormat::UNDEFINED, m_RenderPass[WhichPass], IsSwapChainRenderPass, Framebuffer, SubpassContents );
    }
}

//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffers(const RenderPassData& renderPassData)
//-----------------------------------------------------------------------------
{
    std::vector<VkCommandBuffer> SubCommandBuffers;
    SubCommandBuffers.reserve(3);

    // Add sub commands to render list

    const uint32_t WhichBuffer = renderPassData.WhichBuffer;
    const uint32_t WhichPass = renderPassData.WhichPass;

    if (WhichPass == RP_LIGHT)
    {
        // Do the light commands
        SubCommandBuffers.push_back(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer);
        m_TotalDrawCalls += m_LightCmdBuffer[WhichBuffer].m_NumDrawCalls;
        m_TotalTriangles += m_LightCmdBuffer[WhichBuffer].m_NumTriangles;

        // 'Additional' lights go after the LightCmd buffer (assuming they are enabled)
        if(gAdditionalShadows)
        {
            SubCommandBuffers.push_back(m_AdditionalLightsCmdBuffer[WhichBuffer].m_VkCommandBuffer);
            m_TotalDrawCalls += m_AdditionalLightsCmdBuffer[WhichBuffer].m_NumDrawCalls;
            m_TotalTriangles += m_AdditionalLightsCmdBuffer[WhichBuffer].m_NumTriangles;
        }
    }

    if (m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_NumDrawCalls)
    {
        SubCommandBuffers.push_back(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer);
        m_TotalDrawCalls += m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_NumDrawCalls;
        m_TotalTriangles += m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_NumTriangles;
    }

    // Add all subcommands
    AddPassCommandBuffers(renderPassData, SubCommandBuffers);
}

//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffers(const RenderPassData& RenderPassData, std::span<VkCommandBuffer> SubCommandBuffers)
//-----------------------------------------------------------------------------
{
    // Make sure there is something to execue
    if (SubCommandBuffers.size() == 0)
    {
        // Technically, this may not be an error.  For now, let it fall through and submit nothing
        LOGE("Error! Being asked to add pass command buffers but nothing is in the list!");
    }
    else
    {
        vkCmdExecuteCommands(RenderPassData.CmdBuff.m_VkCommandBuffer, (uint32_t)SubCommandBuffers.size(), SubCommandBuffers.data());
    }
}

//-----------------------------------------------------------------------------
void Application::EndRenderPass(const RenderPassData& RenderPassData)
//-----------------------------------------------------------------------------
{
    // End the render pass.
    if( m_RenderPass[RenderPassData.WhichPass] != VK_NULL_HANDLE )
    {
        RenderPassData.CmdBuff.EndRenderPass();
    }
}

//-----------------------------------------------------------------------------
std::span<VkSemaphore> Application::SubmitGameThreadWork(uint32_t WhichBuffer, VkFence CompletionFence)
//-----------------------------------------------------------------------------
{
    const auto& SubmitList = m_RenderPassSubmitData[WhichBuffer];
    uint32_t NumCmdBuffers = (uint32_t)SubmitList.size();

#ifdef ENABLE_PROFILING
    PROFILE_ENTER(GROUP_VKFRAMEWORK, 0, "SubmitGameThreadWork: %d Buffers", NumCmdBuffers);
#endif // ENABLE_PROFILING

    const uint32_t NumPasses = (uint32_t) SubmitList.size();

    for(uint32_t WhichPass = 0; WhichPass < NumPasses; ++WhichPass)
    {
        auto& OneSubmit = SubmitList[WhichPass];
        bool LastPass = (WhichPass == NumPasses - 1);        

#ifdef ENABLE_PROFILING
        PROFILE_ENTER(GROUP_VKFRAMEWORK, 0, "Submit Cmd Buffer: %s", OneSubmit.Desc.c_str());
#endif // ENABLE_PROFILING

        OneSubmit.CmdBuff.QueueSubmitT( OneSubmit.WaitSemaphores, OneSubmit.WaitDstStageMasks, OneSubmit.SignalSemaphores, LastPass ? CompletionFence : VK_NULL_HANDLE );

#ifdef ENABLE_PROFILING
        PROFILE_EXIT(GROUP_VKFRAMEWORK);    // "Submit Cmd Buffer: %s"
#endif // ENABLE_PROFILING
    }

#ifdef ENABLE_PROFILING
    PROFILE_EXIT(GROUP_VKFRAMEWORK);    // "SubmitGameThreadWork: %d Buffers"
#endif // ENABLE_PROFILING

    return !m_RenderPassSubmitData[WhichBuffer].empty() ? std::span<VkSemaphore>{m_RenderPassSubmitData[WhichBuffer].rbegin()->SignalSemaphores} : std::span<VkSemaphore>{};
}
