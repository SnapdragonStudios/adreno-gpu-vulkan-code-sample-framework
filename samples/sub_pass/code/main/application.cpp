//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

///
/// Sample app for Tonemapping in Shader Resolve (MSAA)
///

#include "application.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "gui/imguiVulkan.hpp"
#include "material/vulkan/drawable.hpp"
#include "material/vulkan/materialManager.hpp"
#include "material/vulkan/shaderManager.hpp"
#include "mesh/meshHelper.hpp"
#include "system/math_common.hpp"
#include "texture/vulkan/textureManager.hpp"
#include "imgui.h"
#include <memory>
#include <bitset>
#include <filesystem>
#include <sys/stat.h>

// Global Variables From Config File
VAR(glm::vec3,  gCameraStartPos, glm::vec3(0.0f, 3.5f, 0.0f), kVariableNonpersistent);
VAR(glm::vec3,  gCameraStartRot, glm::vec3(0.0f, 0.0f, 0.0f), kVariableNonpersistent);
VAR(float,      gFOV, PI_DIV_4, kVariableNonpersistent);
VAR(float,      gNearPlane, 0.1f, kVariableNonpersistent);
VAR(float,      gFarPlane, 80.0f, kVariableNonpersistent);
VAR(bool,       gShaderResolve,     true, kVariableNonpersistent);
VAR(bool,       gUseSubpasses,      false, kVariableNonpersistent);
VAR(bool,       gUseRenderPassTransform,true, kVariableNonpersistent);

namespace
{
    Msaa        gMsaa            = Msaa::Samples4;
    const char* gSceneAssetModel = "SteamPunkSauna.gltf";
}

///
/// @brief Implementation of the Application entrypoint (called by the framework)
/// @return Pointer to Application (derived from @ApplicationHelperBase ... derived from @FrameworkApplicationBase).
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
int Application::PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat> formats)
//-----------------------------------------------------------------------------
{
    // On Snapdragon if the surfaceflinger has to do the rotation to the display native orientation then it will do it at 8bit colordepth.
    // To avoid this we need to enable the 'pre-rotation' of the display (and the use of VK_QCOM_render_pass_transform so we dont have to rotate our buffers/passes manually).
    GetVulkan()->m_UseRenderPassTransform = true;

    // We want to select a SRGB output format (if one exists)
    int index = 0;
    for (const auto& format : formats)
    {
        if (format.format == TextureFormat::B8G8R8A8_SRGB)
            return index;
        ++index;
    }
    return -1;
}

//-----------------------------------------------------------------------------
bool Application::Initialize( uintptr_t windowHandle, uintptr_t instanceHandle )
//-----------------------------------------------------------------------------
{
    if( !ApplicationHelperBase::Initialize( windowHandle, instanceHandle) )
    {
        return false;
    }

    Vulkan* pVulkan = GetVulkan();

    // Disable shader resolve if it is not exposed by Vulkan.
    gShaderResolve = gShaderResolve && pVulkan->GetExtRenderPassShaderResolveAvailable();
    // Disable render pass transform if it is not exposed by Vulkan.
    gUseRenderPassTransform = gUseRenderPassTransform && pVulkan->GetExtRenderPassTransformAvailable();

    m_RequestedUseSubpasses  = gUseSubpasses;

    LoadShaders();

    CreateUniformBuffer( pVulkan, m_ObjectVertUniform );
    CreateUniformBuffer( pVulkan, m_ObjectFragUniform );

    InitGui(windowHandle, instanceHandle);

    InitCommandBuffers();

    InitFramebuffersRenderPassesAndDrawables();

    InitHdr();
        
    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    return true;
}


