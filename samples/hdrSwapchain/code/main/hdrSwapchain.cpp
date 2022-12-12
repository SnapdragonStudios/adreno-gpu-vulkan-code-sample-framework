//===========================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//===========================================================================================

#include "hdrSwapchain.hpp"
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
#include "memory/bufferObject.hpp"
#include "memory/indexBufferObject.hpp"
#include "memory/vertexBufferObject.hpp"
#include "system/math_common.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include <glm/gtc/quaternion.hpp>
#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <filesystem>

namespace
{
    // Global Variables From Config File
    bool    gRenderShadows = true;
    bool    gRenderHud = true;
    bool    gAsyncComputeVSM = true;
    bool    gAsyncComputeNNAO = true;

    glm::vec4 gClearColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

    glm::vec3 gCameraStartPos = glm::vec3(-58.0, 5.55f, -35.7f);
    glm::vec3 gCameraStartRot = glm::vec3(10.0f, -70.0f, 0.0f);
    glm::vec3 gShadowLightPos = glm::vec3(111.0f, 420.0f, -423.0f);
    glm::vec3 gShadowLightTarget = glm::vec3(269.0f, 0.0f, -254.f);

    float   gFOV = PI_DIV_4;
    float   gNearPlane = 1.0f;
    float   gFarPlane = 1800.0f;
    float   gFarPlaneShadows = 500.0f;// furthest distance we want to render shadows FROM THE EYE camera's perspective

    float       gSpecularExponent = 128.0f;
    glm::vec4   gModelIrradianceParams = glm::vec4(1.0f, 0.05f, 0.0f, 0.0f);
    glm::vec4   gModelReflectParams = glm::vec4(1.0f, 0.0f/*env reflect*/, 0.0f, 1.0f);
    float       gMirrorAmount = 0.1f;
    float       gNNAOAmount = 1.0f;
    float       gNormalAmount = 2.0f;
    float       gNormalMirrorReflectAmount = 0.05f;

    static const char* const sRenderPassNames[NUM_RENDER_PASSES] = { "RP_GBUFFER", "RP_SHADOW", "RP_LIGHT", "RP_HUD", "RP_BLIT" };
    static_assert(RP_GBUFFER == 0, "Check order of sRenderPassNames");
    static_assert(RP_BLIT == 4, "Check order of sRenderPassNames");
    static_assert(NUM_RENDER_PASSES == sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0]), "mismatched sRenderPassNames");

    const char* gMuseumAssetsPath = "Media\\Meshes";
    const char* gTextureFolder = "Media\\Textures\\";
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

Application::Application()
    : ApplicationHelperBase()
    , m_TexWhite{}
    , m_DefaultNormal{}
{
    // Camera
    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    // The Object
    m_ObjectScale = 1.00f;
    m_ObjectWorldPos = glm::vec3(0.0f, -0.5f, 0.0f); 

    // Pass Semaphores
    m_PassCompleteSemaphore.fill(VK_NULL_HANDLE);

    // Render passes
    m_RenderPass.fill(VK_NULL_HANDLE);

    // Compute
    m_VsmAsyncComputeCanStartSemaphore = VK_NULL_HANDLE;
    m_VsmAsyncComputeCompleteSemaphore = VK_NULL_HANDLE;
    m_BloomAsyncComputeCanStartSemaphore = VK_NULL_HANDLE;
    m_BloomAsyncComputeCompleteSemaphore = VK_NULL_HANDLE;
    m_NNAOAsyncComputeCanStartSemaphore = VK_NULL_HANDLE;
    m_NNAOAsyncComputeCompleteSemaphore = VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
Application::~Application()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    // Pass Semaphores
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        if (m_PassCompleteSemaphore[WhichPass] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(pVulkan->m_VulkanDevice, m_PassCompleteSemaphore[WhichPass], NULL);
        }
        m_PassCompleteSemaphore[WhichPass] = VK_NULL_HANDLE;
    }

    // Compute
    vkDestroySemaphore(pVulkan->m_VulkanDevice, m_NNAOAsyncComputeCompleteSemaphore, NULL);
    m_NNAOAsyncComputeCompleteSemaphore = VK_NULL_HANDLE;
    vkDestroySemaphore(pVulkan->m_VulkanDevice, m_NNAOAsyncComputeCanStartSemaphore, NULL);
    m_NNAOAsyncComputeCanStartSemaphore = VK_NULL_HANDLE;
    vkDestroySemaphore(pVulkan->m_VulkanDevice, m_BloomAsyncComputeCompleteSemaphore, NULL);
    m_BloomAsyncComputeCompleteSemaphore = VK_NULL_HANDLE;
    vkDestroySemaphore(pVulkan->m_VulkanDevice, m_BloomAsyncComputeCanStartSemaphore, NULL);
    m_BloomAsyncComputeCanStartSemaphore = VK_NULL_HANDLE;
    vkDestroySemaphore(pVulkan->m_VulkanDevice, m_VsmAsyncComputeCompleteSemaphore, NULL);
    m_VsmAsyncComputeCompleteSemaphore = VK_NULL_HANDLE;
    vkDestroySemaphore(pVulkan->m_VulkanDevice, m_VsmAsyncComputeCanStartSemaphore, NULL);
    m_VsmAsyncComputeCanStartSemaphore = VK_NULL_HANDLE;

    // Textures
    for (auto& texture : m_loadedTextures)
    {
        ReleaseTexture(m_vulkan.get(), &texture.second);
    }
    ReleaseTexture(pVulkan, &m_TexWhite);
    ReleaseTexture(pVulkan, &m_DefaultNormal);

    ReleaseTexture(pVulkan, &m_ComputeIntermediateHalfTarget);
    ReleaseTexture(pVulkan, &m_ComputeIntermediateHalf2Target);
    ReleaseTexture(pVulkan, &m_ComputeIntermediateQuarterTarget);
    ReleaseTexture(pVulkan, &m_ComputeIntermediateQuarter2Target);
    ReleaseTexture(pVulkan, &m_ComputeRenderTarget);
    ReleaseTexture(pVulkan, &m_BloomRenderTarget);
    ReleaseTexture(pVulkan, &m_NNAORenderTarget);
    ReleaseTexture(pVulkan, &m_NNAOTempTarget);
    ReleaseTexture(pVulkan, &m_VsmTarget);
}

//-----------------------------------------------------------------------------
int Application::PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR> formats)
//-----------------------------------------------------------------------------
{
    // On Snapdragon if the surfaceflinger has to do the rotation to the display native orientation then it will do it at 8bit colordepth.
    // To avoid this we need to enable the 'pre-rotation' of the display (and the use of VK_QCOM_render_pass_transform so we dont have to rotate our buffers/passes manually).
    m_vulkan->m_UseRenderPassTransform = true;

    // We want to select a SRGB output format (if one exists)
    int index = 0;
    for (const auto& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            return index;
        ++index;
    }
    return -1;
}

//-----------------------------------------------------------------------------
bool Application::Initialize(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize(windowHandle))
    {
        return false;
    }

    if (!ApplicationHelperBase::InitCamera())
    {
        return false;
    }

    // Set the current surface format
    m_RequestedSurfaceFormat = {m_vulkan->m_SurfaceFormat, m_vulkan->m_SurfaceColorSpace};
    m_bEncodeSRGB = strstr(Vulkan::VulkanFormatString(m_vulkan->m_SurfaceFormat), "SRGB") == nullptr;   // if we have an srgb buffer then the output doesnt need to be encoded (hardware will do it for us)

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

    LOGE("Initalize lighting!!");

    return true;
}

