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

#include "application.hpp"
#include "main/applicationEntrypoint.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "camera/cameraData.hpp"
#include "camera/cameraGltfLoader.hpp"
#include "gui/imguiVulkan.hpp"
#include "material/drawable.hpp"
#include "material/vulkan/shaderModule.hpp"
#include "material/shaderManagerT.hpp"
#include "material/materialManager.hpp"
#include "material/vulkan/specializationConstantsLayout.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/meshLoader.hpp"
#include "system/math_common.hpp"
#include "texture/textureManager.hpp"
#include "imgui.h"

#include <random>
#include <iostream>
#include <filesystem>

namespace
{
    static constexpr std::array<const char*, NUM_RENDER_PASSES> sRenderPassNames = { "RP_SGSR", "RP_HUD", "RP_BLIT" };

    glm::vec3 gCameraStartPos = glm::vec3(26.48f, 20.0f, -5.21f);
    glm::vec3 gCameraStartRot = glm::vec3(0.0f, 110.0f, 0.0f);

    float   gFOV = PI_DIV_4;
    float   gNearPlane = 1.0f;
    float   gFarPlane = 1800.0f;
}

///
/// @brief Implementation of the Application entrypoint (called by the framework)
/// @return Pointer to Application (derived from @FrameworkApplicationBase).
/// Creates the Application class.  Ownership is passed to the calling (framework) function.
/// 
FrameworkApplicationBase* Application_ConstructApplication()
{
    return new Application();
}

Application::Application() : ApplicationHelperBase()
{

}

Application::~Application()
{
}

//-----------------------------------------------------------------------------
bool Application::Initialize(uintptr_t windowHandle, uintptr_t hInstance)
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize( windowHandle, hInstance ))
    {
        return false;
    }

    // Window width and height are the same as surface width and height on windows, but on 
    // Android these represent the device screen resolution
    // If smaller resolutions are desired, modify these values as needed (or use their default
    // values)
    gSurfaceWidth  = m_WindowWidth;
    gSurfaceHeight = m_WindowHeight;
    gRenderWidth   = m_WindowWidth;
    gRenderHeight  = m_WindowHeight;

    if (!InitializeCamera())
    {
        return false;
    }

    if (!LoadShaders())
    {
        return false;
    }

    if (!InitUniforms())
    {
        return false;
    }

    if (!CreateRenderTargets())
    {
        return false;
    }

    if (!InitAllRenderPasses())
    {
        return false;
    }

    if (!InitGui(windowHandle))
    {
        return false;
    }

    if (!LoadMeshObjects())
    {
        return false;
    }

    if (!InitCommandBuffers())
    {
        return false;
    }

    if (!InitLocalSemaphores())
    {
        return false;
    }

    if (!BuildCmdBuffers())
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // Uniform Buffers
    ReleaseUniformBuffer(pVulkan, &m_SGSRFragUniform);
     
    // Cmd buffers
    for (int whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        for (auto& cmdBuffer : m_RenderPassData[whichPass].PassCmdBuffer)
        {
            cmdBuffer.Release();
        }

        for (auto& cmdBuffer : m_RenderPassData[whichPass].ObjectsCmdBuffer)
        {
            cmdBuffer.Release();
        }

        m_RenderPassData[whichPass].RenderTarget.Release();
    }

    // Render passes / Semaphores
    for (int whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        vkDestroyRenderPass(pVulkan->m_VulkanDevice, m_RenderPassData[whichPass].RenderPass, nullptr);
        vkDestroySemaphore(pVulkan->m_VulkanDevice, m_RenderPassData[whichPass].PassCompleteSemaphore, nullptr);
    }

    // Drawables
    m_SGSRQuadDrawable.reset();
    m_BlitQuadDrawable.reset();

    // Internal
    m_ShaderManager.reset();
    m_MaterialManager.reset();
    m_CameraController.reset();
    m_AssetManager.reset();

    ApplicationHelperBase::Destroy();
}