//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    //m_ShaderManager->RegisterRenderPassNames( {sRenderPassNames} );

    LOGI("Loading Shaders...");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
          { tIdAndFilename { "Object",                        "Object.json" },
            tIdAndFilename { "Tonemap",                       "Tonemap.json" },
            tIdAndFilename { "TonemapMsaa",                   "TonemapMsaa.json" },
            tIdAndFilename { "TonemapSubpassMsaa",            "TonemapSubpassMsaa.json" },
            tIdAndFilename { "TonemapSubpassShaderResolve1x", "TonemapSubpassShaderResolve1x.json" },
            tIdAndFilename { "TonemapSubpassShaderResolve4x", "TonemapSubpassShaderResolve4x.json" },
            tIdAndFilename { "Blit",                          "Blit.json" }
          })
    {
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second, SHADER_DESTINATION_PATH))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
        }
    }

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitFramebuffersRenderPassesAndDrawables()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    //
    // Clean up old render targets and render passes
    m_LinearColorRT.Release();
    m_TonemapRT.Release();
    m_BlitRenderPass = {};

    //
    // Setup formats for the first pass
    std::vector<TextureFormat> PassColorFormats = { TextureFormat::A2B10G10R10_UNORM_PACK32 };// TextureFormat::R8G8B8A8_UNORM };// { TextureFormat::R16G16B16A16_SFLOAT };
    // std::vector<TextureFormat> PassColorFormats = { TextureFormat::R8G8B8A8_UNORM };
    std::vector<Msaa> PassColorMsaa = { gMsaa };
    std::vector<TEXTURE_TYPE> PassTextureTypes;

    const TextureFormat DepthFormat = pVulkan->GetBestSurfaceDepthFormat();
    const bool NeedsResolve = true;

    RenderPass ObjectRenderPass;
    if( gUseSubpasses )
    {
        //
        // Create a single RenderPass with subpasses for objects and then tonemapping
        //

        PassTextureTypes = { TT_RENDER_TARGET_SUBPASS };
        if(NeedsResolve && !gShaderResolve)
        {
            // Intermediate MSAA target for output of tonemap
            PassColorFormats.push_back( TextureFormat::A2B10G10R10_UNORM_PACK32 );
            PassColorMsaa.push_back( gMsaa );
            PassTextureTypes.push_back( TT_RENDER_TARGET_SUBPASS );
        }
        // Resolved Output
        PassColorFormats.push_back( TextureFormat::A2B10G10R10_UNORM_PACK32 );
        PassColorMsaa.push_back( Msaa::Samples1 );
        PassTextureTypes.push_back( TT_RENDER_TARGET );

        // Create a compatible RenderPass
        if( !Create2SubpassRenderPass( { &PassColorFormats[0],1 }, { &PassColorFormats[1],1 }, DepthFormat, *PassColorMsaa.cbegin(), *PassColorMsaa.crbegin(), ObjectRenderPass ) )
        {
            LOGE( "Unable to create render pass" );
            return false;
        }
        pVulkan->SetDebugObjectName( ObjectRenderPass, "CombinedRenderPass" );
    }
    else
    {
        //
        // Non subpassed.
        // Create two seperate passes - Object and Tonemap
        //

        // Object render pass
        PassTextureTypes = { TT_RENDER_TARGET };
        if( !pVulkan->CreateRenderPass( PassColorFormats,
                                        DepthFormat,
                                        PassColorMsaa.back(),
                                        RenderPassInputUsage::Clear/*color*/,
                                        RenderPassOutputUsage::StoreReadOnly,/*color*/
                                        true,/*clear depth*/
                                        RenderPassOutputUsage::Discard,/*depth*/
                                        ObjectRenderPass ) )
        {
            return false;
        }
        pVulkan->SetDebugObjectName( ObjectRenderPass, "ObjectRenderPass" );

        // Tonemap render pass
        RenderPass TonemapRenderPass;

        std::vector<TextureFormat> TonemapColorAndResolveFormats{ TextureFormat::A2B10G10R10_UNORM_PACK32 };
        // std::vector<TextureFormat> TonemapColorAndResolveFormats{ TextureFormat::R8G8B8A8_UNORM };
        std::span<const TextureFormat> TonemapResolveFormat{};
        std::vector<Msaa> TonemapColorAndResolveMsaa{ PassColorMsaa.back() };
        std::vector<TEXTURE_TYPE> TonemapColorAndResolveTextureTypes{ TT_RENDER_TARGET };

        if( NeedsResolve )
        {
            // Setup to have an additional 1xMSAA buffer for the Resolve after the tonemap
            TonemapColorAndResolveFormats.push_back( TextureFormat::A2B10G10R10_UNORM_PACK32 );
            TonemapResolveFormat = { &TonemapColorAndResolveFormats.back(), 1 };
            TonemapColorAndResolveMsaa.push_back( Msaa::Samples1 );
            TonemapColorAndResolveTextureTypes.push_back( TT_RENDER_TARGET );
        }

        std::span<const TextureFormat> TonemapColorFormat{ &TonemapColorAndResolveFormats.front(),1 };
        if( !pVulkan->CreateRenderPass( TonemapColorFormat,
                                        TextureFormat::UNDEFINED,
                                        TonemapColorAndResolveMsaa.front(),
                                        RenderPassInputUsage::DontCare/*color*/,
                                        RenderPassOutputUsage::StoreReadOnly,/*color*/
                                        false,/*dont clear depth*/
                                        RenderPassOutputUsage::Discard,/*depth*/
                                        TonemapRenderPass,
                                        TonemapResolveFormat) )
        {
            return false;
        }
        pVulkan->SetDebugObjectName( TonemapRenderPass, "TonemapRenderPass" );

        // Tonemap RT
        {
            const RenderTargetInitializeInfo info{
                .Width = gRenderWidth,
                .Height = gRenderHeight,
                .LayerFormats = TonemapColorAndResolveFormats,
                .DepthFormat = TextureFormat::UNDEFINED,
                .TextureTypes = TonemapColorAndResolveTextureTypes,
                .Msaa = TonemapColorAndResolveMsaa,
                .ResolveTextureFormats = TonemapResolveFormat,
                .FilterModes = {}
            };

            // Create the render target for the Tonemap (only needed when passes are seperate - subpass variant has all the rendertarget buffers for both subpasses in m_LinearColorRT)
            if (!m_TonemapRT.Initialize(pVulkan, info, "Tonemap RT", &TonemapRenderPass))
            {
                LOGE("Error initializing LinearColorRT");
                return false;
            }
        }

        m_TonemapRenderContext = RenderContext(std::move(TonemapRenderPass), m_TonemapRT.GetFrameBuffer(), "TonemapRenderContext");
    }

    // Linear Color RT
    {
        const RenderTargetInitializeInfo info{
            .Width = gRenderWidth,
            .Height = gRenderHeight,
            .LayerFormats = PassColorFormats,
            .DepthFormat = DepthFormat,
            .TextureTypes = PassTextureTypes,
            .Msaa = PassColorMsaa,
            .ResolveTextureFormats = {},
            .FilterModes = {}
        };

        // Create render target(s) for the scene render (sub) passes.
        if (!m_LinearColorRT.Initialize(pVulkan, info, "Main RT", &ObjectRenderPass))
        {
            LOGE("Error initializing LinearColorRT");
            return false;
        }
    }

    m_ObjectRenderContext = RenderContext(std::move(ObjectRenderPass), m_LinearColorRT.GetFrameBuffer(), "ObjectRenderContext");

    // Blit render pass
    if( !pVulkan->CreateRenderPass( { &pVulkan->m_SurfaceFormat, 1 },
                                    pVulkan->m_SwapchainDepth.format,
                                    Msaa::Samples1,
                                    RenderPassInputUsage::DontCare/*color*/,
                                    RenderPassOutputUsage::Present,/*color*/
                                    false,/*dont clear depth*/
                                    RenderPassOutputUsage::Discard,/*depth*/
        m_BlitRenderPass) )
    {
        return false;
    }
    pVulkan->SetDebugObjectName(m_BlitRenderPass, "BlitRenderPass" );

    //
    // Setup the drawables using the correct render passes / subpasses
    //

    // Scene (object) drawable
    LoadSceneDrawables(m_ObjectRenderContext);

    if( gUseSubpasses )
    {
        //
        // Tonemap drawable
        //
        ImageInfo tonemapDiffuseImageInfo( m_LinearColorRT.GetColorAttachments().front() );
        tonemapDiffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::map<const std::string, const ImageInfo> tonemapImages = { {"Diffuse", std::move(tonemapDiffuseImageInfo) } };

        const char* pShaderName = "TonemapSubpassMsaa";
        if( gShaderResolve )
        {
            switch( PassColorMsaa.front() ) {
            default:                pShaderName = "TonemapSubpassShaderResolve1x"; break;
            case Msaa::Samples2:    pShaderName = "TonemapSubpassShaderResolve2x"; break;
            case Msaa::Samples4:    pShaderName = "TonemapSubpassShaderResolve4x"; break;
            }
        }
        // Tonemapping runs in second sub-pass at the MSAA samplerate of its destination buffer (either the shader resolve output buffer @ 1xMSAA or the pre hardware resolve output buffer @ 4xMSAA etc)
        m_TonemapDrawable = InitFullscreenDrawable( pShaderName, {}, tonemapImages, m_ObjectRenderContext.GetRenderPass(), 1, PassColorMsaa[1]);

        //
        // Blit drawable
        //
        std::map<const std::string, const TextureVulkan*> blitInputs = { {"Diffuse", &m_LinearColorRT.GetColorAttachments().back() },
                                                                         {"Overlay", &m_GuiRT.GetColorAttachments().front() },
        };

        m_BlitDrawable = InitFullscreenDrawable( "Blit", blitInputs, {}, m_BlitRenderPass, 0, Msaa::Samples1 );
    }
    else
    {
        //
        // Tonemap drawable
        //
        std::map<const std::string, const TextureVulkan*> tonemapTex = { {"Diffuse", &m_LinearColorRT.GetColorAttachments().front() } };
        if( PassColorMsaa.front() == Msaa::Samples1 )
        {
            m_TonemapDrawable = InitFullscreenDrawable( "Tonemap", tonemapTex, {}, m_TonemapRenderContext.GetRenderPass(), 0, PassColorMsaa.front());
        }
        else
        {
            m_TonemapDrawable = InitFullscreenDrawable( "TonemapMsaa", tonemapTex, {}, m_TonemapRenderContext.GetRenderPass(), 0, PassColorMsaa.front());
        }

        //
        // Blit drawable
        //
        std::map<const std::string, const TextureVulkan*> blitInputs = { {"Diffuse", &m_TonemapRT.GetColorAttachments().back() },
                                                                         {"Overlay", &m_GuiRT.GetColorAttachments().front() },
        };
        m_BlitDrawable = InitFullscreenDrawable( "Blit", blitInputs, {}, m_BlitRenderPass, 0, Msaa::Samples1 );
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadSceneDrawables( const RenderContext& renderContext)
//-----------------------------------------------------------------------------
{
    const auto sceneAssetPath = std::filesystem::path(MESH_DESTINATION_PATH).append(gSceneAssetModel).string();

    LOGI( "Loading Scene Drawable %s", sceneAssetPath.c_str());

    Vulkan* pVulkan = GetVulkan();

    const auto* pObjectShader = m_ShaderManager->GetShader( "Object" );
    if( !pObjectShader )
    {
        // Error (missing shader)
        return false;
    }

    const PathManipulator_PrefixDirectory prefixTextureDir{ TEXTURE_DESTINATION_PATH };
    const auto whiteTexture = m_TextureManager->GetOrLoadTexture("white_d.ktx", m_SamplerRepeat, prefixTextureDir);

    // Lambda to load a texture for the given material (slot)
    const auto& textureLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef, const std::string& textureSlotName) -> const PerFrameTexInfo
    {
        const PathManipulator_ChangeExtension changeTextureExt{ ".ktx" };

        auto* diffuseTexture = m_TextureManager->GetOrLoadTexture(materialDef.diffuseFilename, m_SamplerEdgeClamp, prefixTextureDir, changeTextureExt);
        if (!diffuseTexture)
        {
            return { whiteTexture };
        }
        else
        {
            return { diffuseTexture };
        }
    };

    // Lambda to associate (uniform) buffers with their shader binding/slot names.
    const auto& bufferLoader = [&]( const std::string& bufferSlotName ) -> PerFrameBufferVulkan {
        if( bufferSlotName == "Vert" )
        {
            return { m_ObjectVertUniform.buf.GetVkBuffer() };
        }
        else if( bufferSlotName == "Frag" )
        {
            return { m_ObjectFragUniform.buf.GetVkBuffer() };
        }
        else
        {
            assert( 0 );
            return {};
        }
    };

    const auto& materialLoader = [&]( const MeshObjectIntermediate::MaterialDef& materialDef ) -> std::optional<Material>
    {
        using namespace std::placeholders;
        
        return std::move( m_MaterialManager->CreateMaterial(*pObjectShader, NUM_VULKAN_BUFFERS, std::bind( textureLoader, std::cref( materialDef ), _1 ), bufferLoader ) );
    };


    m_SceneObject.clear();

    if( !DrawableLoader::LoadDrawables( *pVulkan, *m_AssetManager, renderContext, sceneAssetPath, materialLoader, m_SceneObject, DrawableLoader::LoaderFlags::None))
    {
        LOGE( "Error loading Object mesh: %s", sceneAssetPath.c_str());
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
std::unique_ptr<ApplicationHelperBase::Drawable> Application::InitFullscreenDrawable( const char* pShaderName, const std::map<const std::string, const TextureVulkan*>& ColorAttachmentsLookup, const std::map<const std::string, const ImageInfo>& ImageAttachmentsLookup, const RenderPass& renderPass, uint32_t subpassIdx, Msaa passMsaa )
//-----------------------------------------------------------------------------
{
    //
    // Tonemap pass is a fullscreen quad, setup a 'drawable'.
    //

    Vulkan* pVulkan = GetVulkan();

    LOGI("Creating %s mesh...", pShaderName);

    const auto* pShader = m_ShaderManager->GetShader(pShaderName);
    assert(pShader);
    if( !pShader )
    {
        LOGE("Error, %s shader is unknown (not loaded?)", pShaderName);
        return nullptr;
    }

    Mesh mesh;
    if (!MeshHelper::CreateMesh(pVulkan->GetMemoryManager(), MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pShader->m_shaderDescription->m_vertexFormats, &mesh))
    {
        LOGE("Error creating Fullscreen Mesh (for %s)", pShaderName);
        return nullptr;
    }

    auto shaderMaterial = m_MaterialManager->CreateMaterial(*pShader, NUM_VULKAN_BUFFERS,
        [this, &ColorAttachmentsLookup](const std::string& texName) -> const PerFrameTexInfo {
            return { ColorAttachmentsLookup.find(texName)->second };
            assert(0);
            return {};
        },
        [](const std::string& bufferName) -> const PerFrameBuffer {
            assert(0);
            return {};
        },
        [this, &ImageAttachmentsLookup](const std::string& imageName) -> const ImageInfo {
            return { ImageAttachmentsLookup.find(imageName)->second };
        }
        );

    static const char* passName = "Fullscreen";
    const Msaa renderPassMultisamples[1] = { passMsaa };
    const uint32_t renderPassSubpasses[1] = { subpassIdx };

    RenderContext renderContext = RenderContext(renderPass.Copy(), {}, passName);
    renderContext.subPass = subpassIdx;

    auto drawable = std::make_unique<Drawable>( *pVulkan, std::move( shaderMaterial ) );
    if (!drawable->Init(renderContext, std::move(mesh), std::nullopt, std::nullopt))
    {
        LOGE("Error creating Blit Drawable");
        return nullptr;
    }

    return std::move(drawable);
}


//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    char szName[256];
    for( uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++ )
    {
        sprintf( szName, "CommandBuffer (%d of %d)", WhichBuffer + 1, NUM_VULKAN_BUFFERS );
        if( !m_CommandBuffer[WhichBuffer].Initialize( pVulkan, szName, CommandBuffer::Type::Primary ) )
        {
            return false;
        }
    }
    return true;
}


//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    UpdateGui();

    UpdateCamera( fltDiffTime );

    auto* pVulkan = GetVulkan();

    // See if we want to change the MSAA mode
    if(  gUseSubpasses != m_RequestedUseSubpasses )
    {
        ChangeMsaaMode();
    }

    auto CurrentVulkanBuffer = pVulkan->SetNextBackBuffer();

    UpdateUniforms( CurrentVulkanBuffer.idx );

    UpdateCommandBuffer( CurrentVulkanBuffer.idx );

    // ... submit the command buffer to the device queue
    m_CommandBuffer[CurrentVulkanBuffer.idx].QueueSubmit( CurrentVulkanBuffer.semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, pVulkan->m_RenderCompleteSemaphore, CurrentVulkanBuffer.fence );

    // Queue is loaded up, tell the driver to start processing
    pVulkan->PresentQueue(pVulkan->m_RenderCompleteSemaphore, CurrentVulkanBuffer.swapchainPresentIdx );
}


