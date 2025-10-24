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
#include "material/vulkan/computable.hpp"
#include "material/vulkan/drawable.hpp"
#include "material/drawableLoader.hpp"
#include "material/vulkan/materialManager.hpp"
#include "material/vulkan/shaderModule.hpp"
#include "material/vulkan/shaderManager.hpp"
#include "material/vulkan/specializationConstantsLayout.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/meshLoader.hpp"
#include "system/math_common.hpp"
#include "texture/vulkan/textureManager.hpp"
#include "imgui.h"
#include "sgsr2_context.hpp"
#include "sgsr2_context_frag.hpp"

#include <random>
#include <iostream>
#include <filesystem>

VAR( char*,     gSceneAssetModel,   "SteamPunkSauna.gltf", kVariableNonpersistent );
VAR( float,     gSceneScale,        1.0f,  kVariableNonpersistent );
VAR( glm::vec3, gCameraStartPos,    glm::vec3(0.0f, 3.5f, 0.0f), kVariableNonpersistent );
VAR( glm::vec3, gCameraStartRot,    glm::vec3(0.0f, 0.0f, 0.0f), kVariableNonpersistent );    // in degrees
VAR( float,     gNearPlane,         0.3f,   kVariableNonpersistent );
VAR( float,     gFarPlane,          1800.0f, kVariableNonpersistent );
VAR( float,     gFov,               45.0f, kVariableNonpersistent );
VAR( int,       gUpscaleMode,       1, kVariableNonpersistent );

namespace
{
    static constexpr std::array<const char*const, NUM_RENDER_PASSES> sRenderPassNames = { "RP_SCENE", "RP_HUD", "RP_BLIT" };

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
        for (auto& cmdBuffer : m_RenderPassData[whichPass].ObjectsCmdBuffer)
        {
            cmdBuffer.Release();
        }

        m_RenderPassData[whichPass].RenderTarget.Release();
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
    m_ShaderManager = std::make_unique<ShaderManager>(*GetVulkan());
    m_ShaderManager->RegisterRenderPassNames(sRenderPassNames);

    m_MaterialManager = std::make_unique<MaterialManager>(*GetVulkan());

