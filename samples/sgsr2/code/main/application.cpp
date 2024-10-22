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
#include "sgsr2_context.hpp"
#include "sgsr2_context_frag.hpp"

#include <random>
#include <iostream>
#include <filesystem>

VAR( char*,     gSceneAssetPath,    "Media\\Meshes\\Museum.gltf", kVariableNonpersistent );
VAR( char*,     gSceneTexturePath,  "Media\\", kVariableNonpersistent );
VAR( float,     gSceneScale,        1.0f,  kVariableNonpersistent );
VAR( glm::vec3, gCameraStartPos,    glm::vec3(26.48f, 20.0f, -5.21f), kVariableNonpersistent );
VAR( glm::vec3, gCameraStartRot,    glm::vec3(0.0f, 110.0f, 0.0f), kVariableNonpersistent );    // in degrees
VAR( float,     gNearPlane,         0.3f,   kVariableNonpersistent );
VAR( float,     gFarPlane,          1800.0f, kVariableNonpersistent );
VAR( float,     gFov,               45.0f, kVariableNonpersistent );
VAR( int,       gUpscaleMode,       1, kVariableNonpersistent );

namespace
{
    static constexpr std::array<const char*, NUM_RENDER_PASSES> sRenderPassNames = { "RP_SCENE", "RP_HUD", "RP_BLIT" };

    float   gNormalAmount = 0.3f;
    float   gNormalMirrorReflectAmount = 0.05f;
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

Application::Application() 
    : ApplicationHelperBase()
{
}

Application::~Application()
{
}

//-----------------------------------------------------------------------------
void Application::PreInitializeSetVulkanConfiguration( Vulkan::AppConfiguration& appConfig)
//-----------------------------------------------------------------------------
{
    appConfig.SwapchainDepthFormat = TextureFormat::UNDEFINED;
}

//-----------------------------------------------------------------------------
bool Application::Initialize(uintptr_t windowHandle, uintptr_t hInstance)
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize( windowHandle, hInstance ))
    {
        return false;
    }

    // If we dont explicitly set the render width/height (in app_config.txt) use the gUpscaleFactor to set scaling.
    const float gUpscaleFactor = 0.66667f;
    const auto* var = GetVariable( "gRenderWidth" );
    if (nullptr == var || 0 == (var->GetFlags() & kVariableModified))
        gRenderWidth = gSurfaceWidth * gUpscaleFactor;
    var = GetVariable( "gRenderHeight" );
    if (nullptr == var || 0 == (var->GetFlags() & kVariableModified))
        gRenderHeight = gSurfaceHeight * gUpscaleFactor;

    if (!InitializeCamera())
    {
        return false;
    }
    
    if (!InitializeLights())
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

    if (!InitSGSR2Context())
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
    auto* const pVulkan = GetVulkan();

    // Uniform Buffers
    ReleaseUniformBuffer(pVulkan, m_ObjectVertUniform);
    ReleaseUniformBuffer(pVulkan, m_LightUniform);
     
    for (auto& [hash, objectUniform] : m_ObjectFragUniforms)
    {
        ReleaseUniformBuffer(pVulkan, &objectUniform.objectFragUniform);
    }

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
    m_SceneDrawables.clear();
    m_BlitQuadDrawable.reset();

    // Internal
    m_ShaderManager.reset();
    m_MaterialManager.reset();
    m_CameraController.reset();
    m_AssetManager.reset();

    ApplicationHelperBase::Destroy();
}