//-----------------------------------------------------------------------------
void Application::UpdateCamera(float elapsedTime)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_Camera.UpdateController(elapsedTime, *m_CameraController);
    m_Camera.UpdateMatrices();
}


//-----------------------------------------------------------------------------
bool Application::UpdateUniforms( uint32_t bufferIdx )
//-----------------------------------------------------------------------------
{
    auto* pVulkan = GetVulkan();

    ObjectVertUniform data{ };
    data.MVPMatrix = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix();
    data.ModelMatrix = glm::identity<glm::mat4>();
    UpdateUniformBuffer( pVulkan, m_ObjectVertUniform, data );

    return true;
}


//-----------------------------------------------------------------------------
bool Application::UpdateCommandBuffer(uint32_t bufferIdx)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    auto& commandBuffer = m_CommandBuffer[bufferIdx];

    if( !commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
    {
        return false;
    }

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = m_LinearColorRT.GetWidth();
    Scissor.extent.height = m_LinearColorRT.GetHeight();

    VkClearColorValue ClearColor{};
    std::fill( std::begin(ClearColor.float32), std::end(ClearColor.float32), 0.0f);
    ClearColor.float32[0] = 0.1f;

    if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, m_LinearColorRT.GetNumColorLayers(), true, m_ObjectRenderContext.GetRenderPass(), false, m_LinearColorRT.GetFrameBuffer(), VK_SUBPASS_CONTENTS_INLINE))
    {
        return false;
    }

    for(const auto& sceneObjectDrawable : m_SceneObject)
        AddDrawableToCmdBuffers(sceneObjectDrawable, &commandBuffer, 1, 1);

    //
    // Now add the tonemapping...
    //
    if( gUseSubpasses )
    {
        // Tonemap is in (the next) subpass.
        commandBuffer.NextSubpass( VK_SUBPASS_CONTENTS_INLINE );
    }
    else
    {
        // Tonemap has its own render pass.
        commandBuffer.EndRenderPass();

        if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, 1, true, m_TonemapRenderContext.GetRenderPass(), false, m_TonemapRT.GetFrameBuffer(), VK_SUBPASS_CONTENTS_INLINE))
        {
            return false;
        }
    }

    AddDrawableToCmdBuffers( *m_TonemapDrawable, &commandBuffer, 1, 1 );

    commandBuffer.EndRenderPass();

    //
    // Do the Gui
    //
    if (m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, 1, true, m_GuiRenderContext.GetRenderPass(), false, m_GuiRT.GetFrameBuffer(), VK_SUBPASS_CONTENTS_INLINE))
        {
            return false;
        }
        GetGui()->Render(commandBuffer.m_VkCommandBuffer);

        commandBuffer.EndRenderPass();
    }

    //
    // Now add the final Blit to swapchain backbuffer...
    //

    Scissor.extent.width = pVulkan->m_SurfaceWidth;
    Scissor.extent.height = pVulkan->m_SurfaceHeight;

    if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, 1, false, m_BlitRenderPass, true/*swapchain*/, pVulkan->m_SwapchainBuffers[bufferIdx].framebuffer.m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE ) )
    {
        return false;
    }

    AddDrawableToCmdBuffers( *m_BlitDrawable, &commandBuffer, 1, 1 );

    commandBuffer.EndRenderPass();

    commandBuffer.End();

    return true;
}