//-----------------------------------------------------------------------------
VulkanTexInfo* Application::GetOrLoadTexture(const char* textureName)
//-----------------------------------------------------------------------------
{
    if (textureName == nullptr || textureName[0] == 0)
    {
        return nullptr;
    }

    auto texturePath = std::filesystem::path(textureName);
    if (!texturePath.has_filename())
    {
        return nullptr;
    }

    auto textureFilename = texturePath.stem();

    auto iter = m_loadedTextures.find(textureFilename.string());
    if (iter != m_loadedTextures.end())
    {
        return &iter->second;
    }

    // Prepare the texture path
    std::string textureInternalPath = gTextureFolder;
    textureInternalPath.append(textureFilename.string());
    textureInternalPath.append(".ktx");

    auto loadedTexture = LoadKTXTexture(m_vulkan.get(), *m_AssetManager, textureInternalPath.c_str());
    if (!loadedTexture.IsEmpty())
    {
        m_loadedTextures.insert({ textureFilename.string() , std::move(loadedTexture) });
        VulkanTexInfo* texInfo = &m_loadedTextures[textureFilename.string()];
        return texInfo;
    }

    return nullptr;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    m_VsmTarget = CreateTextureObject(m_vulkan.get(), gShadowMapWidth, gShadowMapHeight, VK_FORMAT_R32G32_SFLOAT, TEXTURE_TYPE::TT_COMPUTE_TARGET, "BloomDest");
    m_ComputeIntermediateHalfTarget = CreateTextureObject(m_vulkan.get(), gRenderWidth / 2, gRenderHeight / 2, VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "IntermediateHalf");
    m_ComputeIntermediateHalf2Target = CreateTextureObject(m_vulkan.get(), gRenderWidth / 2, gRenderHeight / 2, VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "IntermediateHalf2");
    m_ComputeIntermediateQuarterTarget = CreateTextureObject(m_vulkan.get(), gRenderWidth / 4, gRenderHeight / 4, VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "IntermediateQuarter");
    m_ComputeIntermediateQuarter2Target = CreateTextureObject(m_vulkan.get(), gRenderWidth / 4, gRenderHeight / 4, VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "IntermediateQuarter2");
    m_ComputeRenderTarget = CreateTextureObject(m_vulkan.get(), gRenderWidth / 2, gRenderHeight / 2, VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "ComputeDest");
    m_BloomRenderTarget = CreateTextureObject(m_vulkan.get(), gRenderWidth / 2, gRenderHeight / 2, VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "BloomDest");
    m_NNAORenderTarget = CreateTextureObject(m_vulkan.get(), gRenderWidth / 2, gRenderHeight / 2, VK_FORMAT_R8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "NNAOOutput");
    m_NNAOTempTarget = CreateTextureObject(m_vulkan.get(), gRenderWidth / 2, gRenderHeight / 2, VK_FORMAT_R8_UNORM, TEXTURE_TYPE::TT_COMPUTE_TARGET, "NNAOTemp");

    if (gRenderShadows)
    {
        if (m_VsmTarget.Height != 1024 || m_VsmTarget.Width != 1024)
        {
            LOGE("VSM Compute shader is (currently) hardcoded to 1024x1024 shadow maps...");
        }
        else
        {
            const auto* pComputeShader = m_shaderManager->GetShader("VarianceShadowMap");
            assert(pComputeShader);

            auto material = m_MaterialManager->CreateMaterial(*m_vulkan.get(), *pComputeShader, NUM_VULKAN_BUFFERS,
                [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
                    if (texName == "ShadowDepth")
                        return { &m_Shadows[0].GetDepthTexture(0) };
                    else if (texName == "VarianceShadowMap")
                        return { &m_VsmTarget };
                    assert(0);
                    return {};
                },
                [this](const std::string& bufferName) -> MaterialPass::tPerFrameVkBuffer {
                    return { m_ComputeCtrlUniform.buf.GetVkBuffer() };
                });

            m_VsmComputable = std::make_unique<Computable>(*m_vulkan.get(), std::move(material));
            if (!m_VsmComputable->Init())
            {
                LOGE("Error Creating VSM computable...");
                m_VsmComputable.reset();
            }
            else
            {
                m_VsmComputable->SetDispatchGroupCount(0, { 1, m_VsmTarget.Height,1 });
                m_VsmComputable->SetDispatchGroupCount(1, { m_VsmTarget.Width, 1, 1 });
            }
        }
    }

    const auto* pComputeShader = m_shaderManager->GetShader("NNAO");
    assert(pComputeShader);

    auto material = m_MaterialManager->CreateMaterial(*m_vulkan.get(), *pComputeShader, NUM_VULKAN_BUFFERS,
        [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo 
        {
            if (texName == "NNAOout")
                return { &m_NNAORenderTarget };
            if (texName == "NNAOtmp")
                return { &m_NNAOTempTarget };
            else if (texName == "Depth")
                return { &m_GBufferRT[0].m_DepthAttachment };
            else if (texName == "Normal")
                return { &m_GBufferRT[0].m_ColorAttachments[1] };
            else {

                auto* texture = GetOrLoadTexture(texName.c_str());
                if (texture)
                {
                    return { texture };
                }

                // File not loaded and not found in already loaded list, since these are Neural Network weights can't just wing it!
                assert(0);
                return {};
            }
        },
        [this](const std::string& bufferName) -> MaterialPass::tPerFrameVkBuffer {
            return { m_NNAOCtrlUniform[0].buf.GetVkBuffer() };
        });

    m_NNAOComputable = std::make_unique<Computable>(*m_vulkan.get(), std::move(material));
    if (!m_NNAOComputable->Init())
    {
        LOGE("Error Creating VSM computable...");
        m_NNAOComputable.reset();
    }
    else
    {
        m_NNAOComputable->SetDispatchGroupCount(0, { (m_NNAORenderTarget.Width + 31) / 32, (m_NNAORenderTarget.Height + 31) / 32,1 });
        m_NNAOComputable->SetDispatchGroupCount(1, { (m_NNAORenderTarget.Width + 63) / 64, m_NNAORenderTarget.Height,1 });
        m_NNAOComputable->SetDispatchGroupCount(2, { m_NNAORenderTarget.Width, (m_NNAORenderTarget.Height + 63) / 64,1 });
    }
    
    LOGI("Creating Light mesh...");
    glm::vec4 PosLLRadius = glm::vec4(-1.0f, -1.0f, 2.0f, 2.0f);
    glm::vec4 UVLLRadius = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    MeshObject lightMesh;
    MeshObject::CreateScreenSpaceMesh(m_vulkan.get(), PosLLRadius, UVLLRadius, 0, &lightMesh);

    const auto* pLightShader = m_shaderManager->GetShader("Light");
    assert(pLightShader);
    auto lightShaderMaterial = m_MaterialManager->CreateMaterial(*m_vulkan.get(), *pLightShader, NUM_VULKAN_BUFFERS,
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
            else if (texName == "AO") {
                return { &m_NNAORenderTarget };
            }
            else if (texName == "ShadowVarianceDepth") {
                return { &m_VsmTarget };
            }
            else if (texName == "ShadowDepth") {
                return { &m_Shadows[0].GetDepthTexture(0) };
            }
            assert(0);
            return {};
        },
        [this](const std::string& bufferName) -> MaterialPass::tPerFrameVkBuffer {
            //BlitFragCB
            return { m_LightFragUniform[0].buf.GetVkBuffer() };
        }
        );

    m_LightDrawable = std::make_unique<Drawable>(*m_vulkan.get(), std::move(lightShaderMaterial));
    if (!m_LightDrawable->Init( m_RenderPass[RP_LIGHT], sRenderPassNames[RP_LIGHT], std::move(lightMesh)))
    {
        LOGE("Error Creating Light drawable...");
    }

    LOGI("Creating Blit mesh...");

    MeshObject blitMesh;
    MeshObject::CreateScreenSpaceMesh(m_vulkan.get(), PosLLRadius, UVLLRadius, 0, &blitMesh);

    const auto* pBlitShader = m_shaderManager->GetShader("Blit");
    assert(pBlitShader);
    auto blitShaderMaterial = m_MaterialManager->CreateMaterial(*m_vulkan.get(), *pBlitShader, NUM_VULKAN_BUFFERS,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo {
            if (texName == "Diffuse") {
                return { &m_MainRT[0].m_ColorAttachments[0] };
            }
            else if (texName == "Bloom") {
                return { &m_BloomRenderTarget };
            }
            else if (texName == "Overlay") {
                return { &m_HudRT[0].m_ColorAttachments[0] };
            }
            assert(0);
            return {};
        },
        [this](const std::string& bufferName) -> MaterialPass::tPerFrameVkBuffer {
            //BlitFragCB
            return { m_BlitFragUniform[0].buf.GetVkBuffer() };
        }
        );

    m_BlitDrawable = std::make_unique<Drawable>(*m_vulkan.get(), std::move(blitShaderMaterial));
    if (!m_BlitDrawable->Init( m_RenderPass[RP_BLIT], sRenderPassNames[RP_BLIT], std::move(blitMesh)))
    {
        LOGE("Error Creating Blit drawable...");
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    // Meshes
    m_LightDrawable.reset();
    m_BlitDrawable.reset();
    m_ComputableTest.reset();
    m_VsmComputable.reset();
    m_BloomComputable.reset();
    m_SceneObject.clear();

    // Shaders
    m_shaderManager.reset();

    // Uniform Buffers
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
        {
            ReleaseUniformBuffer(pVulkan, &m_ObjectVertUniform[WhichPass][WhichBuffer]);
            ReleaseUniformBuffer(pVulkan, &m_ObjectFragUniform[WhichPass][WhichBuffer]);
        }   // WhichBuffer
    }   // WhichPass

    ReleaseUniformBuffer(pVulkan, &m_ComputeCtrlUniform);
    ReleaseUniformBuffer(pVulkan, &m_ComputeCtrlUniformHalf);
    ReleaseUniformBuffer(pVulkan, &m_ComputeCtrlUniformQuarter);
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        ReleaseUniformBuffer(pVulkan, &m_NNAOCtrlUniform[WhichBuffer]);
        ReleaseUniformBuffer(pVulkan, &m_BlitFragUniform[WhichBuffer]);
        ReleaseUniformBuffer(pVulkan, &m_LightFragUniform[WhichBuffer]);
    }   // WhichBuffer

    // Finally call into base class destroy
    FrameworkApplicationBase::Destroy();
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    // Reset Metrics
    m_TotalDrawCalls = 0;
    m_TotalTriangles = 0;

    // Grab the vulkan wrapper
    Vulkan* pVulkan = m_vulkan.get();

    // See if we want to change backbuffer surface format
    if (m_RequestedSurfaceFormat.colorSpace != pVulkan->m_SurfaceColorSpace || m_RequestedSurfaceFormat.format != pVulkan->m_SurfaceFormat)
    {
        ChangeSurfaceFormat(m_RequestedSurfaceFormat);
    }

    // Obtain the next swap chain image for the next frame.
    auto CurrentVulkanBuffer = pVulkan->SetNextBackBuffer();
    m_CurrentVulkanBuffer = CurrentVulkanBuffer.idx;

    // ********************************
    // Application Draw() - Begin
    // ********************************

    UpdateGui();

    // Update Camera
    UpdateCamera(fltDiffTime);

    // Wait for game thread to finish

    // If anything changes with the lights (Position, orientation, color, brightness, etc.)
    UpdateLighting(fltDiffTime);

    UpdateShadowMap(fltDiffTime);

    // Update uniform buffers with latest data
    UpdateUniforms(CurrentVulkanBuffer.idx);

    // RP_LIGHT may have additional weights (depending on LPAC/Async Compute work)
    std::vector<VkSemaphore> LightPassWaits;
    std::vector<VkPipelineStageFlags> LightPassWaitStageMasks;

    // First time through, wait for the back buffer to be ready
    tcb::span<const VkSemaphore> pWaitSemaphores = { &CurrentVulkanBuffer.semaphore, 1 };

    const VkPipelineStageFlags DefaultGfxWaitDstStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    const bool ExecuteNNAO = m_LightFragUniformData.AmbientOcclusionScale > 0.0;
    {
        // GBuffer render pass.
        BeginRenderPass(RP_GBUFFER);
        AddPassCommandBuffers(RP_GBUFFER);
        EndRenderPass(RP_GBUFFER);

        if (!ExecuteNNAO)
        {
            SubmitRenderPass(RP_GBUFFER, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_GBUFFER],1 });
        }
        else if (gAsyncComputeNNAO)
        {
            // Submit the gbuffer commands to the queue.
            VkSemaphore SignalSemaphores[] = { m_PassCompleteSemaphore[RP_GBUFFER], m_NNAOAsyncComputeCanStartSemaphore };
            SubmitRenderPass(RP_GBUFFER, pWaitSemaphores, DefaultGfxWaitDstStageMasks, SignalSemaphores);

            // Execue the Ambient Occlusion compute commands on the Async Compute queue.
            m_NNAOAsyncComputeCmdBuffer[CurrentVulkanBuffer.idx].QueueSubmit( m_NNAOAsyncComputeCanStartSemaphore, DefaultGfxWaitDstStageMasks[0], m_NNAOAsyncComputeCompleteSemaphore);
            LightPassWaits.push_back( m_NNAOAsyncComputeCompleteSemaphore );
            LightPassWaitStageMasks.push_back( VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
        }
        else
        {
            // Execue the Ambient Occlusion compute commands on the main (graphics) queue.
            AddPassCommandBuffer(RP_GBUFFER, m_NNAOComputeCmdBuffer[CurrentVulkanBuffer.idx]);

            // Submit the commands (gbuffer and ambient occlusion) to the queue.
            SubmitRenderPass(RP_GBUFFER, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_GBUFFER],1 });
        }
        pWaitSemaphores = {&m_PassCompleteSemaphore[RP_GBUFFER], 1};
    }

    if (gRenderShadows)
    {
        BeginRenderPass(RP_SHADOW);
        AddPassCommandBuffers(RP_SHADOW);
        EndRenderPass(RP_SHADOW);

        // Execue the VSM compute commands on the main (graphics) queue.
        AddPassCommandBuffer(RP_SHADOW, m_VsmComputeCmdBuffer[CurrentVulkanBuffer.idx]);
        // Start the pass rendering (submit to Vulkan queue).
        SubmitRenderPass(RP_SHADOW, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_SHADOW],1 });
        pWaitSemaphores = { &m_PassCompleteSemaphore[RP_SHADOW], 1 };
    }

    {
        BeginRenderPass(RP_LIGHT);
        AddPassCommandBuffers(RP_LIGHT);
        EndRenderPass(RP_LIGHT);

        // Lighting needs to wait for the previous pass (RP_SHADOW) AND the NNAO AsyncCompute task to finish.
        LightPassWaits.push_back( pWaitSemaphores[0] );
        LightPassWaitStageMasks.push_back( VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

        SubmitRenderPass(RP_LIGHT, LightPassWaits, LightPassWaitStageMasks, { &m_PassCompleteSemaphore[RP_LIGHT],1 });
        pWaitSemaphores = { &m_PassCompleteSemaphore[RP_LIGHT], 1 };
    }

    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (gRenderHud && m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = m_Gui->Render(CurrentVulkanBuffer.idx, m_HudRT[0].m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            BeginRenderPass(RP_HUD);
            AddPassCommandBuffers(RP_HUD, { &guiCommandBuffer,1 });

            // End the graphics render pass...
            EndRenderPass(RP_HUD);
            SubmitRenderPass(RP_HUD, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_HUD],1 });
            pWaitSemaphores = { &m_PassCompleteSemaphore[RP_HUD], 1 };
        }
    }

    // Blit Results to the screen
    {
        BeginRenderPass(RP_BLIT);
        AddPassCommandBuffers(RP_BLIT);
        EndRenderPass(RP_BLIT);
        SubmitRenderPass(RP_BLIT, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_BLIT],1 }, CurrentVulkanBuffer.fence);
        pWaitSemaphores = { &m_PassCompleteSemaphore[RP_BLIT], 1 };
    }

    // Queue is loaded up, tell the driver to start processing
    PresentQueue(pWaitSemaphores, CurrentVulkanBuffer.swapchainPresentIdx);

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
bool Application::LoadTextures()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    // Load 'loose' textures
    m_loadedTextures.emplace("Environment", LoadKTXTexture(pVulkan, *m_AssetManager, "./Media/Textures/simplesky_env.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT));
    m_loadedTextures.emplace("Irradiance", LoadKTXTexture(pVulkan, *m_AssetManager, "./Media/Textures/simplesky_irradiance.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT));

    m_TexWhite = LoadKTXTexture(m_vulkan.get(), *m_AssetManager, "./Media/Textures/white_d.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT);
    m_DefaultNormal = LoadKTXTexture(m_vulkan.get(), *m_AssetManager, "./Media/Textures/default_ddn.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT);

    return true;
}

bool Application::CreateRenderTargets()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Creating Render Targets...");
    LOGI("******************************");

    Vulkan* pVulkan = m_vulkan.get();

    const VkFormat DesiredDepthFormat = pVulkan->GetBestVulkanDepthFormat();

    // Setup the GBuffer
    const VkFormat GbufferColorType[] = { VK_FORMAT_R8G8B8A8_UNORM/*Albedo*/, VK_FORMAT_R16G16B16A16_SFLOAT/*Normals*/ };

    if (!m_GBufferRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, GbufferColorType, DesiredDepthFormat, VK_SAMPLE_COUNT_1_BIT, "GBuffer RT"))
    {
        LOGE("Unable to create gbuffer render target");
    }

    // Setup the 'main' (compositing) buffer
    const VkFormat MainColorType[] = { VK_FORMAT_R16G16B16A16_SFLOAT };

    if (!m_MainRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, MainColorType, m_GBufferRT/*inherit depth*/, VK_SAMPLE_COUNT_1_BIT, "Main RT"))
    {
        LOGE("Unable to create main render target");
    }

    // Setup the HUD render target (no depth)
    const VkFormat HudColorType[] = { VK_FORMAT_R8G8B8A8_UNORM };

    if (!m_HudRT.Initialize(pVulkan, gSurfaceWidth, gSurfaceHeight, HudColorType, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitShadowMap()
//-----------------------------------------------------------------------------
{
    //const glm::vec4 shadowPositions[] = { glm::vec4(500.f, 300.f, -50.f, 1.f), glm::vec4(800.f, 300.f, 100.f, 1.f) };
    const glm::vec4 shadowPositions[] = { glm::vec4(-38.f, 19.f, -44.f, 1.f), glm::vec4(-20.f, 19.f, -54.f, 1.f) };
    
    for (uint32_t i = 0; i < cNumShadows; ++i)
    {
        if (!m_Shadows[i].Initialize(*m_vulkan, gShadowMapWidth, gShadowMapHeight, false))
        {
            LOGE("Unable to create shadow (render target?)");
            return false;
        }

        m_Shadows[i].SetLightPos(shadowPositions[i], gShadowLightTarget);
        m_Shadows[i].SetEyeClipPlanes(m_Camera.Fov(), m_Camera.Aspect(), m_Camera.NearClip(), gFarPlaneShadows);
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitLighting()
//-----------------------------------------------------------------------------
{
    m_LightColor = { 0.9f, 0.9f, 0.9f, gSpecularExponent };

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    m_shaderManager = std::make_unique<ShaderManager>(*m_vulkan);
    m_shaderManager->RegisterRenderPassNames({ sRenderPassNames, sRenderPassNames + (sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0])) });

    LOGI("Loading Shaders...");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
        { tIdAndFilename { "ObjectDeferred", "Media\\Shaders\\ObjectDeferred.json" },
          tIdAndFilename { "ObjectEmissive", "Media\\Shaders\\ObjectEmissive.json" },
          tIdAndFilename { "Light", "Media\\Shaders\\Light.json" },
          tIdAndFilename { "Blit", "Media\\Shaders\\Blit.json" },
          tIdAndFilename { "VarianceShadowMap", "Media\\Shaders\\VarianceShadowMap.json" },
          tIdAndFilename { "NNAO", "Media\\Shaders\\NNAO.json" }
        })
    {
        if (!m_shaderManager->AddShader(*m_AssetManager, i.first, i.second))
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
    Vulkan* pVulkan = m_vulkan.get();

    // These are only created here, they are not set to initial values
    LOGI("Creating uniform buffers...");

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
        {
            CreateUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass][WhichBuffer]);
            CreateUniformBuffer(pVulkan, m_ObjectFragUniform[WhichPass][WhichBuffer]);
        }   // WhichBuffer
    }   // WhichPass

    m_ComputeCtrl.width = gRenderWidth;
    m_ComputeCtrl.height = gRenderHeight;
    CreateUniformBuffer(pVulkan, m_ComputeCtrlUniform, &m_ComputeCtrl, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_ComputeCtrl.width = gRenderWidth / 2;
    m_ComputeCtrl.height = gRenderHeight / 2;
    CreateUniformBuffer(pVulkan, m_ComputeCtrlUniformHalf, &m_ComputeCtrl, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_ComputeCtrl.width = gRenderWidth / 4;
    m_ComputeCtrl.height = gRenderHeight / 4;
    CreateUniformBuffer(pVulkan, m_ComputeCtrlUniformQuarter, &m_ComputeCtrl, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Room lights
    m_LightFragUniformData.LightPositions[0] = glm::vec4(-38.0f, 19.0f, -44.f, 20.0f);
    m_LightFragUniformData.LightColors[0] = glm::vec4(0.7f, 1.0f, 1.0f, 1.0f);
    m_LightFragUniformData.LightPositions[1] = glm::vec4(-20.0f, 19.0f, -54.f, 20.0f);
    m_LightFragUniformData.LightColors[1] = glm::vec4(0.7f, 1.0f, 1.0f, 1.0f);

    // Spot lights
    m_LightFragUniformData.LightPositions[2] = glm::vec4(-1.6f, 15.2f, -67.0f, 100.0f);
    m_LightFragUniformData.LightColors[2] = glm::vec4(1.0f, 0.8f, 0.7f, 1.0f);
    m_LightFragUniformData.LightPositions[3] = glm::vec4(1.2f, 15.2f, -67.0f, 100.0f);
    m_LightFragUniformData.LightColors[3] = glm::vec4(1.0f, 0.8f, 0.7f, 1.0f);
    m_LightFragUniformData.LightPositions[4] = glm::vec4(-17.2f, 13.0f, -48.0f, 100.0f);
    m_LightFragUniformData.LightColors[4] = glm::vec4(1.0f, 0.8f, 0.7f, 1.0f);

    m_LightFragUniformData.AmbientColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    m_LightFragUniformData.AmbientOcclusionScale = 1.0f;

    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        CreateUniformBuffer(pVulkan, m_NNAOCtrlUniform[WhichBuffer], &m_NNAOCtrl);
        CreateUniformBuffer(pVulkan, m_BlitFragUniform[WhichBuffer], &m_BlitFragUniformData);
        CreateUniformBuffer(pVulkan, m_LightFragUniform[WhichBuffer], &m_LightFragUniformData);
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t WhichBuffer)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    // ********************************
    // Object
    // ********************************
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // No uniform buffers for HUD or BLIT since objects not in HUD and BLIT
        if (WhichPass == RP_HUD || WhichPass == RP_BLIT || WhichPass == RP_LIGHT)
            continue;

        glm::mat4 LocalModel = glm::mat4(1.0f);
        LocalModel = glm::translate(glm::mat4(1.0f), m_ObjectWorldPos);
        LocalModel = glm::scale(LocalModel, glm::vec3(m_ObjectScale, m_ObjectScale, m_ObjectScale));
        //LocalModel = glm::rotate(LocalModel, 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 LocalMVP = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix() * LocalModel;

        switch (WhichPass)
        {
        case RP_SHADOW:
            m_ObjectVertUniformData.MVPMatrix = m_Shadows[0].GetViewProj() * LocalModel;
            break;
        case RP_GBUFFER:
            m_ObjectVertUniformData.MVPMatrix = LocalMVP;
            break;
        default:
            LOGE("***** %s Not Handled! *****", GetPassName(WhichPass));
            break;
        }

        m_ObjectVertUniformData.ModelMatrix = LocalModel;
        m_ObjectVertUniformData.ShadowMatrix = m_Shadows[0].GetViewProj() * LocalModel;
        UpdateUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass][WhichBuffer], m_ObjectVertUniformData);

        m_ObjectFragUniformData.Color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);  // White by default

        m_ObjectFragUniformData.NormalHeight = glm::vec4(gNormalAmount, gNormalMirrorReflectAmount, 0.0f, 0.0f);

        UpdateUniformBuffer(pVulkan, m_ObjectFragUniform[WhichPass][WhichBuffer], m_ObjectFragUniformData);
    }

    // ********************************
    // Ambient Occlusion
    // ********************************
    m_NNAOCtrl.Width = m_NNAORenderTarget.Width;
    m_NNAOCtrl.Height = m_NNAORenderTarget.Height;
    m_NNAOCtrl.ScreenSize = glm::vec4((float)m_NNAOCtrl.Width, (float)m_NNAORenderTarget.Height, 1.0f / (float)m_NNAOCtrl.Width, 1.0f / (float)m_NNAORenderTarget.Height);
    m_NNAOCtrl.CameraProjection = m_Camera.ProjectionMatrix();
    glm::mat4 CameraProjectionInv = glm::inverse(m_NNAOCtrl.CameraProjection);
    m_NNAOCtrl.CameraInvProjection = CameraProjectionInv;

    UpdateUniformBuffer(pVulkan, m_NNAOCtrlUniform[WhichBuffer], m_NNAOCtrl);

    // ********************************
    // Light
    // ********************************

    glm::mat4 CameraViewInv = glm::inverse(m_Camera.ViewMatrix());
    m_LightFragUniformData.ProjectionInv = CameraProjectionInv;
    m_LightFragUniformData.ViewInv = CameraViewInv;
    m_LightFragUniformData.ViewProjectionInv = CameraViewInv * CameraProjectionInv;
    m_LightFragUniformData.WorldToShadow = m_Shadows[0].GetViewProj();
    m_LightFragUniformData.ProjectionInvW = glm::vec4(CameraProjectionInv[0].w, CameraProjectionInv[1].w, CameraProjectionInv[2].w, CameraProjectionInv[3].w);
    m_LightFragUniformData.CameraPos = glm::vec4(m_Shadows[0].GetLightPos(), 0.0f);

    UpdateUniformBuffer(pVulkan, m_LightFragUniform[WhichBuffer], m_LightFragUniformData);

    // ********************************
    // Blit
    // ********************************

    UpdateUniformBuffer(pVulkan, m_BlitFragUniform[WhichBuffer], m_BlitFragUniformData);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    LOGI("******************************");
    LOGI("Building Command Buffers...");
    LOGI("******************************");

    uint32_t TargetWidth = m_vulkan->m_SurfaceWidth;
    uint32_t TargetHeight = m_vulkan->m_SurfaceHeight;
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

        for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; WhichBuffer++)
        {
            // Set up some values that change based on render pass
            VkRenderPass WhichRenderPass = VK_NULL_HANDLE;
            VkFramebuffer WhichFramebuffer = pVulkan->m_pSwapchainFrameBuffers[WhichBuffer];
            switch (WhichPass)
            {
            case RP_GBUFFER:
                WhichRenderPass = m_GBufferRT.m_RenderPass;
                WhichFramebuffer = m_GBufferRT[0].m_FrameBuffer;
                break;

            case RP_SHADOW:
                WhichFramebuffer = m_Shadows[0].GetFramebuffer();
                WhichRenderPass = m_Shadows[0].GetRenderPass();
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
                WhichFramebuffer = pVulkan->m_pSwapchainFrameBuffers[WhichBuffer];
                break;
            }

            if (WhichPass == RP_LIGHT)
            {
                // Light deferred gbuffer
                if (!m_LightCmdBuffer[WhichBuffer].Begin(WhichFramebuffer, WhichRenderPass))
                {
                    return false;
                }
                vkCmdSetViewport(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
            }

            if (WhichPass == RP_BLIT)
            {
                // Blit (only in the blit pass)
                if (!m_BlitCmdBuffer[WhichBuffer].Begin(WhichFramebuffer, WhichRenderPass, true/*swapchain renderpass*/))
                {
                    return false;
                }
                vkCmdSetViewport(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
            }
            else
            {
                // Objects (can render into any pass except Blit)
                if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].Begin(WhichFramebuffer, WhichRenderPass))
                {
                    return false;
                }
                vkCmdSetViewport(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer, 0, 1, &Scissor);
            }
        }   // Which Buffer
    }   // Which Pass

    // Run through the drawables (each one may be in multiple command buffers)
    for (const auto& drawable : m_SceneObject)
    {
        AddDrawableToCmdBuffers(drawable, &m_ObjectCmdBuffer[0][0], NUM_RENDER_PASSES, m_vulkan->m_SwapchainImageCount);
    }

    // and end their pass
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        if (WhichPass != RP_BLIT)
        {
            for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; WhichBuffer++)
            {
                if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].End())
                {
                    return false;
                }
            }
        }
    }

    // Do the light commands
    AddDrawableToCmdBuffers(*m_LightDrawable.get(), m_LightCmdBuffer, 1, m_vulkan->m_SwapchainImageCount);
    for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_LightCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    // Do the blit commands
    AddDrawableToCmdBuffers(*m_BlitDrawable.get(), m_BlitCmdBuffer, 1, m_vulkan->m_SwapchainImageCount);
    for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_BlitCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; WhichBuffer++)
    {
        // Add the compute command buffers.
        ///TODO: Investigate why adding compute to the primary command buffers (immediately after endrenderpass) crashes vulkan driver on Adreno/
        ///      For now it works if you add the compute commands as a secondary buffer (and add that to the primary immediately after endrenderpass).
        ///      It works on Windows/Nvidia so probably a driver issue that might be fixed in more recent drivers?

        m_VsmComputeCmdBuffer[WhichBuffer].Begin(VK_NULL_HANDLE, VK_NULL_HANDLE);
        if (m_VsmComputable)
        {
            AddComputableToCmdBuffer(*m_VsmComputable, m_VsmComputeCmdBuffer[WhichBuffer]);
        }
        m_VsmComputeCmdBuffer[WhichBuffer].End();

        m_VsmAsyncComputeCmdBuffer[WhichBuffer].Begin();
        if (m_VsmComputable)
        {
            AddComputableToCmdBuffer(*m_VsmComputable, m_VsmAsyncComputeCmdBuffer[WhichBuffer]);
        }
        m_VsmAsyncComputeCmdBuffer[WhichBuffer].End();

        m_DiffractionDownsampleCmdBuffer[WhichBuffer].Begin(VK_NULL_HANDLE, VK_NULL_HANDLE);
        if (m_ComputableTest)
            AddComputableToCmdBuffer(*m_ComputableTest, m_DiffractionDownsampleCmdBuffer[WhichBuffer]);
        m_DiffractionDownsampleCmdBuffer[WhichBuffer].End();

        m_BloomComputeCmdBuffer[WhichBuffer].Begin(VK_NULL_HANDLE, VK_NULL_HANDLE);
        if (m_BloomComputable)
            AddComputableToCmdBuffer(*m_BloomComputable, m_BloomComputeCmdBuffer[WhichBuffer]);
        m_BloomComputeCmdBuffer[WhichBuffer].End();

        m_BloomAsyncComputeCmdBuffer[WhichBuffer].Begin();
        if (m_BloomComputable)
            AddComputableToCmdBuffer(*m_BloomComputable, m_BloomAsyncComputeCmdBuffer[WhichBuffer]);
        m_BloomAsyncComputeCmdBuffer[WhichBuffer].End();

        m_NNAOComputeCmdBuffer[WhichBuffer].Begin(VK_NULL_HANDLE, VK_NULL_HANDLE);
        if (m_NNAOComputable)
        {
            AddComputableToCmdBuffer(*m_NNAOComputable, m_NNAOComputeCmdBuffer[WhichBuffer]);
        }
        m_NNAOComputeCmdBuffer[WhichBuffer].End();

        m_NNAOAsyncComputeCmdBuffer[WhichBuffer].Begin();
        if (m_NNAOComputable)
        {
            AddComputableToCmdBuffer(*m_NNAOComputable, m_NNAOAsyncComputeCmdBuffer[WhichBuffer]);
        }
        m_NNAOAsyncComputeCmdBuffer[WhichBuffer].End();
    }
    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitLocalSemaphores()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = m_vulkan.get();

    VkSemaphoreCreateInfo SemaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_PassCompleteSemaphore[WhichPass]);
        if (!CheckVkError("vkCreateSemaphore()", RetVal))
        {
            return false;
        }
    }

    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_VsmAsyncComputeCanStartSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }
    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_VsmAsyncComputeCompleteSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }

    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_BloomAsyncComputeCanStartSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }
    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_BloomAsyncComputeCompleteSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }

    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_NNAOAsyncComputeCanStartSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }
    RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_NNAOAsyncComputeCompleteSemaphore);
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
    Vulkan* pVulkan = m_vulkan.get();

    char szName[256];
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
        {
            // The Pass Command Buffer => Primary
            sprintf(szName, "Primary (%s; Buffer %d of %d)", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_PassCmdBuffer[WhichBuffer][WhichPass].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
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

        // Blit => Secondary
        sprintf(szName, "Blit (%s; Buffer %d of %d)", GetPassName(RP_BLIT), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
        if (!m_BlitCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }

        // Light => Secondary
        sprintf(szName, "Light (%s; Buffer %d of %d)", GetPassName(RP_BLIT), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
        if (!m_LightCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }

        // Variant Shadow Map calculation - compute in the ASYNC COMPUTE queue
        sprintf(szName, "VSM generation ASYNC (Buffer 1 of 1)");
        if (!m_VsmAsyncComputeCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true/*compute*/))
        {
            return false;
        }
        // Variant Shadow Map calculation - compute but in graphics (regular) queue
        sprintf(szName, "VSM generation (Buffer 1 of 1)");
        if (!m_VsmComputeCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }

        // DiffractionDownsample => Secondary (?)
        sprintf(szName, "DiffractionDownsample (Buffer 1 of 1)");
        if (!m_DiffractionDownsampleCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }

        // BloomDownsample => Secondary (?)
        sprintf(szName, "BloomDownsample (Buffer 1 of 1)");
        if (!m_BloomComputeCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }

        // BloomDownsample => Secondary (?)
        sprintf(szName, "BloomDownsample ASYNC (Buffer 1 of 1)");
        if (!m_BloomAsyncComputeCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true/*compute*/))
        {
            return false;
        }

        // NNAO => Secondary (?)
        sprintf(szName, "NNAO (Buffer 1 of 1)");
        if (!m_NNAOComputeCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }

        // NNAO => Secondary (?)
        sprintf(szName, "NNAO ASYNC (Buffer 1 of 1)");
        if (!m_NNAOAsyncComputeCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true/*compute*/))
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
    Vulkan* pVulkan = m_vulkan.get();

    // Fill in the Setup data
    uint32_t ShadowTargetWidth, ShadowTargetHeight;
    m_Shadows[0].GetTargetSize(ShadowTargetWidth, ShadowTargetHeight);

    m_PassSetup[RP_GBUFFER] = { m_GBufferRT[0].m_pLayerFormats, m_GBufferRT[0].m_DepthFormat,      RenderPassInputUsage::Clear,   true,  RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Store,      gClearColor,m_GBufferRT[0].m_Width,   m_GBufferRT[0].m_Height };
    m_PassSetup[RP_SHADOW] =  { {},                             m_Shadows[0].GetDepthFormat(0),    RenderPassInputUsage::DontCare,true,  RenderPassOutputUsage::Discard,       RenderPassOutputUsage::StoreReadOnly, {},      ShadowTargetWidth,        ShadowTargetHeight };
    m_PassSetup[RP_LIGHT] =   { m_MainRT[0].m_pLayerFormats,    m_GBufferRT[0].m_DepthFormat,      RenderPassInputUsage::Clear,   false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},         m_MainRT[0].m_Width,      m_MainRT[0].m_Height };
    m_PassSetup[RP_HUD] =     { m_HudRT[0].m_pLayerFormats,     m_HudRT[0].m_DepthFormat,          RenderPassInputUsage::Clear,   false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},         m_HudRT[0].m_Width,       m_HudRT[0].m_Height };
    m_PassSetup[RP_BLIT] =    { {pVulkan->m_SurfaceFormat},     pVulkan->m_SwapchainDepth.format,  RenderPassInputUsage::DontCare,false, RenderPassOutputUsage::Present,       RenderPassOutputUsage::Discard,    {},         m_vulkan->m_SurfaceWidth, m_vulkan->m_SurfaceHeight };

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        const auto& PassSetup = m_PassSetup[WhichPass];
        bool IsSwapChainRenderPass = WhichPass == RP_BLIT;

        if (WhichPass == RP_SHADOW)
        {
            m_RenderPass[WhichPass] = m_Shadows[0].GetRenderPass();
        }
        else
        {
            if (!m_vulkan->CreateRenderPass({PassSetup.ColorFormats},
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

    const auto& bufferLoader = [&](const std::string& bufferSlotName) -> MaterialPass::tPerFrameVkBuffer {
        if (bufferSlotName == "Vert")
        {
            return { m_ObjectVertUniform[RP_GBUFFER][0].buf.GetVkBuffer() };
        }
        else if (bufferSlotName == "Frag")
        {
            return { m_ObjectFragUniform[RP_GBUFFER][0].buf.GetVkBuffer() };
        }
        else if (bufferSlotName == "VertShadow")
        {
            return { m_ObjectVertUniform[RP_SHADOW][0].buf.GetVkBuffer() };
        }
        else if (bufferSlotName == "FragShadow")
        {
            return { m_ObjectFragUniform[RP_SHADOW][0].buf.GetVkBuffer() };
        }
        else
        {
            assert(0);
            return {};
        }
    };

    const auto* pOpaqueShader = m_shaderManager->GetShader("ObjectDeferred");
    if (!pOpaqueShader)
    {
        // Error (bad shaderName)
        return false;
    }

    const auto* pEmissiveShader = m_shaderManager->GetShader("ObjectEmissive");
    if (!pEmissiveShader)
    {
        // Error (bad shaderName)
        return false;
    }

    const auto& museumMaterialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef) -> std::optional<Material>
    {
        using namespace std::placeholders;

        auto* diffuseTexture           = GetOrLoadTexture(materialDef.diffuseFilename.c_str());
        auto* normalTexture            = GetOrLoadTexture(materialDef.bumpFilename.c_str());
        auto* emissiveTexture          = GetOrLoadTexture(materialDef.emissiveFilename.c_str());
        auto* metallicRoughnessTexture = GetOrLoadTexture(materialDef.specMapFilename.c_str());

        const Shader* targetShader = !materialDef.emissiveFilename.empty() ? pEmissiveShader : pOpaqueShader;

        return m_MaterialManager->CreateMaterial(*m_vulkan, *targetShader, NUM_VULKAN_BUFFERS, 
            [&](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
            {
                if (texName == "ShadowDepth")
                {
                    return { &m_Shadows[0].GetDepthTexture(0) };
                }
                else if (texName == "ShadowVarianceDepth")
                {
                    return { &m_VsmTarget };
                }
                else if (texName == "Refract")
                {
                    return { &m_ComputeRenderTarget };
                }
                else if (texName == "Environment" || texName == "Irradiance")
                {
                    auto textureIt = m_loadedTextures.find(texName);
                    if (textureIt != m_loadedTextures.end() && !textureIt->second.IsEmpty())
                        return { &textureIt->second };
                    // File not loaded, use default
                    return { &m_TexWhite };
                }

                if (texName == "Diffuse")
                {
                    return { diffuseTexture ? diffuseTexture : &m_TexWhite };
                }
                if (texName == "Normal")
                {
                    return { normalTexture ? normalTexture : &m_DefaultNormal };
                }
                if (texName == "Emissive")
                {
                    return { emissiveTexture ? emissiveTexture : &m_TexWhite };
                }
                if (texName == "MetallicRoughness")
                {
                    return { metallicRoughnessTexture ? metallicRoughnessTexture : &m_TexWhite };
                }

                return {};
            }, bufferLoader);
    };

    DrawableLoader::LoadDrawables( *m_vulkan, *m_AssetManager, m_RenderPass, sRenderPassNames, "./Media/Meshes/Museum.gltf", museumMaterialLoader, m_SceneObject, {}, DrawableLoader::LoaderFlags::None, {} );
 
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
    return m_vulkan->SetSwapchainHrdMetadata(AuthoringProfile);
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    // Gui
    assert(m_RenderPass[RP_HUD] != VK_NULL_HANDLE);
    m_Gui = std::make_unique<GuiImguiVulkan>(*m_vulkan, m_RenderPass[RP_HUD]);
    if (!m_Gui->Initialize(windowHandle, gRenderWidth, gRenderHeight))
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
        ImGui::SetNextWindowSize(ImVec2((gRenderWidth * 3.0f) / 4.0f, 500.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(gRenderWidth / 8.0f, gRenderHeight / 2.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", &settingsOpen, (ImGuiWindowFlags)0))
        {
            // Add our widgets
            const VkSurfaceFormatKHR* selectedFormat = nullptr;
            const auto& formats = m_vulkan->GetVulkanSurfaceFormats();
            // Find which format is 'current'
            for (const auto& format : formats)
            {
                if (format.format == m_vulkan->m_SurfaceFormat && format.colorSpace == m_vulkan->m_SurfaceColorSpace)
                {
                    selectedFormat = &format;
                    break;
                }
            }

            const auto FormatToString = [](const VkSurfaceFormatKHR& format) -> std::string {
                std::string selectableText(Vulkan::VulkanColorSpaceString(format.colorSpace));
                selectableText.append(" \\ ");
                selectableText.append(Vulkan::VulkanFormatString(format.format));
                return selectableText;
            };
            if (ImGui::BeginCombo("Surface", selectedFormat ? FormatToString(*selectedFormat).c_str() : "Unknown", ImGuiComboFlags_HeightLargest))
            {
                for (const auto& format : formats)
                {
                    bool isSelected = selectedFormat == &format;
                    if (ImGui::Selectable(FormatToString(format).c_str(), true))
                    {
                        if (selectedFormat != &format)
                        {
                            m_RequestedSurfaceFormat = format;
                        }
                        selectedFormat = &format;
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (strstr(Vulkan::VulkanFormatString(m_vulkan->m_SurfaceFormat), "SRGB") == nullptr)
            {
                // Show the sRGB encode option (determine if the bliut shader should do the encode)
                ImGui::Checkbox("sRGB encode", &m_bEncodeSRGB);
                m_BlitFragUniformData.sRGB = m_bEncodeSRGB ? 1 : 0;
            }
            else
            {
                // Hardware will handle the sRGB output buffer encoding
                m_BlitFragUniformData.sRGB = 0;
            }
            float spotLightIntensity = m_LightFragUniformData.LightPositions[2].w;
            float roomLightIntensity = m_LightFragUniformData.LightPositions[0].w;
            ImGui::SliderFloat("Spot Lights", &spotLightIntensity, 0.0f, 300000.0f, nullptr, ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Room Lights", &roomLightIntensity, 0.0f, 300000.0f, nullptr, ImGuiSliderFlags_Logarithmic);

            m_LightFragUniformData.LightPositions[0].w = roomLightIntensity;
            m_LightFragUniformData.LightPositions[1].w = roomLightIntensity;
            m_LightFragUniformData.LightPositions[2].w = spotLightIntensity;
            m_LightFragUniformData.LightPositions[3].w = spotLightIntensity;
            m_LightFragUniformData.LightPositions[4].w = spotLightIntensity;

            ImGui::SliderFloat("Occlusion", &m_LightFragUniformData.AmbientOcclusionScale, 0.0f, 3.0f);
            ImGui::SliderFloat("Brightness", &m_BlitFragUniformData.Diffuse, 0.0f, 10.0f);
        }
        ImGui::End(); 

        if (ImGui::Begin("FPS", (bool*)nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("FPS: %.1f", m_CurrentFPS);
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
}

//-----------------------------------------------------------------------------
void Application::UpdateShadowMap(float ElapsedTime)
//-----------------------------------------------------------------------------
{
    m_Shadows[0].Update(m_Camera.ViewMatrix());
}


//-----------------------------------------------------------------------------
bool Application::ChangeSurfaceFormat(VkSurfaceFormatKHR newSurfaceFormat)
//-----------------------------------------------------------------------------
{
    if (!m_vulkan->ChangeSurfaceFormat(newSurfaceFormat))
    {
        return false;
    }

    // We need to modify the blit render pass (the only one that touches the output framebuffer).
    // RenderPass needs to be compatible with the framebuffer's format.
    // m_PassCmdBuffer[RP_BLIT] is good, gets rebuilt every frame
 
    for (auto& blitCmdBuffer : m_BlitCmdBuffer)
    {
        if (!blitCmdBuffer.Reset())
            return false;
    }

    vkDestroyRenderPass(m_vulkan->m_VulkanDevice, m_RenderPass[RP_BLIT], nullptr);
    m_RenderPass[RP_BLIT] = VK_NULL_HANDLE;

    auto& PassSetup = m_PassSetup[RP_BLIT];
    PassSetup.ColorFormats = { m_vulkan->m_SurfaceFormat };
    PassSetup.DepthFormat = { m_vulkan->m_SwapchainDepth.format };

    if (!m_vulkan->CreateRenderPass({ PassSetup.ColorFormats },
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

    // Rebuild the m_BlitCmdBuffer

    VkViewport Viewport = {};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float)PassSetup.TargetWidth;
    Viewport.height = (float)PassSetup.TargetHeight;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = PassSetup.TargetWidth;
    Scissor.extent.height = PassSetup.TargetHeight;

    for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; ++WhichBuffer)
    {
        if (!m_BlitCmdBuffer[WhichBuffer].Begin(m_vulkan->m_pSwapchainFrameBuffers[WhichBuffer], m_RenderPass[RP_BLIT], true/*swapchain renderpass*/))
        {
            return false;
        }

        vkCmdSetViewport(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
        vkCmdSetScissor(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
    }
    // Add the blit commands
    AddDrawableToCmdBuffers(*m_BlitDrawable.get(), m_BlitCmdBuffer, 1, m_vulkan->m_SwapchainImageCount);
    // End the blit command buffer
    for (uint32_t WhichBuffer = 0; WhichBuffer < m_vulkan->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_BlitCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    InitHdr();

    return true;
}

//-----------------------------------------------------------------------------
void Application::BeginRenderPass(RENDER_PASS WhichPass)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    const auto& PassSetup = m_PassSetup[WhichPass];

    // Reset the primary command buffer...
    if (!m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].Reset())
    {
        // What should be done here?
    }

    // ... begin the primary command buffer ...
    if (!m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].Begin())
    {
        // What should be done here?
    }

    VkFramebuffer Framebuffer;
    switch (WhichPass)
    {
    case RP_GBUFFER:
        Framebuffer = m_GBufferRT[0].m_FrameBuffer;
        break;
    case RP_SHADOW:
        Framebuffer = m_Shadows[0].GetFramebuffer();
        break;
    case RP_LIGHT:
        Framebuffer = m_MainRT[0].m_FrameBuffer;
        break;
    case RP_HUD:
        Framebuffer = m_HudRT[0].m_FrameBuffer;
        break;
    case RP_BLIT:
        Framebuffer = pVulkan->m_pSwapchainFrameBuffers[m_CurrentVulkanBuffer];
        break;
    default:
        assert(0);
    }

    VkRect2D PassArea = {};
    PassArea.offset.x = 0;
    PassArea.offset.y = 0;
    PassArea.extent.width = PassSetup.TargetWidth;
    PassArea.extent.height = PassSetup.TargetHeight;

    VkClearColorValue ClearColor[1] = { { PassSetup.ClearColor[0], PassSetup.ClearColor[1], PassSetup.ClearColor[2], PassSetup.ClearColor[3] } };
    bool IsSwapChainRenderPass = WhichPass==RP_BLIT;
    VkSubpassContents SubContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

    m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].BeginRenderPass(PassArea, 0.0f, 1.0f, ClearColor, (uint32_t)PassSetup.ColorFormats.size(), PassSetup.DepthFormat != VK_FORMAT_UNDEFINED, m_RenderPass[WhichPass], IsSwapChainRenderPass, Framebuffer, SubContents);
}

//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffers(RENDER_PASS WhichPass)
//-----------------------------------------------------------------------------
{
    std::vector<VkCommandBuffer> SubCommandBuffers;
    SubCommandBuffers.reserve(2);

    // Add sub commands to render list

    if (WhichPass == RP_LIGHT)
    {
        // Light cmd buffer is added before the m_ObjectCmdBuffer as there may be some objects rendering to the RP_LIGHT pass after the initial light pass (eg emissives)
        SubCommandBuffers.push_back(m_LightCmdBuffer[m_CurrentVulkanBuffer].m_VkCommandBuffer);
        m_TotalDrawCalls += m_LightCmdBuffer[m_CurrentVulkanBuffer].m_NumDrawCalls;
        m_TotalTriangles += m_LightCmdBuffer[m_CurrentVulkanBuffer].m_NumTriangles;
    }

    if (m_ObjectCmdBuffer[m_CurrentVulkanBuffer][WhichPass].m_NumDrawCalls)
    {
        SubCommandBuffers.push_back(m_ObjectCmdBuffer[m_CurrentVulkanBuffer][WhichPass].m_VkCommandBuffer);
        m_TotalDrawCalls += m_ObjectCmdBuffer[m_CurrentVulkanBuffer][WhichPass].m_NumDrawCalls;
        m_TotalTriangles += m_ObjectCmdBuffer[m_CurrentVulkanBuffer][WhichPass].m_NumTriangles;
    }

    if (WhichPass == RP_BLIT)
    {
        assert(SubCommandBuffers.empty()); // We currently don't handle m_ObjectCmdBuffer having commands to go in the blit pass, would need to recreate the cmd buffer when the renderpass changes (which will happen when the backbuffer surface format is switched)
        SubCommandBuffers.push_back(m_BlitCmdBuffer[m_CurrentVulkanBuffer].m_VkCommandBuffer);
        m_TotalDrawCalls += m_BlitCmdBuffer[m_CurrentVulkanBuffer].m_NumDrawCalls;
        m_TotalTriangles += m_BlitCmdBuffer[m_CurrentVulkanBuffer].m_NumTriangles;
    }

    // Add all subcommands
    AddPassCommandBuffers(WhichPass, SubCommandBuffers);
}

//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffers(RENDER_PASS WhichPass, tcb::span<VkCommandBuffer> SubCommandBuffers)
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
        vkCmdExecuteCommands(m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].m_VkCommandBuffer, (uint32_t)SubCommandBuffers.size(), SubCommandBuffers.data());
    }
}

//-----------------------------------------------------------------------------
void Application::EndRenderPass(RENDER_PASS WhichPass)
//-----------------------------------------------------------------------------
{
    m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].EndRenderPass();
}

//-----------------------------------------------------------------------------
void Application::SubmitRenderPass(RENDER_PASS WhichPass, const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, VkFence CompletionFence)
//-----------------------------------------------------------------------------
{
    // ... end the command buffer ...
    m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].End();

    m_PassCmdBuffer[m_CurrentVulkanBuffer][WhichPass].QueueSubmit(WaitSemaphores, WaitDstStageMasks, SignalSemaphores, CompletionFence);
}