    LOGI("******************************");
    LOGI("Loading Shaders...");
    LOGI("******************************");
    
    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
            { 
                tIdAndFilename { "Blit",             "Blit.json" },
                tIdAndFilename { "SceneOpaque",      "SceneOpaque.json" },
                tIdAndFilename { "SceneTransparent", "SceneTransparent.json" },
                tIdAndFilename { "sgsr2",            "sgsr2.json" },
                tIdAndFilename { "sgsr2_frag",       "sgsr2_frag.json" },
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

    if (!m_RenderPassData[RP_SCENE].RenderTarget.Initialize(pVulkan, gRenderWidth, gRenderHeight, SceneColorTypes, desiredDepthFormat, Msaa::Samples1, "Scene RT"))
    {
        LOGE("Unable to create scene render target");
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

    for (uint32_t whichPass = 0; whichPass < RP_BLIT; whichPass++)
    {
        std::span<const TextureFormat> colorFormats = m_RenderPassData[whichPass].RenderTarget.m_pLayerFormats;
        TextureFormat                  depthFormat  = m_RenderPassData[whichPass].RenderTarget.m_DepthFormat;

        const auto& passSetup = m_RenderPassData[whichPass].RenderPassSetup;
        auto& passData = m_RenderPassData[whichPass];
        
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
        m_RenderPassData[RP_BLIT].RenderContext.push_back( {vulkan.m_SwapchainRenderPass.Copy(), {}, vulkan.GetSwapchainFramebuffer( whichBuffer ), "RP_BLIT"} );
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
        input_images.color = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[0];
        input_images.opaque_color = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[0];
        input_images.velocity = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[1];
        input_images.depth = &m_RenderPassData[RP_SCENE].RenderTarget.m_DepthAttachment;

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
        input_images.color = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[0];
        input_images.opaque_color = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[0];
        input_images.velocity = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[1];
        input_images.depth = &m_RenderPassData[RP_SCENE].RenderTarget.m_DepthAttachment;

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

    m_TextureManager->SetDefaultFilenameManipulators(PathManipulator_PrefixDirectory(TEXTURE_DESTINATION_PATH));

    const PathManipulator_PrefixDirectory prefixTextureDir{ TEXTURE_DESTINATION_PATH };
    auto* whiteTexture         = m_TextureManager->GetOrLoadTexture("white_d.ktx", m_SamplerRepeat, prefixTextureDir);
    auto* blackTexture         = m_TextureManager->GetOrLoadTexture("black_d.ktx", m_SamplerRepeat, prefixTextureDir);
    auto* normalDefaultTexture = m_TextureManager->GetOrLoadTexture("normal_default.ktx", m_SamplerRepeat, prefixTextureDir);
    
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
        const PathManipulator_PrefixDirectory prefixTextureDir{ TEXTURE_DESTINATION_PATH };
        const PathManipulator_ChangeExtension changeTextureExt{".ktx"};

        auto* diffuseTexture           = m_TextureManager->GetOrLoadTexture(materialDef.diffuseFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        auto* normalTexture            = m_TextureManager->GetOrLoadTexture(materialDef.bumpFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        auto* emissiveTexture          = m_TextureManager->GetOrLoadTexture(materialDef.emissiveFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        auto* metallicRoughnessTexture = m_TextureManager->GetOrLoadTexture(materialDef.specMapFilename, m_SamplerRepeat, prefixTextureDir, changeTextureExt);
        bool alphaCutout               = materialDef.alphaCutout;
        bool transparent               = materialDef.transparent;

        const auto* targetShader = transparent ? pSceneTransparentShader : pSceneOpaqueShader;

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

        auto shaderMaterial = m_MaterialManager->CreateMaterial(*targetShader, NUM_VULKAN_BUFFERS,
            [&](const std::string& texName) -> const MaterialManager::tPerFrameTexInfo
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
            [&](const std::string& bufferName) -> PerFrameBufferVulkan
            {
                if (bufferName == "Vert")
                {
                    return {m_ObjectVertUniform.bufferHandles.begin(), m_ObjectVertUniform.bufferHandles.end()};
                }
                else if (bufferName == "Frag")
                {
                    return { UniformBufferLoader(objectMaterial).objectFragUniform.buf.GetVkBuffer() };
                }
                else if (bufferName == "Light")
                {
                    return {m_LightUniform.bufferHandles.begin(), m_LightUniform.bufferHandles.end()};
                }

                return {};
            }
            );

        return shaderMaterial;
    };


    const uint32_t loaderFlags = 0; // No instancing
    const bool ignoreTransforms = (loaderFlags & DrawableLoader::LoaderFlags::IgnoreHierarchy) != 0;

    const auto sceneAssetPath = std::filesystem::path(MESH_DESTINATION_PATH).append(gSceneAssetModel).string();
    MeshLoaderModelSceneSanityCheck meshSanityCheckProcessor(sceneAssetPath);
    MeshObjectIntermediateGltfProcessor meshObjectProcessor(sceneAssetPath, ignoreTransforms, glm::vec3(1.0f,1.0f,1.0f));
    CameraGltfProcessor meshCameraProcessor{};

    if (!MeshLoader::LoadGltf(*m_AssetManager, sceneAssetPath, meshSanityCheckProcessor, meshObjectProcessor, meshCameraProcessor) ||
        !DrawableLoader::CreateDrawables(vulkan,
            std::move(meshObjectProcessor.m_meshObjects),
            m_RenderPassData[RP_SCENE].RenderContext,
            MaterialLoader,
            m_SceneDrawables,
            loaderFlags))
    {
        LOGE("Error Loading the scene gltf file");
        LOGI("Please verify if you have all required assets on the sample media folder");
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

    Mesh blitQuadMesh;
    if (!MeshHelper::CreateMesh<Vulkan>( vulkan.GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pBlitQuadShader->m_shaderDescription->m_vertexFormats, &blitQuadMesh ))
    {
        return false;
    }

    // Blit Material
    auto blitQuadShaderMaterial = m_MaterialManager->CreateMaterial(*pBlitQuadShader, 2,
        [this](const std::string& texName) -> const MaterialManager::tPerFrameTexInfo
        {
            if (texName == "Diffuse")
            {
                return { m_sgsr2_context->GetSceneColorOutput() };
            }
            else if (texName == "Overlay")
            {
                return { &m_RenderPassData[RP_HUD].RenderTarget.m_ColorAttachments[0] };
            }
            return {};
        },
        [this](const std::string& bufferName) -> PerFrameBuffer
        {
            return {};
        }
        );

    m_BlitQuadDrawable = std::make_unique<Drawable>(vulkan, std::move(blitQuadShaderMaterial));
    if (!m_BlitQuadDrawable->Init(m_RenderPassData[RP_BLIT].RenderContext[0], std::move(blitQuadMesh)))
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

    m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);

    char szName[256];
    for (uint32_t whichBuffer = 0; whichBuffer < m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.size(); whichBuffer++)
    {
        // Model => Secondary
        sprintf(szName, "Model (%s; Buffer %d of %d)", GetPassName(RP_SCENE), whichBuffer + 1, NUM_VULKAN_BUFFERS);
        if (!m_RenderPassData[RP_SCENE].ObjectsCmdBuffer[whichBuffer].Initialize(pVulkan, szName, CommandListBase::Type::Secondary))
        {
            return false;
        }
    }

    for (auto& command_buffer : m_UpscaleAndPostCommandList)
    {
        command_buffer.Initialize( pVulkan, "SGSR2 and Post Process Command List", CommandListBase::Type::Primary );
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("*************************************");
    LOGI("Building Secondary Command Buffers...");
    LOGI("*************************************");

    auto& vulkan = *GetVulkan();

    // Begin recording
    {
        auto& renderPassData = m_RenderPassData[RP_SCENE];
        auto& renderContext  = renderPassData.RenderContext[0];

        for (auto& cmdBufer: renderPassData.ObjectsCmdBuffer)
        {
            uint32_t targetWidth = renderPassData.RenderTarget.m_Width;
            uint32_t targetHeight = renderPassData.RenderTarget.m_Height;
            const VkViewport viewport {
                .width      = (float)targetWidth,
                .height     = (float)targetHeight,
                .minDepth   = 0.0f,
                .maxDepth   = 1.0f
            };

            const VkRect2D scissor{
                .extent {
                    .width = targetWidth,
                    .height = targetHeight
            }};

            // Set up some values that change based on render pass
            const Framebuffer<Vulkan>& framebuffer = *renderContext.GetFramebuffer();

            // Objects (can render into any pass)
            if (!cmdBufer.Begin(framebuffer, renderContext.GetRenderPass()))
            {
                return false;
            }
            vkCmdSetViewport( cmdBufer, 0, 1, &viewport );
            vkCmdSetScissor ( cmdBufer, 0, 1, &scissor );
        }
    }
    
    // Scene drawables
    for (const auto& sceneDrawable : m_SceneDrawables)
    {
        AddDrawableToCmdBuffers(sceneDrawable, m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.size()));
    }

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
    switch (gUpscaleMode)
    {
        case 0:
        default:
            break;
        case 1:
            m_Camera.SetJitter(m_sgsr2_context->GetJitter() / glm::vec2(gRenderWidth, gRenderHeight));
            break;
        case 2:
            m_Camera.SetJitter(m_sgsr2_context_frag->GetJitter() / glm::vec2(gRenderWidth, gRenderHeight));
            break;
    } 

    static float cameraSpeedFactor = 1.0f;
    cameraSpeedFactor = std::max(0.25f, cameraSpeedFactor + ImGui::GetIO().MouseWheel * 0.25f);

    m_Camera.UpdateController(fltDiffTime * cameraSpeedFactor, *m_CameraController);
    m_Camera.UpdateMatrices();
    
    // Update uniform buffers with latest data
    UpdateUniforms(whichBuffer);
    m_sgsr2_context->UpdateUniforms( *GetVulkan(), m_Camera, prev_view_proj_matrix );
    m_sgsr2_context_frag->UpdateUniforms( *GetVulkan(), m_Camera, prev_view_proj_matrix );

    // First time through, wait for the back buffer to be ready
    std::span<const VkSemaphore> pWaitSemaphores = { &currentVulkanBuffer.semaphore, 1 };


    auto& command_buffer = m_UpscaleAndPostCommandList[whichBuffer];
    command_buffer.Begin();

    // RP_SCENE
    {
        const auto& renderContext = m_RenderPassData[RP_SCENE].RenderContext[0];
        const fvk::VkRenderPassBeginInfo RPBeginInfo{renderContext.GetRenderPassBeginInfo()};
        vkCmdBeginRenderPass( command_buffer, &RPBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS );
        vkCmdExecuteCommands( command_buffer, 1, &m_RenderPassData[RP_SCENE].ObjectsCmdBuffer[whichBuffer].m_VkCommandBuffer );
        vkCmdEndRenderPass( command_buffer );
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
            const fvk::VkRenderPassBeginInfo RPBeginInfo{renderContext.GetRenderPassBeginInfo()};
            vkCmdBeginRenderPass( command_buffer, &RPBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS );
            vkCmdExecuteCommands( command_buffer, 1, &guiCommandBuffer );
            vkCmdEndRenderPass( command_buffer );
        }
    }

    // SGSR 2.0
    {
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

        const auto& renderContext = m_RenderPassData[RP_BLIT].RenderContext[currentVulkanBuffer.swapchainPresentIdx];
        const fvk::VkRenderPassBeginInfo RPBeginInfo{renderContext.GetRenderPassBeginInfo()};
        vkCmdBeginRenderPass( command_buffer, &RPBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
        const auto scissor  = renderContext.GetRenderPassClearData().scissor;
        const auto viewport = renderContext.GetRenderPassClearData().viewport;
        vkCmdSetScissor( command_buffer, 0, 1, &scissor);
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        const TextureVulkan* pUpscaledBuffer = nullptr;
        switch (gUpscaleMode) {
        case 0:
        default:
            pUpscaledBuffer = &m_RenderPassData[RP_SCENE].RenderTarget.m_ColorAttachments[0];
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

        vkCmdEndRenderPass( command_buffer );
    }
    vkEndCommandBuffer( command_buffer );

    // Submit the commands to the queue.
    command_buffer.QueueSubmit( currentVulkanBuffer.semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vulkan.m_RenderCompleteSemaphore, currentVulkanBuffer.fence );

    // Queue is loaded up, tell the driver to start processing
    vulkan.PresentQueue(vulkan.m_RenderCompleteSemaphore, currentVulkanBuffer.swapchainPresentIdx);
}
