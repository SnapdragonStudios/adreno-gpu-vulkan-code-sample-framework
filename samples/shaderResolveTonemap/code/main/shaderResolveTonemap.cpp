//============================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================

///
/// Sample app for Tonemapping in Shader Resolve (MSAA)
///

#include "shaderResolveTonemap.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "gui/imguiVulkan.hpp"
#include "material/drawable.hpp"
#include "material/materialManager.hpp"
#include "material/shader.hpp"
#include "material/shaderDescription.hpp"
#include "material/shaderManager.hpp"
#include "system/math_common.hpp"
#include "imgui.h"
#include <bitset>
#include <filesystem>
#include <sys/stat.h>

// Global Variables From Config File
VAR(glm::vec3, gCameraStartPos, glm::vec3(-33.530f, 13.840f, 33.140f), kVariableNonpersistent);
VAR(glm::quat, gCameraStartRot, glm::quat(0.112f, 0.003f, -0.994f, 0.00727102f), kVariableNonpersistent);
VAR(float, gFOV, PI_DIV_4, kVariableNonpersistent);
VAR(float, gNearPlane, 0.1f, kVariableNonpersistent);
VAR(float, gFarPlane, 50.0f, kVariableNonpersistent);
VAR(bool,       gShaderResolve,     false, kVariableNonpersistent);
VAR(bool,       gUseSubpasses,      false, kVariableNonpersistent);
VAR(bool,       gUseRenderPassTransform,false, kVariableNonpersistent);

namespace
{
    VkSampleCountFlagBits gMsaa = VK_SAMPLE_COUNT_1_BIT;