//-----------------------------------------------------------------------------
int Application::PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat> surfaceFormat)
//-----------------------------------------------------------------------------
{
    for (int i = 0; i < surfaceFormat.size(); i++)
    {
        if (surfaceFormat[i].format == TextureFormat::B8G8R8A8_SRGB || surfaceFormat[i].format == TextureFormat::R8G8B8A8_SRGB)
        {
            return i;
        }
    }
    
    return -1;
}

//-----------------------------------------------------------------------------
void Application::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& configuration)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool Application::InitializeCamera()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Initializing Camera...");
    LOGI("******************************");

    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    // Camera Controller //

#if defined(OS_ANDROID)
    typedef CameraControllerTouch           tCameraController;
#else
    typedef CameraController                tCameraController;
#endif

    auto cameraController = std::make_unique<tCameraController>();
    if (!cameraController->Initialize(gRenderWidth, gRenderHeight))
    {
        return false;
    }

    m_CameraController = std::move(cameraController);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    m_ShaderManager = std::make_unique<ShaderManagerT<Vulkan>>(*GetVulkan());
    m_ShaderManager->RegisterRenderPassNames(sRenderPassNames);

    m_MaterialManager = std::make_unique<MaterialManagerT<Vulkan>>();

    LOGI("******************************");
    LOGI("Loading Shaders...");
    LOGI("******************************");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
            { tIdAndFilename { "Blit",  "Media\\Shaders\\Blit.json" },
              tIdAndFilename { "SGSR", "Media\\Shaders\\sgsr_shader_mobile.json" }
            })
    {
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
            LOGI("Please verify if you have all required assets on the sample media folder");
            LOGI("If you are running on Android, don't forget to run the `02_CopyMediaToDevice.bat` script to copy all media files into the device memory");
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateRenderTargets()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    LOGI("**************************");
    LOGI("Creating Render Targets...");
    LOGI("**************************");

    const TextureFormat MainColorType[] = { TextureFormat::B8G8R8A8_SRGB };
    const TextureFormat HudColorType[]  = { TextureFormat::B8G8R8A8_SRGB };

    if (!m_RenderPassData[RP_SGSR].RenderTarget.Initialize(pVulkan, gRenderWidth, gRenderHeight, MainColorType, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "SGSR RT"))
    {
        LOGE("Unable to create scene render target");
        return false;
    }

    // Notice no depth on the HUD RT
    if (!m_RenderPassData[RP_HUD].RenderTarget.Initialize(pVulkan, gSurfaceWidth, gSurfaceHeight, HudColorType, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitUniforms()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Initializing Uniforms...");
    LOGI("******************************");

    Vulkan* const pVulkan = GetVulkan();

    if (!CreateUniformBuffer(pVulkan, m_SGSRFragUniform))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitAllRenderPasses()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    //                                       ColorInputUsage |               ClearDepthRenderPass | ColorOutputUsage |                     DepthOutputUsage |              ClearColor
    m_RenderPassData[RP_SGSR].PassSetup  = { RenderPassInputUsage::Clear,    true,                  RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard, {}};
    m_RenderPassData[RP_HUD].PassSetup   = { RenderPassInputUsage::Clear,    true,                  RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard, {}};
    m_RenderPassData[RP_BLIT].PassSetup  = { RenderPassInputUsage::DontCare, true,                  RenderPassOutputUsage::Present,        RenderPassOutputUsage::Discard, {}};

    TextureFormat surfaceFormat = pVulkan->m_SurfaceFormat;
    auto swapChainColorFormat = std::span<const TextureFormat>({ &surfaceFormat, 1 });
    auto swapChainDepthFormat = pVulkan->m_SwapchainDepth.format;

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        bool isSwapChainRenderPass = whichPass == RP_BLIT;

        std::span<const TextureFormat> colorFormats = isSwapChainRenderPass ? swapChainColorFormat : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
        TextureFormat                  depthFormat  = isSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

        const auto& passSetup = m_RenderPassData[whichPass].PassSetup;
        
        if (!pVulkan->CreateRenderPass(
            { colorFormats },
            depthFormat,
            VK_SAMPLE_COUNT_1_BIT,
            passSetup.ColorInputUsage,
            passSetup.ColorOutputUsage,
            passSetup.ClearDepthRenderPass,
            passSetup.DepthOutputUsage,
            & m_RenderPassData[whichPass].RenderPass))
        {
            return false;
        }
            
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    const auto& hudRenderTarget = m_RenderPassData[RP_HUD].RenderTarget;
    m_Gui = std::make_unique<GuiImguiGfx<Vulkan>>(*GetVulkan(), m_RenderPassData[RP_HUD].RenderPass);
    if (!m_Gui->Initialize(windowHandle, hudRenderTarget[0].m_Width, hudRenderTarget[0].m_Height))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    LOGI("***********************");
    LOGI("Initializing Shaders... ");
    LOGI("***********************");

    const auto* pSGSRQuadShader         = m_ShaderManager->GetShader("SGSR");
    const auto* pBlitQuadShader         = m_ShaderManager->GetShader("Blit");
    if (!pSGSRQuadShader || !pBlitQuadShader)
    {
        return false;
    }
    
    LOGI("*******************************");
    LOGI("Loading and preparing assets...");
    LOGI("*******************************");

    m_TextureManager->SetDefaultFilenameManipulators(PathManipulator_PrefixDirectory{ "Media\\" }, PathManipulator_ChangeExtension{ ".ktx" });

    m_sgsr_sampler = CreateSampler(*pVulkan, SamplerAddressMode::ClampEdge, SamplerFilter::Nearest, SamplerBorderColor::TransparentBlackFloat, 0.0f/*mipBias*/);
    if (m_sgsr_sampler.IsEmpty())
        return false;

    auto* sceneTexture = apiCast<tGfxApi>(m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\artifact.ktx", m_sgsr_sampler));

    if (!sceneTexture)
    {
        LOGE("Failed to load supporting textures");
        return false;
    }

    m_scene_texture_original_size.x = static_cast<float>(sceneTexture->Width);
    m_scene_texture_original_size.y = static_cast<float>(sceneTexture->Height);

    LOGI("*********************");
    LOGI("Creating SGSR mesh...");
    LOGI("*********************");

    MeshObject sgsrQuadMesh;
    MeshHelper::CreateScreenSpaceMesh(pVulkan->GetMemoryManager(), 0, &sgsrQuadMesh);

    // SGSR Material
    auto sgsrQuadShaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pSGSRQuadShader, pVulkan->m_SwapchainImageCount,
        [this, &sceneTexture](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
        {
            if (texName == "SceneColor")
            {
                return { sceneTexture };
            }
            return {};
        },
        [this](const std::string& bufferName) -> tPerFrameVkBuffer
        {
            if (bufferName == "SceneInfo")
            {
                return { m_SGSRFragUniform.buf.GetVkBuffer() };
            }

            return {};
        }
        );

    m_SGSRQuadDrawable = std::make_unique<Drawable>(*pVulkan, std::move(sgsrQuadShaderMaterial));
    if (!m_SGSRQuadDrawable->Init(m_RenderPassData[RP_SGSR].RenderPass, sRenderPassNames[RP_SGSR], std::move(sgsrQuadMesh)))
    {
        return false;
    }

    LOGI("*********************");
    LOGI("Creating Blit mesh...");
    LOGI("*********************");

    MeshObject blitQuadMesh;
    MeshHelper::CreateScreenSpaceMesh(pVulkan->GetMemoryManager(), 0, &blitQuadMesh);

    // Blit Material
    auto blitQuadShaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pBlitQuadShader, pVulkan->m_SwapchainImageCount,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
        {
            if (texName == "Diffuse")
            {
                return { &m_RenderPassData[RP_SGSR].RenderTarget[0].m_ColorAttachments[0] };
            }
            else if (texName == "Overlay")
            {
                return { &m_RenderPassData[RP_HUD].RenderTarget[0].m_ColorAttachments[0] };
            }
            return {};
        },
        [this](const std::string& bufferName) -> tPerFrameVkBuffer
        {
            return {};
        }
        );

    m_BlitQuadDrawable = std::make_unique<Drawable>(*pVulkan, std::move(blitQuadShaderMaterial));
    if (!m_BlitQuadDrawable->Init(m_RenderPassData[RP_BLIT].RenderPass, sRenderPassNames[RP_BLIT], std::move(blitQuadMesh)))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("*******************************");
    LOGI("Initializing Command Buffers...");
    LOGI("*******************************");

    Vulkan* const pVulkan = GetVulkan();

    auto GetPassName = [](uint32_t whichPass)
    {
        if (whichPass >= sRenderPassNames.size())
        {
            LOGE("GetPassName() called with unknown pass (%d)!", whichPass);
            return "RP_UNKNOWN";
        }

        return sRenderPassNames[whichPass];
    };

    m_RenderPassData[RP_SGSR].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_SGSR].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_HUD].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_HUD].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_BLIT].PassCmdBuffer.resize(pVulkan->m_SwapchainImageCount);
    m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.resize(pVulkan->m_SwapchainImageCount);

    char szName[256];
    const VkCommandBufferLevel CmdBuffLevel = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        for (uint32_t whichBuffer = 0; whichBuffer < m_RenderPassData[whichPass].PassCmdBuffer.size(); whichBuffer++)
        {
            // The Pass Command Buffer => Primary
            sprintf(szName, "Primary (%s; Buffer %d of %d)", GetPassName(whichPass), whichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
            {
                return false;
            }

            // Model => Secondary
            sprintf(szName, "Model (%s; Buffer %d of %d)", GetPassName(whichPass), whichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].Initialize(pVulkan, szName, CmdBuffLevel))
            {
                return false;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitLocalSemaphores()
//-----------------------------------------------------------------------------
{
    LOGI("********************************");
    LOGI("Initializing Local Semaphores...");
    LOGI("********************************");

    const VkSemaphoreCreateInfo SemaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        VkResult retVal = vkCreateSemaphore(GetVulkan()->m_VulkanDevice, &SemaphoreInfo, NULL, &m_RenderPassData[whichPass].PassCompleteSemaphore);
        if (!CheckVkError("vkCreateSemaphore()", retVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("***************************");
    LOGI("Building Command Buffers...");
    LOGI("****************************");

    Vulkan* const pVulkan = GetVulkan();

    // Begin recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData         = m_RenderPassData[whichPass];
        bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];

            uint32_t targetWidth  = bisSwapChainRenderPass ? pVulkan->m_SurfaceWidth : renderPassData.RenderTarget[0].m_Width;
            uint32_t targetHeight = bisSwapChainRenderPass ? pVulkan->m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

            VkViewport viewport = {};
            viewport.x          = 0.0f;
            viewport.y          = 0.0f;
            viewport.width      = (float)targetWidth;
            viewport.height     = (float)targetHeight;
            viewport.minDepth   = 0.0f;
            viewport.maxDepth   = 1.0f;

            VkRect2D scissor      = {};
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;
            scissor.extent.width  = targetWidth;
            scissor.extent.height = targetHeight;

            // Set up some values that change based on render pass
            VkRenderPass  whichRenderPass  = renderPassData.RenderPass;
            VkFramebuffer whichFramebuffer = bisSwapChainRenderPass ? pVulkan->m_SwapchainBuffers[whichBuffer].framebuffer : renderPassData.RenderTarget[0].m_FrameBuffer;

            // Objects (can render into any pass except Blit)
            if (!cmdBufer.Begin(whichFramebuffer, whichRenderPass, bisSwapChainRenderPass))
            {
                return false;
            }
            vkCmdSetViewport(cmdBufer.m_VkCommandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBufer.m_VkCommandBuffer, 0, 1, &scissor);
        }
    }

    // Blit quad drawable
    AddDrawableToCmdBuffers(*m_BlitQuadDrawable.get(), m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.size()));

    // SGSR quad drawable
    AddDrawableToCmdBuffers(*m_SGSRQuadDrawable.get(), m_RenderPassData[RP_SGSR].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_SGSR].ObjectsCmdBuffer.size()));

    // End recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData = m_RenderPassData[whichPass];

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];
            if (!cmdBufer.End())
            {
                return false;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
    if (m_Gui)
    {
        m_Gui->Update();
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::Begin("FPS", (bool*)nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("Texture Size [%f, %f]", m_SGSRFragUniformData.TechniqueInfo.z, m_SGSRFragUniformData.TechniqueInfo.w);
            ImGui::Text("Render Target Size [%d, %d]", gRenderWidth, gRenderHeight);
            ImGui::Separator();
            ImGui::Text("SGSR");
            ImGui::Checkbox("SGSR Active", &m_sgsr_active);
            ImGui::Checkbox("SGSR Edge Direction (remove banding)", &m_sgsr_edge_dir_active);

            static int super_resolution_selection = 0;
            std::array<const char*, 5> super_resolution_options = {"Native", "1.25x", "1.33x", "1.66x", "2x"};
            if (ImGui::BeginCombo("Upscaler", super_resolution_options[super_resolution_selection]))
            {
                for (int i = 0; i < super_resolution_options.size(); i++)
                {
                    if (ImGui::Selectable(super_resolution_options[i], i == super_resolution_selection))
                    {
                        super_resolution_selection = i;
                    }
                }

                ImGui::EndCombo();
            }

            switch (super_resolution_selection)
            {
            case 0: m_sgsr_factor = 1.0f; break;
            case 1: m_sgsr_factor = 0.8f; break;
            case 2: m_sgsr_factor = 0.75f; break;
            case 3: m_sgsr_factor = 0.6f; break;
            case 4: m_sgsr_factor = 0.5f; break;
            }
        }
        ImGui::End();

        return;
    }
}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t whichBuffer)
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // SGSR data
    {
        // Calculate the scale factor required to properly display the input image at render
        // resolution
        m_sgsr_scale_factor = static_cast<float>(gRenderHeight) / m_scene_texture_original_size.y;

        m_SGSRFragUniformData.TechniqueInfo.x = m_sgsr_active          ? 1.0f : 0.0f;
        m_SGSRFragUniformData.TechniqueInfo.y = m_sgsr_edge_dir_active ? 1.0f : 0.0f;
        m_SGSRFragUniformData.TechniqueInfo.z = m_scene_texture_original_size.x * m_sgsr_factor * m_sgsr_scale_factor;
        m_SGSRFragUniformData.TechniqueInfo.w = m_scene_texture_original_size.y * m_sgsr_factor * m_sgsr_scale_factor;

        m_SGSRFragUniformData.ViewportInfo.z = m_scene_texture_original_size.x;
        m_SGSRFragUniformData.ViewportInfo.w = m_scene_texture_original_size.y;
        m_SGSRFragUniformData.ViewportInfo.x = (1.0f / m_SGSRFragUniformData.ViewportInfo.z);
        m_SGSRFragUniformData.ViewportInfo.y = (1.0f / m_SGSRFragUniformData.ViewportInfo.w);
        
        UpdateUniformBuffer(pVulkan, m_SGSRFragUniform, m_SGSRFragUniformData);
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // Obtain the next swap chain image for the next frame.
    auto currentVulkanBuffer = pVulkan->SetNextBackBuffer();
    uint32_t whichBuffer     = currentVulkanBuffer.idx;

    // ********************************
    // Application Draw() - Begin
    // ********************************

    UpdateGui();

    // Update camera
    m_Camera.UpdateController(fltDiffTime * 10.0f, *m_CameraController);
    m_Camera.UpdateMatrices();
 
    // Update uniform buffers with latest data
    UpdateUniforms(whichBuffer);

    // First time through, wait for the back buffer to be ready
    std::span<const VkSemaphore> pWaitSemaphores = { &currentVulkanBuffer.semaphore, 1 };

    const VkPipelineStageFlags DefaultGfxWaitDstStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    // RP_SGSR
    {
        BeginRenderPass(whichBuffer, RP_SGSR, currentVulkanBuffer.swapchainPresentIdx);
        AddPassCommandBuffer(whichBuffer, RP_SGSR);
        EndRenderPass(whichBuffer, RP_SGSR);

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_SGSR, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_SGSR].PassCompleteSemaphore,1 });
        pWaitSemaphores = { &m_RenderPassData[RP_SGSR].PassCompleteSemaphore, 1 };
    }

    // RP_HUD
    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = GetGui()->Render(whichBuffer, m_RenderPassData[RP_HUD].RenderTarget[0].m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            BeginRenderPass(whichBuffer, RP_HUD, currentVulkanBuffer.swapchainPresentIdx);
            vkCmdExecuteCommands(m_RenderPassData[RP_HUD].PassCmdBuffer[whichBuffer].m_VkCommandBuffer, 1, &guiCommandBuffer);
            EndRenderPass(whichBuffer, RP_HUD);

            // Submit the commands to the queue.
            SubmitRenderPass(whichBuffer, RP_HUD, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_HUD].PassCompleteSemaphore,1 });
            pWaitSemaphores = { &m_RenderPassData[RP_HUD].PassCompleteSemaphore,1 };
        }
    }

    // Blit Results to the screen
    {
        BeginRenderPass(whichBuffer, RP_BLIT, currentVulkanBuffer.swapchainPresentIdx);
        AddPassCommandBuffer(whichBuffer, RP_BLIT);
        EndRenderPass(whichBuffer, RP_BLIT);

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_BLIT, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 }, currentVulkanBuffer.fence);
        pWaitSemaphores = { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 };
    }

    // Queue is loaded up, tell the driver to start processing
    pVulkan->PresentQueue(pWaitSemaphores, currentVulkanBuffer.swapchainPresentIdx);

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
void Application::BeginRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, uint32_t WhichSwapchainImage)
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();
    auto& renderPassData         = m_RenderPassData[whichPass];
    bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

    if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Reset())
    {
        LOGE("Pass (%d) command buffer Reset() failed !", whichPass);
    }

    if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Begin())
    {
        LOGE("Pass (%d) command buffer Begin() failed !", whichPass);
    }

    VkFramebuffer framebuffer = nullptr;
    switch (whichPass)
    {
    case RP_SGSR:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_HUD:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_BLIT:
        framebuffer = pVulkan->m_SwapchainBuffers[WhichSwapchainImage].framebuffer;
        break;
    default:
        framebuffer = nullptr;
        break;
    }

    assert(framebuffer != nullptr);

    VkRect2D passArea = {};
    passArea.offset.x = 0;
    passArea.offset.y = 0;
    passArea.extent.width  = bisSwapChainRenderPass ? pVulkan->m_SurfaceWidth  : renderPassData.RenderTarget[0].m_Width;
    passArea.extent.height = bisSwapChainRenderPass ? pVulkan->m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

    TextureFormat                  swapChainColorFormat = pVulkan->m_SurfaceFormat;
    auto                           swapChainColorFormats = std::span<const TextureFormat>({ &swapChainColorFormat, 1 });
    TextureFormat                  swapChainDepthFormat = pVulkan->m_SwapchainDepth.format;
    std::span<const TextureFormat> colorFormats         = bisSwapChainRenderPass ? swapChainColorFormats : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
    TextureFormat                  depthFormat          = bisSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

    VkClearColorValue clearColor = { renderPassData.PassSetup.ClearColor[0], renderPassData.PassSetup.ClearColor[1], renderPassData.PassSetup.ClearColor[2], renderPassData.PassSetup.ClearColor[3] };

    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].BeginRenderPass(
        passArea,
        0.0f,
        1.0f,
        { &clearColor , 1 },
        (uint32_t)colorFormats.size(),
        depthFormat != TextureFormat::UNDEFINED,
        m_RenderPassData[whichPass].RenderPass,
        bisSwapChainRenderPass,
        framebuffer,
        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}


//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffer(uint32_t whichBuffer, RENDER_PASS whichPass)
//-----------------------------------------------------------------------------
{
    if (m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].m_NumDrawCalls)
    {
        vkCmdExecuteCommands(m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].m_VkCommandBuffer, 1, &m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].m_VkCommandBuffer);
    }
}

//-----------------------------------------------------------------------------
void Application::EndRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass)
//-----------------------------------------------------------------------------
{
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].EndRenderPass();
}

//-----------------------------------------------------------------------------
void Application::SubmitRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, std::span<VkSemaphore> SignalSemaphores, VkFence CompletionFence)
//-----------------------------------------------------------------------------
{
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].End();
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].QueueSubmit(WaitSemaphores, WaitDstStageMasks, SignalSemaphores, CompletionFence);
}