//-----------------------------------------------------------------------------
bool Application::InitializeLights()
//-----------------------------------------------------------------------------
{
    m_LightUniformData.AmbientColor = glm::vec4{1.0f,1.0f,1.0f,1.0f};

    m_LightUniformData.SpotLights_pos[0] = glm::vec4(-6.900000f, 32.299999f, -1.900000f, 1.0f);
    m_LightUniformData.SpotLights_pos[1] = glm::vec4(3.300000f, 26.900000f, 7.600000f, 1.0f);
    m_LightUniformData.SpotLights_pos[2] = glm::vec4(12.100000f, 41.400002f, -2.800000f, 1.0f);
    m_LightUniformData.SpotLights_pos[3] = glm::vec4(-5.400000f, 18.500000f, 28.500000f, 1.0f);

    m_LightUniformData.SpotLights_dir[0] = glm::vec4(-0.534696f, -0.834525f, 0.132924f, 0.0f);
    m_LightUniformData.SpotLights_dir[1] = glm::vec4(0.000692f, -0.197335f, 0.980336f, 0.0f);
    m_LightUniformData.SpotLights_dir[2] = glm::vec4(0.985090f, -0.172016f, 0.003000f, 0.0f);
    m_LightUniformData.SpotLights_dir[3] = glm::vec4(0.674125f, -0.295055f, -0.677125f, 0.0f);

    m_LightUniformData.SpotLights_color[0] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 3.000000f);
    m_LightUniformData.SpotLights_color[1] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 3.500000f);
    m_LightUniformData.SpotLights_color[2] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 2.000000f);
    m_LightUniformData.SpotLights_color[3] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 2.800000f);

    return true;
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
    m_Camera.SetFov(gFov * TO_RADIANS);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    // Camera Controller //

#if defined(OS_ANDROID)
    typedef CameraControllerTouch           tCameraController;
#else
    typedef CameraController                tCameraController;