    const char* gMuseumAssetsPath = "Media\\Meshes";
    const char* gTextureFolder = "Media\\Textures\\";
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
int Application::PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR> formats)
//-----------------------------------------------------------------------------
{
    // On Snapdragon if the surfaceflinger has to do the rotation to the display native orientation then it will do it at 8bit colordepth.
    // To avoid this we need to enable the 'pre-rotation' of the display (and the use of VK_QCOM_render_pass_transform so we dont have to rotate our buffers/passes manually).
    m_vulkan->m_UseRenderPassTransform = gUseRenderPassTransform;

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
bool Application::Initialize( uintptr_t windowHandle )
//-----------------------------------------------------------------------------
{
    if( !ApplicationHelperBase::Initialize( windowHandle ) )
    {
        return false;
    }

    Vulkan* pVulkan = m_vulkan.get();

    // Disable shader resolve if it is not exposed by Vulkan.
    gShaderResolve = gShaderResolve && pVulkan->GetExtRenderPassShaderResolveAvailable();
    // Disable render pass transform if it is not exposed by Vulkan.
    gUseRenderPassTransform = gUseRenderPassTransform && m_vulkan->GetExtRenderPassTransformAvailable();

    m_RequestedShaderResolve = gShaderResolve;
    m_RequestedUseSubpasses  = gUseSubpasses;
    m_RequestedUseRenderPassTransform = gUseRenderPassTransform;
    m_RequestedMsaa = (VkSampleCountFlagBits) gMsaa;

    LoadShaders();

    CreateUniformBuffer( pVulkan, m_ObjectVertUniform );
    CreateUniformBuffer( pVulkan, m_ObjectFragUniform );

    m_MaterialManager = std::make_unique<MaterialManager>();

    m_TexWhite = LoadKTXTexture(pVulkan, *m_AssetManager, "./Media/Textures/white_d.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT);
   
    InitGui(windowHandle);

    InitCommandBuffers();

    InitFramebuffersRenderPassesAndDrawables();

    InitHdr();
        
    m_Camera.SetPosition(gCameraStartPos, glm::normalize(gCameraStartRot));
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    return true;
}


//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    m_ShaderManager = std::make_unique<ShaderManager>(*m_vulkan);
    //m_ShaderManager->RegisterRenderPassNames( {sRenderPassNames} );

    LOGI("Loading Shaders...");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
          { tIdAndFilename { "Object",                        "Media\\Shaders\\Object.json" },
            tIdAndFilename { "Tonemap",                       "Media\\Shaders\\Tonemap.json" },
            tIdAndFilename { "TonemapMsaa",                   "Media\\Shaders\\TonemapMsaa.json" },
            tIdAndFilename { "TonemapSubpassMsaa",            "Media\\Shaders\\TonemapSubpassMsaa.json" },
            tIdAndFilename { "TonemapSubpassShaderResolve1x", "Media\\Shaders\\TonemapSubpassShaderResolve1x.json" },
            tIdAndFilename { "TonemapSubpassShaderResolve2x", "Media\\Shaders\\TonemapSubpassShaderResolve2x.json" },
            tIdAndFilename { "TonemapSubpassShaderResolve4x", "Media\\Shaders\\TonemapSubpassShaderResolve4x.json" },
            tIdAndFilename { "Blit",                          "Media\\Shaders\\Blit.json" }
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
bool Application::InitFramebuffersRenderPassesAndDrawables()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    //
    // Clean up old render targets and render passes
    m_LinearColorRT.Release();
    m_TonemapRT.Release();

    if (m_BlitRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(pVulkan->m_VulkanDevice, m_BlitRenderPass, nullptr);
        m_BlitRenderPass = VK_NULL_HANDLE;
    }

    //
    // Setup formats for the first pass
    std::vector<VkFormat> PassColorFormats = { VK_FORMAT_A2B10G10R10_UNORM_PACK32 };// VK_FORMAT_R8G8B8A8_UNORM };// { VK_FORMAT_R16G16B16A16_SFLOAT };
    // std::vector<VkFormat> PassColorFormats = { VK_FORMAT_R8G8B8A8_UNORM };
    std::vector<VkSampleCountFlagBits> PassColorMsaa = { gMsaa };
    std::vector<TEXTURE_TYPE> PassTextureTypes;

    const VkFormat DepthFormat = pVulkan->GetBestVulkanDepthFormat();
    const bool NeedsResolve = gMsaa != VK_SAMPLE_COUNT_1_BIT;

    VkRenderPass ObjectRenderPass = VK_NULL_HANDLE;
    if( gUseSubpasses )
    {
        //
        // Create a single RenderPass with subpasses for objects and then tonemapping
        //

        PassTextureTypes = { TT_RENDER_TARGET_SUBPASS };
        if(NeedsResolve && !gShaderResolve)
        {
            // Intermediate MSAA target for output of tonemap
            PassColorFormats.push_back( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
            PassColorMsaa.push_back( gMsaa );
            PassTextureTypes.push_back( TT_RENDER_TARGET_SUBPASS );
        }
        // Resolved Output
        PassColorFormats.push_back( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
        PassColorMsaa.push_back( VK_SAMPLE_COUNT_1_BIT );
        PassTextureTypes.push_back( TT_RENDER_TARGET );

        // Create a compatible RenderPass
        if( !Create2SubpassRenderPass( { &PassColorFormats[0],1 }, { &PassColorFormats[1],1 }, DepthFormat, *PassColorMsaa.cbegin(), *PassColorMsaa.crbegin(), &ObjectRenderPass ) )
        {
            LOGE( "Unable to create render pass" );
            return false;
        }
        pVulkan->SetDebugObjectName( ObjectRenderPass, "CombinedRenderPass" );

        m_TonemapSubPassIdx = 1;
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
                                        &ObjectRenderPass ) )
        {
            return false;
        }
        pVulkan->SetDebugObjectName( ObjectRenderPass, "ObjectRenderPass" );

        // Tonemap render pass
        VkRenderPass TonemapRenderPass = VK_NULL_HANDLE;

        std::vector<VkFormat> TonemapColorAndResolveFormats{ VK_FORMAT_A2B10G10R10_UNORM_PACK32 };
        // std::vector<VkFormat> TonemapColorAndResolveFormats{ VK_FORMAT_R8G8B8A8_UNORM };
        tcb::span<const VkFormat> TonemapResolveFormat{};
        std::vector<VkSampleCountFlagBits> TonemapColorAndResolveMsaa{ PassColorMsaa.back() };
        std::vector<TEXTURE_TYPE> TonemapColorAndResolveTextureTypes{ TT_RENDER_TARGET };

        if( NeedsResolve )
        {
            // Setup to have an additional 1xMSAA buffer for the Resolve after the tonemap
            TonemapColorAndResolveFormats.push_back( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
            TonemapResolveFormat = { &TonemapColorAndResolveFormats.back(), 1 };
            TonemapColorAndResolveMsaa.push_back( VK_SAMPLE_COUNT_1_BIT );
            TonemapColorAndResolveTextureTypes.push_back( TT_RENDER_TARGET );
        }

        tcb::span<const VkFormat> TonemapColorFormat{ &TonemapColorAndResolveFormats.front(),1 };
        if( !pVulkan->CreateRenderPass( TonemapColorFormat,
                                        VK_FORMAT_UNDEFINED,
                                        TonemapColorAndResolveMsaa.front(),
                                        RenderPassInputUsage::DontCare/*color*/,
                                        RenderPassOutputUsage::StoreReadOnly,/*color*/
                                        false,/*dont clear depth*/
                                        RenderPassOutputUsage::Discard,/*depth*/
                                        &TonemapRenderPass,
                                        TonemapResolveFormat) )
        {
            return false;
        }
        pVulkan->SetDebugObjectName( TonemapRenderPass, "TonemapRenderPass" );

        // Create the render target for the Tonemap (only needed when passes are seperate - subpass variant has all the rendertarget buffers for both subpasses in m_LinearColorRT)
        if( !m_TonemapRT.Initialize( pVulkan, gRenderWidth, gRenderHeight, TonemapColorAndResolveFormats, VK_FORMAT_UNDEFINED, TonemapRenderPass/*takes ownership*/, (VkRenderPass) VK_NULL_HANDLE, TonemapColorAndResolveMsaa, "Tonemap RT", TonemapColorAndResolveTextureTypes ) )
        {
            LOGE( "Error initializing LinearColorRT" );
            return false;
        }

        m_TonemapSubPassIdx = 0;    // not subpassed!
    }

    // Create render target(s) for the scene render (sub) passes.
    if (!m_LinearColorRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, PassColorFormats, DepthFormat, ObjectRenderPass, (VkRenderPass)VK_NULL_HANDLE, PassColorMsaa, "Main RT", PassTextureTypes))
    {
        LOGE("Error initializing LinearColorRT");
        return false;
    }

    // Blit render pass
    if( !pVulkan->CreateRenderPass( { &pVulkan->m_SurfaceFormat, 1 },
                                    pVulkan->m_SwapchainDepth.format,
                                    VK_SAMPLE_COUNT_1_BIT,
                                    RenderPassInputUsage::DontCare/*color*/,
                                    RenderPassOutputUsage::Present,/*color*/
                                    false,/*dont clear depth*/
                                    RenderPassOutputUsage::Discard,/*depth*/
                                    &m_BlitRenderPass ) )
    {
        return false;
    }
    pVulkan->SetDebugObjectName( m_BlitRenderPass, "BlitRenderPass" );

    //
    // Setup the drawables using the correct render passes / subpasses
    //

    // Scene (object) drawable
    LoadSceneDrawables( ObjectRenderPass, 0, gMsaa );

    if( gUseSubpasses )
    {
        //
        // Tonemap drawable
        //
        ImageInfo tonemapDiffuseImageInfo( m_LinearColorRT[0].m_ColorAttachments.front() );
        tonemapDiffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::map<const std::string, const ImageInfo> tonemapImages = { {"Diffuse", std::move(tonemapDiffuseImageInfo) } };

        const char* pShaderName = "TonemapSubpassMsaa";
        if( gShaderResolve )
        {
            switch( PassColorMsaa.front() ) {
            default:                     pShaderName = "TonemapSubpassShaderResolve1x"; break;
            case VK_SAMPLE_COUNT_2_BIT:  pShaderName = "TonemapSubpassShaderResolve2x"; break;
            case VK_SAMPLE_COUNT_4_BIT:  pShaderName = "TonemapSubpassShaderResolve4x"; break;
            }
        }
        // Tonemapping runs in second sub-pass at the MSAA samplerate of its destination buffer (either the shader resolve output buffer @ 1xMSAA or the pre hardware resolve output buffer @ 4xMSAA etc)
        m_TonemapDrawable = InitFullscreenDrawable( pShaderName, {}, tonemapImages, ObjectRenderPass, m_TonemapSubPassIdx, PassColorMsaa[1] );

        //
        // Blit drawable
        //
        std::map<const std::string, const VulkanTexInfo*> blitInputs = { {"Diffuse", &m_LinearColorRT[0].m_ColorAttachments.back() },
                                                                         {"Overlay", &m_GuiRT[0].m_ColorAttachments.front() },
        };

        m_BlitDrawable = InitFullscreenDrawable( "Blit", blitInputs, {}, m_BlitRenderPass, 0, VK_SAMPLE_COUNT_1_BIT );
    }
    else
    {
        //
        // Tonemap drawable
        //
        std::map<const std::string, const VulkanTexInfo*> tonemapTex = { {"Diffuse", &m_LinearColorRT[0].m_ColorAttachments.front() } };
        if( PassColorMsaa.front() == VK_SAMPLE_COUNT_1_BIT )
        {
            m_TonemapDrawable = InitFullscreenDrawable( "Tonemap", tonemapTex, {}, m_TonemapRT.m_RenderPass, 0, PassColorMsaa.front() );
        }
        else
        {
            m_TonemapDrawable = InitFullscreenDrawable( "TonemapMsaa", tonemapTex, {}, m_TonemapRT.m_RenderPass, 0, PassColorMsaa.front() );
        }

        //
        // Blit drawable
        //
        std::map<const std::string, const VulkanTexInfo*> blitInputs = { {"Diffuse", &m_TonemapRT[0].m_ColorAttachments.back() },
                                                                         {"Overlay", &m_GuiRT[0].m_ColorAttachments.front() },
        };
        m_BlitDrawable = InitFullscreenDrawable( "Blit", blitInputs, {}, m_BlitRenderPass, 0, VK_SAMPLE_COUNT_1_BIT );
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadSceneDrawables( VkRenderPass renderPass, uint32_t subpassIdx, VkSampleCountFlagBits passMsaa )
//-----------------------------------------------------------------------------
{
    const char* pGLTFMeshFile = "./Media/Meshes/Museum.gltf";

    std::string texturesPath("Media\\");
    

    LOGI( "Loading Scene Drawable %s", pGLTFMeshFile );

    Vulkan* pVulkan = m_vulkan.get();

    const auto* pObjectShader = m_ShaderManager->GetShader( "Object" );
    if( !pObjectShader )
    {
        // Error (missing shader)
        return false;
    }

    // Lambda to load a texture for the given material (slot)
    const auto& textureLoader = [&]( const MeshObjectIntermediate::MaterialDef& materialDef, const std::string& textureSlotName ) -> const MaterialPass::tPerFrameTexInfo
    {
        std::string textureName = materialDef.diffuseFilename;
        if (textureName.empty())
        {
            return { &m_TexWhite };
        }

        auto texturePath = std::filesystem::path(textureName);
        if (!texturePath.has_filename())
        {
            return { &m_TexWhite };
        }

        auto textureFilename = texturePath.stem();

        auto textureIt = m_LoadedTextures.find(textureFilename.string());
        if (textureIt == m_LoadedTextures.end())
        {
            // Prepare the texture path
            std::string textureInternalPath = gTextureFolder;
            textureInternalPath.append(textureFilename.string());
            textureInternalPath.append(".ktx");

            auto loadedTexture = LoadKTXTexture(pVulkan, *m_AssetManager, textureInternalPath.c_str(), VK_SAMPLER_ADDRESS_MODE_REPEAT);
            if (!loadedTexture.IsEmpty())
            {
                textureIt = m_LoadedTextures.insert({ textureFilename.string() , std::move(loadedTexture) }).first;
            }
            else
            {
                return { &m_TexWhite };
            }
        }

        assert(!textureIt->second.IsEmpty());

        return { &textureIt->second };
    };

    // Lambda to associate (uniform) buffers with their shader binding/slot names.
    const auto& bufferLoader = [&]( const std::string& bufferSlotName ) -> MaterialPass::tPerFrameVkBuffer {
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
        if (materialDef.diffuseFilename.find("ChineseVaseA_ChineseVaseA_MAT_BaseColor") == -1)
        {
            // render ChineseVase only to make the scene simple
            return {};
        }
        
        return std::move( m_MaterialManager->CreateMaterial( *pVulkan, *pObjectShader, NUM_VULKAN_BUFFERS, std::bind( textureLoader, std::cref( materialDef ), _1 ), bufferLoader ) );
    };


    static const char* opaquePassName = "RP_OPAQUE";
    const VkSampleCountFlagBits renderPassMultisamples[1] = { passMsaa };
    const uint32_t renderPassSubpasses[1] = { subpassIdx };

    m_SceneObject.clear();

    if( !DrawableLoader::LoadDrawables( *pVulkan, *m_AssetManager, { &renderPass,1 }, &opaquePassName, pGLTFMeshFile, materialLoader, m_SceneObject, renderPassMultisamples, DrawableLoader::LoaderFlags::None, renderPassSubpasses ) )
    {
        LOGE( "Error loading Object mesh: %s", pGLTFMeshFile );
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
std::unique_ptr<Drawable> Application::InitFullscreenDrawable( const char* pShaderName, const std::map<const std::string, const VulkanTexInfo*>& ColorAttachmentsLookup, const std::map<const std::string, const ImageInfo>& ImageAttachmentsLookup, VkRenderPass renderPass, uint32_t subpassIdx, VkSampleCountFlagBits passMsaa )
//-----------------------------------------------------------------------------
{
    //
    // Tonemap pass is a fullscreen quad, setup a 'drawable'.
    //

    Vulkan* pVulkan = m_vulkan.get();

    LOGI("Creating %s mesh...", pShaderName);

    const auto* pShader = m_ShaderManager->GetShader(pShaderName);
    assert(pShader);
    if( !pShader )
    {
        LOGE("Error, %s shader is unknown (not loaded?)", pShaderName);
        return nullptr;
    }

    MeshObject mesh;
    if (!MeshObject::CreateMesh(pVulkan, MeshObjectIntermediate::CreateScreenSpaceMesh(), 0, pShader->m_shaderDescription->m_vertexFormats, &mesh))
    {
        LOGE("Error creating Fullscreen Mesh (for %s)", pShaderName);
        return nullptr;
    }

    auto shaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pShader, NUM_VULKAN_BUFFERS,
        [this, &ColorAttachmentsLookup](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo {
            return { ColorAttachmentsLookup.find(texName)->second };
            assert(0);
            return {};
        },
        []( const std::string& bufferName ) -> const MaterialPass::tPerFrameVkBuffer {
            assert(0);
            return {};
        },
        [this, &ImageAttachmentsLookup]( const std::string& imageName ) -> const ImageInfo {
            return { ImageAttachmentsLookup.find(imageName)->second };
        }
    );

    static const char* passName = "Fullscreen";
    const VkSampleCountFlagBits renderPassMultisamples[1] = { passMsaa };
    const uint32_t renderPassSubpasses[1] = { subpassIdx };

    auto drawable = std::make_unique<Drawable>( *pVulkan, std::move( shaderMaterial ) );
    if( !drawable->Init( renderPass, passName, std::move(mesh), std::nullopt, std::nullopt, renderPassMultisamples, renderPassSubpasses ) )
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
    Vulkan* pVulkan = m_vulkan.get();

    char szName[256];
    for( uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++ )
    {
        sprintf( szName, "CommandBuffer (%d of %d)", WhichBuffer + 1, NUM_VULKAN_BUFFERS );
        if( !m_CommandBuffer[WhichBuffer].Initialize( pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false ) )
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

    auto* pVulkan = m_vulkan.get();

    // See if we want to change the MSAA mode
    if( gShaderResolve != m_RequestedShaderResolve || gUseSubpasses != m_RequestedUseSubpasses || gMsaa != m_RequestedMsaa || gUseRenderPassTransform != m_RequestedUseRenderPassTransform)
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
    ObjectVertUniform data{ };
    data.MVPMatrix = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix();
    data.ModelMatrix = glm::identity<glm::mat4>();
    UpdateUniformBuffer( m_vulkan.get(), m_ObjectVertUniform, data );

    return true;
}


//-----------------------------------------------------------------------------
bool Application::UpdateCommandBuffer(uint32_t bufferIdx)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    auto& commandBuffer = m_CommandBuffer[bufferIdx];

    if( !commandBuffer.Begin( m_LinearColorRT[0].m_FrameBuffer, m_LinearColorRT.m_RenderPass ) )
    {
        return false;
    }

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = m_LinearColorRT[0].m_Width;
    Scissor.extent.height = m_LinearColorRT[0].m_Height;

    VkClearColorValue ClearColor{};
    std::fill( std::begin(ClearColor.float32), std::end(ClearColor.float32), 0.0f);
    ClearColor.float32[0] = 0.1f;

    if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, m_LinearColorRT[0].GetNumColorLayers(), true, m_LinearColorRT.m_RenderPass, false, m_LinearColorRT[0].m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE  ) )
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

        if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, 1, true, m_TonemapRT.m_RenderPass, false, m_TonemapRT[0].m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE ) )
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
        if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, 1, true, m_GuiRT.m_RenderPass, false, m_GuiRT[0].m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE ) )
        {
            return false;
        }
        m_Gui->Render(commandBuffer.m_VkCommandBuffer);

        commandBuffer.EndRenderPass();
    }

    //
    // Now add the final Blit to swapchain backbuffer...
    //

    Scissor.extent.width = pVulkan->m_SurfaceWidth;
    Scissor.extent.height = pVulkan->m_SurfaceHeight;

    if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, { &ClearColor , 1 }, 1, false, m_BlitRenderPass, true/*swapchain*/, pVulkan->m_pSwapchainFrameBuffers[bufferIdx], VK_SUBPASS_CONTENTS_INLINE ) )
    {
        return false;
    }

    AddDrawableToCmdBuffers( *m_BlitDrawable, &commandBuffer, 1, 1 );

    commandBuffer.EndRenderPass();

    commandBuffer.End();

    return true;
}


