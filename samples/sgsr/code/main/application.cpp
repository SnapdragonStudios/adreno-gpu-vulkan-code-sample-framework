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
#include "material/vulkan/drawable.hpp"
#include "material/vulkan/shaderManager.hpp"
#include "material/vulkan/materialManager.hpp"
#include "material/vulkan/specializationConstantsLayout.hpp"
#include "memory/vulkan/drawIndirectBufferObject.hpp"
#include "memory/vulkan/indexBufferObject.hpp"
#include "memory/vulkan/vertexBufferObject.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/meshLoader.hpp"
#include "system/math_common.hpp"
#include "texture/textureManager.hpp"
#include "imgui.h"

#include <random>
#include <iostream>
#include <filesystem>

using namespace std::string_literals;

namespace
{
    static const std::array<const char*const, NUM_RENDER_PASSES> sRenderPassNames = { "RP_SGSR", "RP_HUD", "RP_BLIT" };

    glm::vec3 gCameraStartPos = glm::vec3(0.0f, 3.5f, 0.0f);
    glm::vec3 gCameraStartRot = glm::vec3(0.0f, 0.0f, 0.0f);

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

    if (!CreateRenderPasses())
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
        m_RenderPassData[whichPass] = {};
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
    m_ShaderManager = std::make_unique<ShaderManager>(*GetVulkan());
    m_ShaderManager->RegisterRenderPassNames(sRenderPassNames);

    m_MaterialManager = std::make_unique<MaterialManager>(*GetVulkan());