//-----------------------------------------------------------------------------
bool Application::Create2SubpassRenderPass( const std::span<const TextureFormat> InternalColorFormats, const std::span<const TextureFormat> OutputColorFormats, TextureFormat InternalDepthFormat, Msaa InternalMsaa, Msaa OutputMsaa, RenderPass& rRenderPass/*out*/ )
//-----------------------------------------------------------------------------
{
    assert( !InternalColorFormats.empty() );                // not supporting a depth only pass
    assert( !OutputColorFormats.empty() );

    Vulkan* pVulkan = GetVulkan();

    const bool NeedsResolve = InternalMsaa != OutputMsaa;

    // Color attachments and Depth attachment
    std::vector<VkAttachmentDescription> PassAttachDescs;
    PassAttachDescs.reserve(InternalColorFormats.size()+OutputColorFormats.size()+2);

    // Each subpass needs a reference to its attachments
    std::array<VkSubpassDescription, 2> SubpassDesc { 
        VkSubpassDescription {0/*flags*/, VK_PIPELINE_BIND_POINT_GRAPHICS},
        VkSubpassDescription {0/*flags*/, VK_PIPELINE_BIND_POINT_GRAPHICS} };
    std::vector<VkAttachmentReference> ColorReferencesPass0;
    ColorReferencesPass0.reserve(InternalColorFormats.size());

    std::vector<VkAttachmentReference> InputReferencesPass1;
    std::vector<VkAttachmentReference> ColorReferencesPass1;
    std::vector<VkAttachmentReference> ResolveReferencesPass1;
    InputReferencesPass1.reserve(InternalColorFormats.size());
    ColorReferencesPass1.reserve(OutputColorFormats.size());
    ResolveReferencesPass1.reserve(OutputColorFormats.size());

    const bool HasDepth = InternalDepthFormat != TextureFormat::UNDEFINED;
    const bool HasPass2ReadDepth = HasDepth && false;

    //
    // First Pass Color Attachments (what is written by the first pass)
    // Also setup as inputs to the second pass.
    for(const auto& ColorFormat: InternalColorFormats)
    {
        // Pass0 color and depth buffers setup to clear on load, discard on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass0 = { 0/*flags*/,
            TextureFormatToVk(ColorFormat)/*format*/,
            EnumToVk(InternalMsaa)/*samples*/,
            VK_ATTACHMENT_LOAD_OP_CLEAR/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/};
        PassAttachDescs.push_back( AttachmentDescPass0 );

        ColorReferencesPass0.push_back({ (uint32_t) PassAttachDescs.size()-1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL/*output of first pass*/ });
        InputReferencesPass1.push_back({ (uint32_t) PassAttachDescs.size()-1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*input of second pass*/ });
    }

    //
    // Second Pass Color Attachments (these are the outputs from the second pass)
    for(const auto& ColorFormat: OutputColorFormats)
    {
        // Pass1 color buffers setup to, store on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass1 = { 0, TextureFormatToVk(ColorFormat)/*format*/,
            (gShaderResolve||!NeedsResolve) ? EnumToVk(OutputMsaa) : EnumToVk(InternalMsaa)/*samples*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
            (gShaderResolve||!NeedsResolve) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/, 
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/};
        PassAttachDescs.push_back( AttachmentDescPass1 );

        ColorReferencesPass1.push_back({ (uint32_t) PassAttachDescs.size()-1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*output of second pass*/ });
    }

    if ( NeedsResolve && !gShaderResolve )
    {
        // Shader Resolve not available, drop back to resolving at the end of the pass.

        //
        // Second pass Resolve Attachments (output)
        for( const auto& ColorFormat : OutputColorFormats )
        {
            // Pass1 color buffers to resolve to at end of pass.
            VkAttachmentDescription AttachmentDescResolvePass1 = { 0, TextureFormatToVk(ColorFormat)/*format*/,
                EnumToVk(OutputMsaa)/*samples*/,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
                VK_ATTACHMENT_STORE_OP_STORE/*storeOp*/,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/, 
                VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
                VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/};

            PassAttachDescs.push_back( AttachmentDescResolvePass1 );
            ResolveReferencesPass1.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } );
        }

        //
        // Setup the resolve (second subpass)
        SubpassDesc[1].pResolveAttachments = ResolveReferencesPass1.data();
    }

    //
    // Depth Attachment (cleared at start of pass, written by first subpass, discarded after pass)
    VkAttachmentReference DepthReference = {};
    VkAttachmentReference DepthReferencePass1 = {};
    if (HasDepth)
    {
        VkAttachmentDescription AttachmentDescDepthPass0 = { 0/*flags*/,
            TextureFormatToVk(InternalDepthFormat)/*format*/,
            EnumToVk(InternalMsaa)/*samples*/,
            VK_ATTACHMENT_LOAD_OP_CLEAR/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            HasPass2ReadDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL/*finalLayout*/};

        PassAttachDescs.push_back( AttachmentDescDepthPass0 );
        DepthReference = { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        if (HasPass2ReadDepth)
            InputReferencesPass1.push_back({ (uint32_t) PassAttachDescs.size()-1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL/*input of second pass*/ });
    }

    if( !NeedsResolve )
    {
        // Doesnt need to do a resolve (shader or otherwise) - msaa is same for input and output from this pass.
    }
    else if( gShaderResolve )
    {
        // Shader Resolve extension (VK_QCOM_render_pass_shader_resolve) is available, use it. 
        assert( OutputMsaa == Msaa::Samples1 );
        SubpassDesc[1].flags = 0x00000008/*VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM*/ | 0x00000004/*VK_SUBPASS_DESCRIPTION_FRAGMENT_REGION_BIT_QCOM*/;
    }

    //
    // Subpass dependencies
    std::array<VkSubpassDependency, 1> PassDependencies = {};
    if (ColorReferencesPass0.size() > 0)
    {
        SubpassDesc[0].colorAttachmentCount = (uint32_t)ColorReferencesPass0.size();
        SubpassDesc[0].pColorAttachments = ColorReferencesPass0.data();
        SubpassDesc[1].inputAttachmentCount = (uint32_t)InputReferencesPass1.size();
        SubpassDesc[1].pInputAttachments = InputReferencesPass1.data();
    }
    if (ColorReferencesPass1.size() > 0)
    {
        SubpassDesc[1].colorAttachmentCount = (uint32_t)ColorReferencesPass1.size();
        SubpassDesc[1].pColorAttachments = ColorReferencesPass1.data();
    }

    // Only first pass writes the Depth
    if( HasDepth )
    {
        SubpassDesc[0].pDepthStencilAttachment = &DepthReference;
    }

    // Color subpass (takes output of first subpass as input to fragment shader)
    PassDependencies[0].srcSubpass = 0;
    PassDependencies[0].dstSubpass = 1;
    PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    PassDependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[0].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Now ready to actually create the render pass
    const VkRenderPassCreateInfo RenderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .flags = 0,
        .attachmentCount = (uint32_t)PassAttachDescs.size(),
        .pAttachments = PassAttachDescs.data(),
        .subpassCount = (uint32_t)SubpassDesc.size(),
        .pSubpasses = SubpassDesc.data(),
        .dependencyCount = (uint32_t)PassDependencies.size(),
        .pDependencies = PassDependencies.data()
    };

    VkRenderPass vkRenderPass = VK_NULL_HANDLE;
    VkResult RetVal = vkCreateRenderPass(pVulkan->m_VulkanDevice, &RenderPassInfo, NULL, &vkRenderPass);
    if (!CheckVkError("vkCreateRenderPass()", RetVal))
    {
        return false;
    }
    rRenderPass.mRenderPass = {pVulkan->m_VulkanDevice, vkRenderPass};

    return true;
}