//-----------------------------------------------------------------------------
bool Application::Create2SubpassRenderPass( const tcb::span<const VkFormat> InternalColorFormats, const tcb::span<const VkFormat> OutputColorFormats, VkFormat InternalDepthFormat, VkSampleCountFlagBits InternalMsaa, VkSampleCountFlagBits OutputMsaa, VkRenderPass* pRenderPass/*out*/ )
//-----------------------------------------------------------------------------
{
    assert(pRenderPass && *pRenderPass == VK_NULL_HANDLE);  // check not already allocated and that we have a location to place the renderpass handle
    assert( !InternalColorFormats.empty() );                // not supporting a depth only pass
    assert( !OutputColorFormats.empty() );

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

    const bool HasDepth = InternalDepthFormat != VK_FORMAT_UNDEFINED;
    const bool HasPass2ReadDepth = HasDepth && false;

    //
    // First Pass Color Attachments (what is written by the first pass)
    // Also setup as inputs to the second pass.
    for(const auto& ColorFormat: InternalColorFormats)
    {
        // Pass0 color and depth buffers setup to clear on load, discard on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass0 = { 0/*flags*/,
            ColorFormat/*format*/,
            InternalMsaa/*samples*/,
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
        VkAttachmentDescription AttachmentDescPass1 = { 0, ColorFormat/*format*/,
            (gShaderResolve||!NeedsResolve) ? OutputMsaa : InternalMsaa/*samples*/,
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
            VkAttachmentDescription AttachmentDescResolvePass1 = { 0, ColorFormat/*format*/,
                OutputMsaa/*samples*/,
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
            InternalDepthFormat/*format*/,
            InternalMsaa/*samples*/,
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
        assert( OutputMsaa == VK_SAMPLE_COUNT_1_BIT );
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
    VkRenderPassCreateInfo RenderPassInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.flags = 0;
    RenderPassInfo.attachmentCount = (uint32_t) PassAttachDescs.size();
    RenderPassInfo.pAttachments = PassAttachDescs.data();
    RenderPassInfo.subpassCount = (uint32_t) SubpassDesc.size();
    RenderPassInfo.pSubpasses = SubpassDesc.data();
    RenderPassInfo.dependencyCount = (uint32_t) PassDependencies.size();
    RenderPassInfo.pDependencies = PassDependencies.data();

    VkResult RetVal = vkCreateRenderPass(m_vulkan->m_VulkanDevice, &RenderPassInfo, NULL, pRenderPass);
    if (!CheckVkError("vkCreateRenderPass()", RetVal))
    {
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
void Application::ChangeMsaaMode()
//-----------------------------------------------------------------------------
{
    gShaderResolve = m_RequestedShaderResolve;
    gUseSubpasses  = m_RequestedUseSubpasses;
    gMsaa = m_RequestedMsaa;
    gUseRenderPassTransform = m_RequestedUseRenderPassTransform;

    m_vulkan->m_UseRenderPassTransform = gUseRenderPassTransform;

    m_vulkan->RecreateSwapChain();

    InitFramebuffersRenderPassesAndDrawables();
}


//-----------------------------------------------------------------------------
bool Application::InitHdr()
//-----------------------------------------------------------------------------
{
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
    return m_vulkan->SetSwapchainHrdMetadata(AuthoringProfile);
}


//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    const VkFormat GuiColorFormat[]{ VK_FORMAT_R8G8B8A8_UNORM };
    const TEXTURE_TYPE GuiTextureTypes[]{ TT_RENDER_TARGET };

    if( !m_GuiRT.Initialize( pVulkan, gRenderWidth, gRenderHeight, GuiColorFormat, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "Gui RT", GuiTextureTypes ) )
    {
        return false;
    }

    m_Gui = std::make_unique<GuiImguiVulkan>(*pVulkan, m_GuiRT.m_RenderPass);
    if (!m_Gui->Initialize(windowHandle, m_GuiRT[0].m_Width, m_GuiRT[0].m_Height))
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
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
            if( m_vulkan->GetExtRenderPassShaderResolveAvailable() )
            {
                ImGui::Checkbox( "Shader resolve", &m_RequestedShaderResolve );
                ImGui::Checkbox("Use Subpasses with Shader Resolve", &m_RequestedUseSubpasses);
            }
            else
            {
                ImGui::Text( "Shader Resolve extension not available" );
            }            

            int iMsaa = (int) std::bitset<32>(m_RequestedMsaa - 1).count();
            ImGui::Combo( "MSAA", &iMsaa, "x1\0x2\0x4\0" );
            m_RequestedMsaa = (VkSampleCountFlagBits) (1<<iMsaa);
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
    Vulkan* pVulkan = m_vulkan.get();

    m_SceneObject.clear();
    m_TonemapDrawable.reset();
    m_BlitDrawable.reset();

    for (auto& texture : m_LoadedTextures)
    {
        ReleaseTexture(pVulkan, &texture.second);
    }

    ReleaseUniformBuffer( pVulkan, &m_ObjectVertUniform );
    ReleaseUniformBuffer( pVulkan, &m_ObjectFragUniform );

    m_GuiRT.Release();
    m_TonemapRT.Release();
    m_LinearColorRT.Release();

    m_MaterialManager.reset();
    m_ShaderManager.reset();

    if( m_BlitRenderPass != VK_NULL_HANDLE )
    {
        vkDestroyRenderPass( pVulkan->m_VulkanDevice, m_BlitRenderPass, nullptr );
        m_BlitRenderPass = VK_NULL_HANDLE;
    }

    // Finally call into base class destroy
    ApplicationHelperBase::Destroy();
}