    LOGI("******************************");
    LOGI("Loading Shaders...");
    LOGI("******************************");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
            { tIdAndFilename { "Blit"s, "Blit.json"s },
              tIdAndFilename { "SGSR"s, "sgsr_shader_mobile.json"s }
            })
    {
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second, SHADER_DESTINATION_PATH))
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

    const TextureFormat MainColorType[] = { TextureFormat::B8G8R8A8_SRGB };
    const TextureFormat HudColorType[]  = { TextureFormat::B8G8R8A8_SRGB };

    LOGI("*************************************");
    LOGI("Creating Render Targets...");
    LOGI("*************************************");

    if (!m_RenderPassData[RP_SGSR].RenderTarget.Initialize(pVulkan, gRenderWidth, gRenderHeight, MainColorType, TextureFormat::UNDEFINED, Msaa::Samples1, "SGSR RT"))
    {
        LOGE( "Unable to create scene render target" );
        return false;
    }

    // Notice no depth on the HUD RT
    if (!m_RenderPassData[RP_HUD].RenderTarget.Initialize(pVulkan, gSurfaceWidth, gSurfaceHeight, HudColorType, TextureFormat::UNDEFINED, Msaa::Samples1, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateRenderPasses()
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();

    //                                            ColorInputUsage |               ClearDepthRenderPass | ColorOutputUsage |                     DepthOutputUsage |                     ClearColor
    m_RenderPassData[RP_SGSR].RenderPassSetup = { RenderPassInputUsage::Clear,    false,                 RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard,        {}};
    m_RenderPassData[RP_HUD].RenderPassSetup  = { RenderPassInputUsage::Clear,    false,                 RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard,        {}};
    m_RenderPassData[RP_BLIT].RenderPassSetup = { RenderPassInputUsage::DontCare, false,                 RenderPassOutputUsage::Present,        RenderPassOutputUsage::Discard,        {}};

    for (uint32_t whichPass = 0; whichPass < RP_BLIT; whichPass++)
    {
        std::span<const TextureFormat> colorFormats = m_RenderPassData[whichPass].RenderTarget.m_pLayerFormats;
        TextureFormat                  depthFormat  = m_RenderPassData[whichPass].RenderTarget.m_DepthFormat;

        const auto& passSetup = m_RenderPassData[whichPass].RenderPassSetup;
        auto& passData        = m_RenderPassData[whichPass];
        
        RenderPass renderPass;
        if (!vulkan.CreateRenderPass(
            { colorFormats },
            depthFormat,
            Msaa::Samples1,
            passSetup.ColorInputUsage,
            passSetup.ColorOutputUsage,
            passSetup.ClearDepthRenderPass,
            passSetup.DepthOutputUsage,
            renderPass))
        {
            return false;
        }
        Framebuffer<Vulkan> framebuffer;
        framebuffer.Initialize( vulkan,
                                renderPass,
                                passData.RenderTarget.m_ColorAttachments,
                                &passData.RenderTarget.m_DepthAttachment,
                                sRenderPassNames[whichPass] );
        passData.RenderContext.push_back({std::move(renderPass), {}/*pipeline*/, std::move(framebuffer), sRenderPassNames[whichPass]});
    }
    for (auto whichBuffer = 0; whichBuffer < vulkan.GetSwapchainBufferCount(); ++whichBuffer)
    {
        m_RenderPassData[RP_BLIT].RenderContext.push_back({ vulkan.m_SwapchainRenderPass.Copy(), {}, vulkan.GetSwapchainFramebuffer(whichBuffer), "RP_BLIT" });
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
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    const auto& hudRenderTarget = m_RenderPassData[RP_HUD].RenderTarget;
    m_Gui = std::make_unique<GuiImguiGfx>(*GetVulkan(), m_RenderPassData[RP_HUD].RenderContext[0].GetRenderPass().Copy());
    if (!m_Gui->Initialize(windowHandle, TextureFormat::R8G8B8A8_UNORM, hudRenderTarget.m_Width, hudRenderTarget.m_Height))
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

    const auto* pSGSRQuadShader         = m_ShaderManager->GetShader("SGSR"s);
    const auto* pBlitQuadShader         = m_ShaderManager->GetShader("Blit"s);
    if (!pSGSRQuadShader || !pBlitQuadShader)
    {
        return false;
    }
    
    LOGI("*******************************");
    LOGI("Loading and preparing assets...");
    LOGI("*******************************");

    m_sgsr_sampler = CreateSampler(*pVulkan, SamplerAddressMode::ClampEdge, SamplerFilter::Nearest, SamplerBorderColor::TransparentBlackFloat, 0.0f/*mipBias*/);
    if (m_sgsr_sampler.IsEmpty())
        return false;

    const PathManipulator_PrefixDirectory prefixTextureDir{ TEXTURE_DESTINATION_PATH };
    auto* sceneTexture = apiCast<tGfxApi>(m_TextureManager->GetOrLoadTexture("artifact.ktx"s, m_sgsr_sampler, prefixTextureDir));
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

    Mesh sgsrQuadMesh;
    MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pSGSRQuadShader->m_shaderDescription->m_vertexFormats, &sgsrQuadMesh );

    // SGSR Material
    auto sgsrQuadShaderMaterial = m_MaterialManager->CreateMaterial(*pSGSRQuadShader, pVulkan->m_SwapchainImageCount,
        [this, &sceneTexture](const std::string& texName) -> const MaterialManagerBase::tPerFrameTexInfo
        {
            if (texName == "SceneColor")
            {
                return { sceneTexture };
            }
            return {};
        },
        [this](const std::string& bufferName) -> PerFrameBufferVulkan
        {
            if (bufferName == "SceneInfo")
            {
                return { m_SGSRFragUniform.buf.GetVkBuffer() };
            }

            return {};
        }
        );

    m_SGSRQuadDrawable = std::make_unique<Drawable>(*pVulkan, std::move(sgsrQuadShaderMaterial));
    if (!m_SGSRQuadDrawable->Init( m_RenderPassData[RP_SGSR].RenderContext[0], std::move(sgsrQuadMesh)))
    {
        return false;
    }

    LOGI("*********************");
    LOGI("Creating Blit mesh...");
    LOGI("*********************");

    Mesh blitQuadMesh;
    MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pBlitQuadShader->m_shaderDescription->m_vertexFormats, &blitQuadMesh);

    // Blit Material
    auto blitQuadShaderMaterial = m_MaterialManager->CreateMaterial(*pBlitQuadShader, pVulkan->m_SwapchainImageCount,
        [this](const std::string& texName, MaterialManagerBase::tPerFrameTexInfo& texInfo)
        {
            if (texName == "Diffuse")
            {
                texInfo = { &m_RenderPassData[RP_SGSR].RenderTarget.m_ColorAttachments[0] };
            }
            else if (texName == "Overlay")
            {
                texInfo = { &m_RenderPassData[RP_HUD].RenderTarget.m_ColorAttachments[0] };
            }
            return;
        },
        [this](const std::string& bufferName, PerFrameBufferBase& buffers)
        {
        }
        );

    m_BlitQuadDrawable = std::make_unique<Drawable>(*pVulkan, std::move(blitQuadShaderMaterial));
    if (!m_BlitQuadDrawable->Init( m_RenderPassData[RP_BLIT].RenderContext[0], std::move( blitQuadMesh ) ))
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

    // Per frame Command Buffer
    char szName[256];
    for (uint32_t whichBuffer = 0; whichBuffer < m_CommandLists.size(); whichBuffer++)
    {
        sprintf( szName, "Command Buffer (%d of %d)", whichBuffer + 1, NUM_VULKAN_BUFFERS );
        if (!m_CommandLists[whichBuffer].Initialize( pVulkan, szName, CommandList::Type::Primary ))
        {
            return false;
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

    UpdateGui();

    // Update camera
    m_Camera.UpdateController(fltDiffTime * 10.0f, *m_CameraController);
    m_Camera.UpdateMatrices();
 
    // Update uniform buffers with latest data
    UpdateUniforms(whichBuffer);

    // Open the command list for recording commands
    auto& cmdBuffer = m_CommandLists[whichBuffer];
    cmdBuffer.Begin();

    // RP_SCENE
    {
        const auto& renderContext = m_RenderPassData[RP_SGSR].RenderContext[0];
        const fvk::VkRenderPassBeginInfo RPBeginInfo{ renderContext.GetRenderPassBeginInfo() };
        vkCmdBeginRenderPass(cmdBuffer, &RPBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        AddDrawableToCmdBuffers(*m_SGSRQuadDrawable.get(), &cmdBuffer, 1, 1, whichBuffer);
        vkCmdEndRenderPass(cmdBuffer);
    }

    // RP_HUD
    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = GetGui()->Render(whichBuffer, m_RenderPassData[RP_HUD].RenderTarget.m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            const auto& renderContext = m_RenderPassData[RP_HUD].RenderContext[0];
            const fvk::VkRenderPassBeginInfo RPBeginInfo{ renderContext.GetRenderPassBeginInfo() };
            vkCmdBeginRenderPass(cmdBuffer, &RPBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vkCmdExecuteCommands(cmdBuffer, 1, &guiCommandBuffer);
            vkCmdEndRenderPass(cmdBuffer);
        }
    }

    // Blit Results to the screen
    {
        const auto& renderContext = m_RenderPassData[RP_BLIT].RenderContext[0];
        const fvk::VkRenderPassBeginInfo RPBeginInfo{ renderContext.GetRenderPassBeginInfo() };
        vkCmdBeginRenderPass(cmdBuffer, &RPBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        AddDrawableToCmdBuffers(*m_BlitQuadDrawable.get(), &cmdBuffer, 1, 1, whichBuffer);
        vkCmdEndRenderPass(cmdBuffer);
    }

    // Close the command list
    cmdBuffer.End();

    // Submit and present the queue
    cmdBuffer.QueueSubmit( currentVulkanBuffer, pVulkan->m_RenderCompleteSemaphore );
    pVulkan->PresentQueue( pVulkan->m_RenderCompleteSemaphore, currentVulkanBuffer.swapchainPresentIdx );
}