#endif

    auto cameraController = std::make_unique<tCameraController>();
    if (!cameraController->Initialize(gSurfaceWidth, gSurfaceHeight))
    {
        return false;
    }

    m_CameraController = std::move(cameraController);
    m_CameraController->SetMoveSpeed( 0.4f );

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
              tIdAndFilename { "SceneOpaque", "Media\\Shaders\\SceneOpaque.json" },
              tIdAndFilename { "SceneTransparent", "Media\\Shaders\\SceneTransparent.json" },
              tIdAndFilename { "sgsr2", "Media\\Shaders\\sgsr2.json" },
              tIdAndFilename { "sgsr2_frag", "Media\\Shaders\\sgsr2_frag.json" },
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
    auto* const pVulkan = GetVulkan();

    LOGI("**************************");
    LOGI("Creating Render Targets...");
    LOGI("**************************");

    // TextureFormat vkDesiredDepthFormat = vulkan.GetBestSurfaceDepthFormat();
    TextureFormat desiredDepthFormat = TextureFormat::D24_UNORM_S8_UINT;

    const TextureFormat SceneColorTypes[] = 
    {
        TextureFormat::R8G8B8A8_SRGB, // Color
        TextureFormat::R16G16_SFLOAT  // Velocity
    };
    const TextureFormat HudColorType[]   = { TextureFormat::R8G8B8A8_SRGB };

    if (!m_RenderPassData[RP_SCENE].RenderTarget.Initialize(pVulkan, gRenderWidth, gRenderHeight, SceneColorTypes, desiredDepthFormat, VK_SAMPLE_COUNT_1_BIT, "Scene RT"))
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

    auto* const pVulkan = GetVulkan();

    if (!CreateUniformBuffer(pVulkan, m_ObjectVertUniform))
    {
        return false;
    }

    if (!CreateUniformBuffer(pVulkan, m_LightUniform))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitAllRenderPasses()
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();

    //                                             ColorInputUsage |               ClearDepthRenderPass | ColorOutputUsage |                     DepthOutputUsage |                     ClearColor
    m_RenderPassData[RP_SCENE].RenderPassSetup = { RenderPassInputUsage::Clear,    true,                  RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::StoreReadOnly,  {}};
    m_RenderPassData[RP_HUD].RenderPassSetup   = { RenderPassInputUsage::Clear,    false,                 RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard,        {}};
    m_RenderPassData[RP_BLIT].RenderPassSetup  = { RenderPassInputUsage::DontCare, true,                  RenderPassOutputUsage::Present,        RenderPassOutputUsage::Discard,        {}};

    TextureFormat surfaceFormat = vulkan.m_SurfaceFormat;
    auto swapChainColorFormat = std::span<const TextureFormat>({ &surfaceFormat, 1 });
    auto swapChainDepthFormat = vulkan.m_SwapchainDepth.format;

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        bool isSwapChainRenderPass = whichPass == RP_BLIT;

        std::span<const TextureFormat> colorFormats = isSwapChainRenderPass ? swapChainColorFormat : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
        TextureFormat                  depthFormat  = isSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

        const auto& passSetup = m_RenderPassData[whichPass].RenderPassSetup;
        
        if (!vulkan.CreateRenderPass(
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
bool Application::InitSGSR2Context()
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();
    m_sgsr2_context = std::make_unique<SGSR2::Context>();
    m_sgsr2_context_frag = std::make_unique<SGSR2_Frag::Context>();

    {
        SGSR2::UpscalerConfiguration upscaler_config;
        upscaler_config.render_size = glm::uvec2( gRenderWidth, gRenderHeight );
        upscaler_config.display_size = glm::uvec2( gSurfaceWidth, gSurfaceHeight );

        SGSR2::InputImages input_images;
        input_images.color = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[0];
        input_images.opaque_color = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[0];
        input_images.velocity = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[1];
        input_images.depth = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_DepthAttachment;

        if (!m_sgsr2_context->Initialize(
            vulkan,
            *m_ShaderManager,
            *m_MaterialManager,
            upscaler_config,
            input_images ))
        {
            return false;
        }
    }

    {
        SGSR2_Frag::UpscalerConfiguration upscaler_config;
        upscaler_config.render_size = glm::uvec2( gRenderWidth, gRenderHeight );
        upscaler_config.display_size = glm::uvec2( gSurfaceWidth, gSurfaceHeight );

        SGSR2_Frag::InputImages input_images;
        input_images.color = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[0];
        input_images.opaque_color = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[0];
        input_images.velocity = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[1];
        input_images.depth = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_DepthAttachment;

        if (!m_sgsr2_context_frag->Initialize(
            vulkan,
            *m_ShaderManager,
            *m_MaterialManager,
            upscaler_config,
            input_images ))
        {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();

    LOGI("***********************");
    LOGI("Initializing Meshes... ");
    LOGI("***********************");

    const auto* pSceneOpaqueShader      = m_ShaderManager->GetShader("SceneOpaque");
    const auto* pSceneTransparentShader = m_ShaderManager->GetShader("SceneTransparent");
    const auto* pBlitQuadShader         = m_ShaderManager->GetShader("Blit");
    if (!pSceneOpaqueShader || !pSceneTransparentShader || !pBlitQuadShader)
    {
        return false;
    }
    
    // Bias the texture mips to be as-if the scene was rendering at the upscaled resolution.
    {
        float mipBias = std::max( std::log2( float( gRenderWidth ) / float( gSurfaceWidth ) ), std::log2( float( gRenderHeight ) / float( gSurfaceHeight ) ) );
        ReleaseSampler( vulkan, &m_SamplerRepeat );
        m_SamplerRepeat = CreateSampler( vulkan, SamplerAddressMode::Repeat, SamplerFilter::Linear, SamplerBorderColor::TransparentBlackFloat, mipBias );
        if (m_SamplerRepeat.IsEmpty())
            return false;
    }

    LOGI("***********************************");
    LOGI("Loading and preparing the museum...");
    LOGI("***********************************");

    m_TextureManager->SetDefaultFilenameManipulators(PathManipulator_PrefixDirectory{ "Media\\" }, PathManipulator_ChangeExtension{ ".ktx" });

    auto* whiteTexture         = m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\white_d.ktx", m_SamplerRepeat);
    auto* blackTexture         = m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\black_d.ktx", m_SamplerRepeat);
    auto* normalDefaultTexture = m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\normal_default.ktx", m_SamplerRepeat);

    if (!whiteTexture || !blackTexture || !normalDefaultTexture)
    {
        LOGE("Failed to load supporting textures");
        return false;
    }

    auto UniformBufferLoader = [&](const ObjectMaterialParameters& objectMaterialParameters) -> ObjectMaterialParameters&
    {
        auto hash = objectMaterialParameters.GetHash();

        auto iter = m_ObjectFragUniforms.try_emplace(hash, ObjectMaterialParameters());
        if (iter.second)
        {
            iter.first->second.objectFragUniformData = objectMaterialParameters.objectFragUniformData;
            if (!CreateUniformBuffer(&vulkan, iter.first->second.objectFragUniform))
            {
                LOGE("Failed to create object uniform buffer");
            }
        }

        return iter.first->second;
    };

    auto MaterialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef)->std::optional<Material>
    {
        const PathManipulator_PrefixDirectory prefixTextureDir{gSceneTexturePath};
        const PathManipulator_ChangeExtension changeTextureExt{".ktx"};

        auto* diffuseTexture           = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.diffuseFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        auto* normalTexture            = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.bumpFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        auto* emissiveTexture          = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.emissiveFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        auto* metallicRoughnessTexture = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.specMapFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        bool alphaCutout               = materialDef.alphaCutout;
        bool transparent               = materialDef.transparent;

        const Shader* targetShader = transparent ? pSceneTransparentShader : pSceneOpaqueShader;

        ObjectMaterialParameters objectMaterial;
        objectMaterial.objectFragUniformData.Color.r = static_cast<float>(materialDef.baseColorFactor[0]);
        objectMaterial.objectFragUniformData.Color.g = static_cast<float>(materialDef.baseColorFactor[1]);
        objectMaterial.objectFragUniformData.Color.b = static_cast<float>(materialDef.baseColorFactor[2]);
        objectMaterial.objectFragUniformData.Color.a = static_cast<float>(materialDef.baseColorFactor[3]);
        objectMaterial.objectFragUniformData.ORM.b   = static_cast<float>(materialDef.metallicFactor);
        objectMaterial.objectFragUniformData.ORM.g   = static_cast<float>(materialDef.roughnessFactor);

        if (diffuseTexture == nullptr || normalTexture == nullptr)
        {
            return std::nullopt;
        }

        auto shaderMaterial = m_MaterialManager->CreateMaterial(vulkan, *targetShader, NUM_VULKAN_BUFFERS,
            [&](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
            {
                if (texName == "Diffuse")
                {
                    return { diffuseTexture ? diffuseTexture : whiteTexture };
                }
                if (texName == "Normal")
                {
                    return { normalTexture ? normalTexture : normalDefaultTexture };
                }
                if (texName == "Emissive")
                {
                    return { emissiveTexture ? emissiveTexture : blackTexture };
                }
                if (texName == "MetallicRoughness")
                {
                    return { metallicRoughnessTexture ? metallicRoughnessTexture : blackTexture };
                }

                return {};
            },
            [&](const std::string& bufferName) -> tPerFrameVkBuffer
            {
                if (bufferName == "Vert")
                {
                    return {m_ObjectVertUniform.vkBuffers.begin(), m_ObjectVertUniform.vkBuffers.end()};
                }
                else if (bufferName == "Frag")
                {
                    return { UniformBufferLoader(objectMaterial).objectFragUniform.buf.GetVkBuffer() };
                }
                else if (bufferName == "Light")
                {
                    return {m_LightUniform.vkBuffers.begin(), m_LightUniform.vkBuffers.end()};
                }

                return {};
            }
            );

        return shaderMaterial;
    };


    const auto loaderFlags = 0; // No instancing
    const bool ignoreTransforms = (loaderFlags & DrawableLoader::LoaderFlags::IgnoreHierarchy) != 0;

    MeshLoaderModelSceneSanityCheck meshSanityCheckProcessor(gSceneAssetPath);
    MeshObjectIntermediateGltfProcessor meshObjectProcessor(gSceneAssetPath, ignoreTransforms, glm::vec3(1.0f,1.0f,1.0f));
    CameraGltfProcessor meshCameraProcessor{};

    if (!MeshLoader::LoadGltf(*m_AssetManager, gSceneAssetPath, meshSanityCheckProcessor, meshObjectProcessor, meshCameraProcessor) ||
        !DrawableLoader::CreateDrawables(vulkan,
                                        std::move(meshObjectProcessor.m_meshObjects),
                                        { &m_RenderPassData[RP_SCENE].RenderPass, 1 },
                                        &sRenderPassNames[RP_SCENE],
                                        MaterialLoader,
                                        m_SceneDrawables,
                                        {},    // RenderPassMultisample
                                        loaderFlags,
                                        {}))   // RenderPassSubpasses
    {
        LOGE("Error Loading the museum gltf file");
        LOGI("Please verify if you have all required assets on the sample media folder");
        LOGI("If you are running on Android, don't forget to run the `02_CopyMediaToDevice.bat` script to copy all media files into the device memory");
        return false;
    }

    if (!meshCameraProcessor.m_cameras.empty())
    {
        const auto& camera = meshCameraProcessor.m_cameras[0];
        m_Camera.SetPosition(camera.Position, camera.Orientation);
    }

    LOGI("*********************");
    LOGI("Creating Quad mesh...");
    LOGI("*********************");

    MeshObject blitQuadMesh;
    if (!MeshHelper::CreateMesh<Vulkan>( vulkan.GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pBlitQuadShader->m_shaderDescription->m_vertexFormats, &blitQuadMesh ))
    {
        return false;
    }

    // Blit Material
    auto blitQuadShaderMaterial = m_MaterialManager->CreateMaterial(vulkan, *pBlitQuadShader, 2,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
        {
            if (texName == "Diffuse")
            {
                return { m_sgsr2_context->GetSceneColorOutput() };
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

    m_BlitQuadDrawable = std::make_unique<Drawable>(vulkan, std::move(blitQuadShaderMaterial));
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

    auto* const pVulkan = GetVulkan();

    auto GetPassName = [](uint32_t whichPass)
    {
        if (whichPass >= sRenderPassNames.size())
        {
            LOGE("GetPassName() called with unknown pass (%d)!", whichPass);
            return "RP_UNKNOWN";
        }

        return sRenderPassNames[whichPass];
    };

    m_RenderPassData[RP_SCENE].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
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

    for (auto& command_buffer : m_UpscaleAndPostCommandList)
    {
        command_buffer.Initialize( pVulkan, "SGSR2 and Post Process Command List", VK_COMMAND_BUFFER_LEVEL_PRIMARY );
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

    auto& vulkan = *GetVulkan();

    // Begin recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData         = m_RenderPassData[whichPass];
        bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];

            uint32_t targetWidth  = bisSwapChainRenderPass ? vulkan.m_SurfaceWidth : renderPassData.RenderTarget[0].m_Width;
            uint32_t targetHeight = bisSwapChainRenderPass ? vulkan.m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

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
            VkFramebuffer whichFramebuffer = bisSwapChainRenderPass ? vulkan.m_SwapchainBuffers[whichBuffer].framebuffer : renderPassData.RenderTarget[0].m_FrameBuffer;

            // Objects (can render into any pass except Blit)
            if (!cmdBufer.Begin(whichFramebuffer, whichRenderPass, bisSwapChainRenderPass))
            {
                return false;
            }
            vkCmdSetViewport(cmdBufer.m_VkCommandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBufer.m_VkCommandBuffer, 0, 1, &scissor);
        }
    }
    
    // Scene drawables
    for (const auto& sceneDrawable : m_SceneDrawables)
    {
        AddDrawableToCmdBuffers(sceneDrawable, m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.size()));
    }

    // Blit quad drawable
    AddDrawableToCmdBuffers(*m_BlitQuadDrawable.get(), m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.size()));

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
            ImGui::Text("FPS: %.1f", m_CurrentFPS);
            auto rotationDeg = glm::eulerAngles(m_Camera.Rotation()) * TO_DEGREES;
            ImGui::Text("Camera [%f, %f, %f] [%f, %f, %f]", m_Camera.Position().x, m_Camera.Position().y, m_Camera.Position().z, rotationDeg.x, rotationDeg.y, rotationDeg.z);
            static std::array<const char* const, 3> sUpscaleModeNames{"Bilerp", "SGSR2 3pass", "SGSR2 fragment"};
            ImGui::ListBox( "UpscaleMode", &gUpscaleMode, sUpscaleModeNames.data(), (int)sUpscaleModeNames.size() );
            ImGui::DragFloat3("Sun Dir", &m_LightUniformData.LightDirection.x, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat3("Sun Color", &m_LightUniformData.LightColor.x, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Sun Intensity", &m_LightUniformData.LightColor.w, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat3("Ambient Color", &m_LightUniformData.AmbientColor.x, 0.01f, 0.0f, 1.0f);

            for (int i = 0; i < NUM_SPOT_LIGHTS; i++)
            {
                std::string childName = std::string("Spot Light ").append(std::to_string(i+1));
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", childName.c_str());

                if (ImGui::CollapsingHeader(childName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
                {
                    ImGui::PushID(i);

                    ImGui::DragFloat3("Pos", &m_LightUniformData.SpotLights_pos[i].x, 0.1f);
                    ImGui::DragFloat3("Dir", &m_LightUniformData.SpotLights_dir[i].x, 0.01f, -1.0f, 1.0f);
                    ImGui::DragFloat3("Color", &m_LightUniformData.SpotLights_color[i].x, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Intensity", &m_LightUniformData.SpotLights_color[i].w, 0.1f, 0.0f, 100.0f);

                    ImGui::PopID();
                }

                ImDrawList* list = ImGui::GetWindowDrawList();

                glm::vec3 LightDirNotNormalized = m_LightUniformData.SpotLights_dir[i];
                LightDirNotNormalized = glm::normalize(LightDirNotNormalized);
                m_LightUniformData.SpotLights_dir[i] = glm::vec4(LightDirNotNormalized, 0.0f);
            }

            glm::vec3 LightDirNotNormalized   = m_LightUniformData.LightDirection;
            LightDirNotNormalized             = glm::normalize(LightDirNotNormalized);
            m_LightUniformData.LightDirection = glm::vec4(LightDirNotNormalized, 0.0f);
        }
        ImGui::End();

        return;
    }
}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t whichBuffer)
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    // Vert data
    {
        glm::mat4 LocalModel = glm::mat4(1.0f);
        LocalModel           = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        LocalModel           = glm::scale(LocalModel, glm::vec3(gSceneScale));
        glm::mat4 LocalMVP   = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix() * LocalModel;

        m_ObjectVertUniformData.MVPMatrix   = LocalMVP;
        m_ObjectVertUniformData.ModelMatrix = LocalModel;
        UpdateUniformBuffer(pVulkan, m_ObjectVertUniform, m_ObjectVertUniformData, whichBuffer);
    }

    // Frag data
    for (auto& [hash, objectUniform] : m_ObjectFragUniforms)
    {
        UpdateUniformBuffer(pVulkan, objectUniform.objectFragUniform, objectUniform.objectFragUniformData);
    }

    // Light data
    {
        glm::mat4 CameraViewInv       = glm::inverse(m_Camera.ViewMatrix());
        glm::mat4 CameraProjection    = m_Camera.ProjectionMatrix();
        glm::mat4 CameraProjectionInv = glm::inverse(CameraProjection);

        m_LightUniformData.ProjectionInv     = CameraProjectionInv;
        m_LightUniformData.ViewInv           = CameraViewInv;
        m_LightUniformData.ViewProjectionInv = glm::inverse(CameraProjection * m_Camera.ViewMatrix());
        m_LightUniformData.ProjectionInvW    = glm::vec4(CameraProjectionInv[0].w, CameraProjectionInv[1].w, CameraProjectionInv[2].w, CameraProjectionInv[3].w);
        m_LightUniformData.CameraPos         = glm::vec4(m_Camera.Position(), 0.0f);

        UpdateUniformBuffer(pVulkan, m_LightUniform, m_LightUniformData, whichBuffer);
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();

    // Obtain the next swap chain image for the next frame.
    auto currentVulkanBuffer = vulkan.SetNextBackBuffer();
    uint32_t whichBuffer     = currentVulkanBuffer.idx;

    // ********************************
    // Application Draw() - Begin
    // ********************************

    UpdateGui();

    const auto prev_view_proj_matrix = m_Camera.ProjectionMatrixNoJitter() * m_Camera.ViewMatrix();

    // Update camera
    m_Camera.SetJitter( m_sgsr2_context->GetJitter() / glm::vec2(gRenderWidth, gRenderHeight));
    m_Camera.UpdateController(fltDiffTime, *m_CameraController);
    m_Camera.UpdateMatrices();
    
    // Update uniform buffers with latest data
    UpdateUniforms(whichBuffer);
    m_sgsr2_context->UpdateUniforms( *GetVulkan(), m_Camera, prev_view_proj_matrix );
    m_sgsr2_context_frag->UpdateUniforms( *GetVulkan(), m_Camera, prev_view_proj_matrix );

    // First time through, wait for the back buffer to be ready
    std::span<const VkSemaphore> pWaitSemaphores = { &currentVulkanBuffer.semaphore, 1 };

    const VkPipelineStageFlags DefaultGfxWaitDstStageMasks[] = 
    { 
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
    };

    // RP_SCENE
    {
        BeginRenderPass(whichBuffer, RP_SCENE, currentVulkanBuffer.swapchainPresentIdx);
        AddPassCommandBuffer(whichBuffer, RP_SCENE);
        EndRenderPass(whichBuffer, RP_SCENE);

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_SCENE, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_SCENE].PassCompleteSemaphore,1 });
        pWaitSemaphores = { &m_RenderPassData[RP_SCENE].PassCompleteSemaphore, 1 };
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

    // SGSR 2.0

    {
        auto& command_buffer = m_UpscaleAndPostCommandList[whichBuffer];
        command_buffer.Begin();

        // Upscale
        switch (gUpscaleMode) {
        case 0:
        default:
            break;
        case 1:
            m_sgsr2_context->Dispatch( *GetVulkan(), command_buffer );
            break;
        case 2:
            m_sgsr2_context_frag->Dispatch( *GetVulkan(), command_buffer );
            break;
        }

        VkRect2D passArea = {};
        passArea.offset.x = 0;
        passArea.offset.y = 0;
        passArea.extent.width = vulkan.m_SurfaceWidth;
        passArea.extent.height = vulkan.m_SurfaceHeight;

        VkClearColorValue clearColor{.int32 = {}};

        command_buffer.BeginRenderPass(
            passArea,
            0.0f,
            1.0f,
            {&clearColor , 1},
            (uint32_t)1,
            false/*has depth*/,
            m_RenderPassData[RP_BLIT].RenderPass,
            true/*swapchain*/,
            vulkan.m_SwapchainBuffers[currentVulkanBuffer.swapchainPresentIdx].framebuffer,
            VK_SUBPASS_CONTENTS_INLINE );

        const TextureVulkan* pUpscaledBuffer = nullptr;
        switch (gUpscaleMode) {
        case 0:
        default:
            pUpscaledBuffer = &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[0];
            break;
        case 1:
            pUpscaledBuffer = apiCast<Vulkan>( m_sgsr2_context->GetSceneColorOutput() );
            break;
        case 2:
            pUpscaledBuffer = apiCast<Vulkan>( m_sgsr2_context_frag->GetSceneColorOutput() );
            break;

        }
        static uint32_t outputBufferFlipFlop = 0;
        outputBufferFlipFlop ^= 1;
        m_BlitQuadDrawable->GetMaterial().UpdateDescriptorSetBinding(outputBufferFlipFlop, "Diffuse", *pUpscaledBuffer);
        AddDrawableToCmdBuffers( *m_BlitQuadDrawable, &command_buffer, 1, 1, outputBufferFlipFlop );

        command_buffer.EndRenderPass();
        command_buffer.End();

        // Submit the commands to the queue.
        command_buffer.QueueSubmit( pWaitSemaphores, DefaultGfxWaitDstStageMasks, {&m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1}, currentVulkanBuffer.fence );
        pWaitSemaphores = { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 };
    }

    // Queue is loaded up, tell the driver to start processing
    vulkan.PresentQueue(pWaitSemaphores, currentVulkanBuffer.swapchainPresentIdx);
    vulkan.WaitUntilIdle();

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
void Application::BeginRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, uint32_t WhichSwapchainImage)
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();
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
    case RP_SCENE:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_HUD:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_BLIT:
        framebuffer = vulkan.m_SwapchainBuffers[WhichSwapchainImage].framebuffer;
        break;
    default:
        framebuffer = nullptr;
        break;
    }

    assert(framebuffer != nullptr);

    VkRect2D passArea = {};
    passArea.offset.x = 0;
    passArea.offset.y = 0;
    passArea.extent.width  = bisSwapChainRenderPass ? vulkan.m_SurfaceWidth  : renderPassData.RenderTarget[0].m_Width;
    passArea.extent.height = bisSwapChainRenderPass ? vulkan.m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

    TextureFormat                  swapChainColorFormat = vulkan.m_SurfaceFormat;
    auto                           swapChainColorFormats = std::span<const TextureFormat>({ &swapChainColorFormat, 1 });
    TextureFormat                  swapChainDepthFormat = vulkan.m_SwapchainDepth.format;
    std::span<const TextureFormat> colorFormats         = bisSwapChainRenderPass ? swapChainColorFormats : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
    TextureFormat                  depthFormat          = bisSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

    VkClearColorValue clearColor = { renderPassData.RenderPassSetup.ClearColor[0], renderPassData.RenderPassSetup.ClearColor[1], renderPassData.RenderPassSetup.ClearColor[2], renderPassData.RenderPassSetup.ClearColor[3] };

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