//-----------------------------------------------------------------------------
void Application::ChangeMsaaMode()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    gUseSubpasses  = m_RequestedUseSubpasses;
    
    pVulkan->RecreateSwapChain();

    InitFramebuffersRenderPassesAndDrawables();
}


//-----------------------------------------------------------------------------
bool Application::InitHdr()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    // Set the color profile
    VkHdrMetadataEXT AuthoringProfile = {VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
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
    return pVulkan->SetSwapchainHrdMetadata(AuthoringProfile);
}


//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle, uintptr_t instanceHandle)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    const TextureFormat GuiColorFormat[]{ TextureFormat::R8G8B8A8_UNORM };
    const TEXTURE_TYPE GuiTextureTypes[]{ TT_RENDER_TARGET };

    RenderPass guiRenderPass;
    if (!pVulkan->CreateRenderPass(
        GuiColorFormat,
        TextureFormat::UNDEFINED,
        Msaa::Samples1,
        RenderPassInputUsage::Clear/*color*/,
        RenderPassOutputUsage::Store,/*color*/
        false,/*dont clear depth*/
        RenderPassOutputUsage::Discard,/*depth*/
        guiRenderPass))
    {
        return false;
    }

    // GUI RT
    {
        const RenderTargetInitializeInfo info{
            .Width = gRenderWidth,
            .Height = gRenderHeight,
            .LayerFormats = GuiColorFormat,
            .DepthFormat = TextureFormat::UNDEFINED,
            .TextureTypes = GuiTextureTypes,
            .Msaa = {},
            .ResolveTextureFormats = {},
            .FilterModes = {}
        };

        if (!m_GuiRT.Initialize(pVulkan, info, "Gui RT", &guiRenderPass))
        {
            return false;
        }
    }

    m_GuiRenderContext = {std::move(guiRenderPass), m_GuiRT .GetFrameBuffer(), "Gui Pass"};

    m_Gui = std::make_unique<GuiImguiGfx>(*pVulkan, m_GuiRenderContext.GetRenderPass());
    if (!m_Gui->Initialize(windowHandle, TextureFormat::R8G8B8A8_SRGB, m_GuiRT.GetWidth(), m_GuiRT.GetHeight()))
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    if (m_Gui)
    {
        // Update Gui
        m_Gui->Update();

        // Begin our window.
        static bool settingsOpen = true;
        ImGui::SetNextWindowSize(ImVec2((gRenderWidth * 3.0f) / 4.0f, 500.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2((float)gRenderWidth / 8.0f, gRenderHeight / 2.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", &settingsOpen, (ImGuiWindowFlags)0))
        {
            // Add our widgets
            if( pVulkan->GetExtRenderPassShaderResolveAvailable() )
            {
                ImGui::Checkbox("Use Subpasses", &m_RequestedUseSubpasses);
            }
            else
            {
                ImGui::Text("RenderPass shader resolve isn't available");
            }
        
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
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    m_SceneObject.clear();
    m_TonemapDrawable.reset();
    m_BlitDrawable.reset();

    ReleaseUniformBuffer( pVulkan, &m_ObjectVertUniform );
    ReleaseUniformBuffer( pVulkan, &m_ObjectFragUniform );

    m_GuiRT.Release();
    m_TonemapRT.Release();
    m_LinearColorRT.Release();

    m_MaterialManager.reset();
    m_ShaderManager.reset();

    m_BlitRenderPass = {};

    // Finally call into base class destroy
    ApplicationHelperBase::Destroy();
}
