//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

///
/// Sample app for VK_QCOM_rotated_copy_commands extension
///

#include "rotatedCopy.hpp"
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

// Global Variables From Config File
//VAR(glm::vec3,  gCameraStartPos,    glm::vec3(0.0f, 0.0f, 2.5f), kVariableNonpersistent);
//VAR(glm::vec3,  gCameraStartRot,    glm::vec3(0.0f, 0.0f, 0.0f), kVariableNonpersistent);
//VAR(glm::vec3, gCameraStartPos, glm::vec3(-33.7f, 7.6f, 34.7f), kVariableNonpersistent);
//VAR(glm::vec3, gCameraStartRot, glm::vec3(-10.0f, 200.0f, 0.0f), kVariableNonpersistent);
//VAR(glm::vec3, gCameraStartPos, glm::vec3(46.9f, 4.8f, 34.7f), kVariableNonpersistent);
//VAR(glm::vec3, gCameraStartRot, glm::vec3(0.0f, -30.0f, 0.0f), kVariableNonpersistent);
VAR(glm::vec3, gCameraStartPos, glm::vec3(26.48f, 20.0f, -5.21f), kVariableNonpersistent);
VAR(glm::vec3, gCameraStartRot, glm::vec3(0.0f, 110.0f, 0.0f), kVariableNonpersistent);
//glm::vec3 gCameraStartPos = glm::vec3(26.48f, 20.0f, -5.21f);
//glm::vec3 gCameraStartRot = glm::vec3(0.0f, 110.0f, 0.0f);

VAR(float,      gFOV,               PI_DIV_4, kVariableNonpersistent);
VAR(float,      gNearPlane,         0.1f, kVariableNonpersistent);
VAR(float,      gFarPlane,          80.0f, kVariableNonpersistent);

VAR(bool,       gRotatedFinalBlit,  true, kVariableNonpersistent);

namespace
{
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
    m_vulkan->m_UseRenderPassTransform = false;
    m_vulkan->m_UsePreTransform = gRotatedFinalBlit;

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
void Application::PreInitializeSetVulkanConfiguration( Vulkan::AppConfiguration& config )
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::PreInitializeSetVulkanConfiguration( config );
    config.RequiredExtension( "VK_KHR_copy_commands2" );
    config.RequiredExtension( "VK_QCOM_rotated_copy_commands" );
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

    // Get the pointer to the rotated blit function.
    m_fpCmdBlitImage2KHR = (PFN_vkCmdBlitImage2KHR)vkGetInstanceProcAddr(pVulkan->GetVulkanInstance(), "vkCmdBlitImage2KHR");
    if (m_fpCmdBlitImage2KHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkCmdBlitImage2KHR");
        // Disable rotated blit as we can't get the function pointer.
        gRotatedFinalBlit = false;
    }

    m_RequestedRotatedFinalBlit = gRotatedFinalBlit;

    LoadShaders();

    CreateUniformBuffer( pVulkan, m_ObjectVertUniform );
    CreateUniformBuffer( pVulkan, m_ObjectFragUniform );

    m_MaterialManager = std::make_unique<MaterialManager>();

    //m_TexWhite = m_LoadedTextures.emplace( "textures/white_d.ktx", LoadKTXTexture( pVulkan, *m_AssetManager, "./Media/Textures/white_d.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT ) );
    m_TexWhite = LoadKTXTexture(pVulkan, *m_AssetManager, "./Media/Textures/white_d.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT);
    // m_LoadedTextures.emplace( "Skull_Diffuse.ktx", LoadKTXTexture( pVulkan, *m_AssetManager, "./Media/Textures/Skull_Diffuse.ktx", VK_SAMPLER_ADDRESS_MODE_REPEAT ) );

    InitGui(windowHandle);

    InitCommandBuffers();

    InitFramebuffersRenderPassesAndDrawables();

    InitCamera();

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
            tIdAndFilename { "Tonemap",                       "Media\\Shaders\\Tonemap.json"}
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
    m_SceneRT.Release();
    m_TonemapRT.Release();


    //
    // Create two seperate passes - Scene and Tonemap
    //

    const VkFormat ScenePassColorFormats[] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32 };
    const TEXTURE_TYPE ScenePassTextureTypes[] = { TT_RENDER_TARGET };
    const VkSampleCountFlagBits ScenePassMsaa[] = { VK_SAMPLE_COUNT_1_BIT };
    const VkFormat ScenePassDepthFormat = pVulkan->GetBestVulkanDepthFormat();

    const VkFormat TonemapPassColorFormats[] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32 };
    const VkSampleCountFlagBits TonemapPassMsaa[] = { VK_SAMPLE_COUNT_1_BIT };
    const TEXTURE_TYPE TonemapPassTextureTypes[] = { TT_RENDER_TARGET_TRANSFERSRC };

    //
    // Scene render pass
    VkRenderPass SceneRenderPass = VK_NULL_HANDLE;
    if( !pVulkan->CreateRenderPass( ScenePassColorFormats,
                                    ScenePassDepthFormat,
                                    VK_SAMPLE_COUNT_1_BIT,
                                    RenderPassInputUsage::Clear/*color*/,
                                    RenderPassOutputUsage::StoreReadOnly,/*color*/
                                    true,/*clear depth*/
                                    RenderPassOutputUsage::Discard,/*depth*/
                                    &SceneRenderPass ) )
    {
        return false;
    }
    pVulkan->SetDebugObjectName( SceneRenderPass, "SceneRenderPass" );

    // Create render target for 3d scene.
    if (!m_SceneRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, ScenePassColorFormats, ScenePassDepthFormat, SceneRenderPass, (VkRenderPass)VK_NULL_HANDLE, ScenePassMsaa, "Scene RT", ScenePassTextureTypes))
    {
        LOGE("Error initializing LinearColorRT");
        return false;
    }

    // Load Scene (object) drawable
    LoadSceneDrawables(SceneRenderPass, ScenePassMsaa);


    //
    // Tonemap render pass
    VkRenderPass TonemapRenderPass = VK_NULL_HANDLE;
    if( !pVulkan->CreateRenderPass( TonemapPassColorFormats,
                                    VK_FORMAT_UNDEFINED,
                                    VK_SAMPLE_COUNT_1_BIT,
                                    RenderPassInputUsage::DontCare/*color*/,
                                    RenderPassOutputUsage::StoreTransferSrc,/*color*/
                                    false,/*dont clear depth*/
                                    RenderPassOutputUsage::Discard,/*depth*/
                                    &TonemapRenderPass) )
    {
        return false;
    }
    pVulkan->SetDebugObjectName( TonemapRenderPass, "TonemapRenderPass" );

    // Create the render target for the Tonemap/composite
    if( !m_TonemapRT.Initialize( pVulkan, gRenderWidth, gRenderHeight, TonemapPassColorFormats, VK_FORMAT_UNDEFINED, TonemapRenderPass/*takes ownership*/, (VkRenderPass) VK_NULL_HANDLE, TonemapPassMsaa, "Tonemap RT", TonemapPassTextureTypes ) )
    {
        LOGE( "Error initializing LinearColorRT" );
        return false;
    }

    // Tonemap drawable
    std::map<const std::string, const VulkanTexInfo*> tonemapTex = { {"Diffuse", &m_SceneRT[0].m_ColorAttachments.front() },
                                                                     {"Overlay", &m_GuiRT[0].m_ColorAttachments.front() } };
    m_TonemapDrawable = InitFullscreenDrawable( "Tonemap", tonemapTex, {}, m_TonemapRT.m_RenderPass, 0, TonemapPassMsaa );

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadSceneDrawables( VkRenderPass renderPass, const tcb::span<const VkSampleCountFlagBits> renderPassMultisamples )
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
    const auto& textureLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef, const std::string& textureSlotName) -> const MaterialPass::tPerFrameTexInfo
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
    const auto& bufferLoader = [&](const std::string& bufferSlotName) -> MaterialPass::tPerFrameVkBuffer {
        if (bufferSlotName == "Vert")
        {
            return { m_ObjectVertUniform.buf.GetVkBuffer() };
        }
        else if (bufferSlotName == "Frag")
        {
            return { m_ObjectFragUniform.buf.GetVkBuffer() };
        }
        else
        {
            assert(0);
            return {};
        }
    };

    const auto& materialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef) -> std::optional<Material>
    {
        using namespace std::placeholders;
        if (materialDef.diffuseFilename.find("ChineseVaseA") == -1)
        {
            // render ChineseVase only to make the scene simple
            // return {};
        }

        return std::move(m_MaterialManager->CreateMaterial(*pVulkan, *pObjectShader, NUM_VULKAN_BUFFERS, std::bind(textureLoader, std::cref(materialDef), _1), bufferLoader));
    };


    static const char* opaquePassName = "RP_OPAQUE";
    const uint32_t renderPassSubpasses[] = { 0 };

    m_SceneObject.clear();

    if( !DrawableLoader::LoadDrawables( *pVulkan, *m_AssetManager, { &renderPass,1 }, &opaquePassName, pGLTFMeshFile, materialLoader, m_SceneObject, renderPassMultisamples, DrawableLoader::LoaderFlags::None, renderPassSubpasses ) )
    {
        LOGE( "Error loading Object mesh: %s", pGLTFMeshFile );
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
std::unique_ptr<Drawable> Application::InitFullscreenDrawable( const char* pShaderName, const std::map<const std::string, const VulkanTexInfo*>& ColorAttachmentsLookup, const std::map<const std::string, const ImageInfo>& ImageAttachmentsLookup, VkRenderPass renderPass, uint32_t subpassIdx, const tcb::span<const VkSampleCountFlagBits> renderPassMultisamples )
//-----------------------------------------------------------------------------
{
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
    const uint32_t renderPassSubpasses[1] = { subpassIdx };

    auto drawable = std::make_unique<Drawable>( *pVulkan, std::move( shaderMaterial ) );
    if( !drawable->Init( renderPass, passName, std::move(mesh), std::nullopt, std::nullopt, &renderPassMultisamples[0], &renderPassSubpasses[0] ) )
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
bool Application::InitCamera()
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::InitCamera())
    {
        return false;
    }
    m_CameraController->SetMoveSpeed(0.001f);

    // Camera
    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    return true;
}


//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    UpdateGui();

    UpdateCamera( fltDiffTime );

    auto* pVulkan = m_vulkan.get();

    // See if we want to change swapchain/rotate mode
    if( gRotatedFinalBlit != m_RequestedRotatedFinalBlit)
    {
        ChangeSwapchainMode();
    }

    auto CurrentVulkanBuffer = pVulkan->SetNextBackBuffer();

    UpdateUniforms( CurrentVulkanBuffer.idx );

    UpdateCommandBuffer( CurrentVulkanBuffer.idx );

    // ... submit the command buffer to the device queue
    m_CommandBuffer[CurrentVulkanBuffer.idx].QueueSubmit( CurrentVulkanBuffer, pVulkan->m_RenderCompleteSemaphore );
    // and Present
    PresentQueue( pVulkan->m_RenderCompleteSemaphore, CurrentVulkanBuffer.swapchainPresentIdx );
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

    if( !commandBuffer.Begin( m_SceneRT[0].m_FrameBuffer, m_SceneRT.m_RenderPass ) )
    {
        return false;
    }

    VkImage swapchainImage = pVulkan->m_pSwapchainBuffers[bufferIdx].image;

    {
        // Need to transition the swapchain before it can be blitted to (not needed if written by a render pass)
        VkImageMemoryBarrier presentPreBlitTransitionBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        presentPreBlitTransitionBarrier.srcAccessMask = 0;
        presentPreBlitTransitionBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentPreBlitTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        presentPreBlitTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentPreBlitTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentPreBlitTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentPreBlitTransitionBarrier.image = swapchainImage;
        presentPreBlitTransitionBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presentPreBlitTransitionBarrier.subresourceRange.baseMipLevel = 0;
        presentPreBlitTransitionBarrier.subresourceRange.levelCount = 1;
        presentPreBlitTransitionBarrier.subresourceRange.baseArrayLayer = 0;
        presentPreBlitTransitionBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(commandBuffer.m_VkCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &presentPreBlitTransitionBarrier);
    }

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = m_SceneRT[0].m_Width;
    Scissor.extent.height = m_SceneRT[0].m_Height;

    VkClearColorValue ClearColor[1] {0.1f, 0.0f, 0.0f, 0.0f};

    if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, ClearColor, m_SceneRT[0].GetNumColorLayers(), true, m_SceneRT.m_RenderPass, false, m_SceneRT[0].m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE  ) )
    {
        return false;
    }

    for(const auto& sceneObjectDrawable : m_SceneObject)
        AddDrawableToCmdBuffers(sceneObjectDrawable, &commandBuffer, 1, 1);

    commandBuffer.EndRenderPass();

    //
    // Do the Gui
    //
    if (m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        if( !commandBuffer.BeginRenderPass( Scissor, 0.0f, 1.0f, ClearColor, 1, true, m_GuiRT.m_RenderPass, false, m_GuiRT[0].m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE ) )
        {
            return false;
        }
        m_Gui->Render(commandBuffer.m_VkCommandBuffer);

        commandBuffer.EndRenderPass();
    }

    //
    // Now add the tonemapping (and add UI overlay)...
    //
    if (!commandBuffer.BeginRenderPass(Scissor, 0.0f, 1.0f, ClearColor, 1, true, m_TonemapRT.m_RenderPass, false, m_TonemapRT[0].m_FrameBuffer, VK_SUBPASS_CONTENTS_INLINE))
    {
        return false;
    }

    AddDrawableToCmdBuffers(*m_TonemapDrawable, &commandBuffer, 1, 1);

    commandBuffer.EndRenderPass();

    if (gRotatedFinalBlit)
    {
        // Rotated blit
        AddRotatedSwapchainBlitToCmdBuffer(commandBuffer, m_TonemapRT[0], swapchainImage);
    }
    else
    {
        // Non rotated blit
        AddSwapchainBlitToCmdBuffer(commandBuffer, m_TonemapRT[0], swapchainImage);
    }

    {
        // Need to transition the swapchain before it can be presented
        VkImageMemoryBarrier presentPostBlitTransitionBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        presentPostBlitTransitionBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentPostBlitTransitionBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        presentPostBlitTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentPostBlitTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentPostBlitTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentPostBlitTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentPostBlitTransitionBarrier.image = swapchainImage;
        presentPostBlitTransitionBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presentPostBlitTransitionBarrier.subresourceRange.baseMipLevel = 0;
        presentPostBlitTransitionBarrier.subresourceRange.levelCount = 1;
        presentPostBlitTransitionBarrier.subresourceRange.baseArrayLayer = 0;
        presentPostBlitTransitionBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(commandBuffer.m_VkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &presentPostBlitTransitionBarrier);
    }

    commandBuffer.End();

    return true;
}


//-----------------------------------------------------------------------------
void Application::AddRotatedSwapchainBlitToCmdBuffer(Wrap_VkCommandBuffer& commandBuffer, const CRenderTarget& srcRT, VkImage swapchainImage)
//-----------------------------------------------------------------------------
{
    //
    // Now add the final Blit to swapchain backbuffer...
    //

    VkImageBlit2KHR blitImageRegion{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR };
    blitImageRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitImageRegion.srcSubresource.layerCount = 1;
    blitImageRegion.srcOffsets[0] = { 0,0,0 };
    blitImageRegion.srcOffsets[1] = { (int)srcRT.m_Width, (int)srcRT.m_Height, 1 };
    blitImageRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitImageRegion.dstSubresource.layerCount = 1;
    blitImageRegion.dstOffsets[0] = { 0,0,0 };
    blitImageRegion.dstOffsets[1] = { (int)m_vulkan->m_SurfaceWidth, (int)m_vulkan->m_SurfaceHeight, 1 };

    VkBlitImageInfo2KHR blitImageInfo{ VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR };
    blitImageInfo.srcImage = srcRT.m_ColorAttachments.back().GetVkImage();
    blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitImageInfo.dstImage = swapchainImage;
    blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitImageInfo.regionCount = 1;
    blitImageInfo.pRegions = &blitImageRegion;
    blitImageInfo.filter = VK_FILTER_NEAREST;

    // If the render surface needs a pre-transofrm (rotation) then add the VkCopyCommandTransformInfoQCOM
    // structure on to the end of the VkImageBlit2KHR region to indicate we want to rotate the blit.
    VkCopyCommandTransformInfoQCOM blitRotate{ VK_STRUCTURE_TYPE_COPY_COMMAND_TRANSFORM_INFO_QCOM };

    blitRotate.transform = m_vulkan->GetPreTransform();
    if (blitRotate.transform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        blitImageRegion.pNext = &blitRotate;
        if (blitRotate.transform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR || blitRotate.transform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
        {
            // The output surface is rotated - so we need to compensate for that.
            std::swap(blitImageRegion.dstOffsets[1].x, blitImageRegion.dstOffsets[1].y);
        }
    }

    // Add the BlitImage2 command to the command buffer.
    m_fpCmdBlitImage2KHR(commandBuffer.m_VkCommandBuffer, &blitImageInfo);
}


//-----------------------------------------------------------------------------
void Application::AddSwapchainBlitToCmdBuffer(Wrap_VkCommandBuffer& commandBuffer, const CRenderTarget& srcRT,  VkImage swapchainImage)
//-----------------------------------------------------------------------------
{
    VkImageBlit blitImageRegion{};
    blitImageRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitImageRegion.srcSubresource.layerCount = 1;
    blitImageRegion.srcOffsets[0] = { 0,0,0 };
    blitImageRegion.srcOffsets[1] = { (int)srcRT.m_Width, (int)srcRT.m_Height, 1 };
    blitImageRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitImageRegion.dstSubresource.layerCount = 1;
    blitImageRegion.dstOffsets[0] = { 0,0,0 };
    blitImageRegion.dstOffsets[1] = { (int)m_vulkan->m_SurfaceWidth, (int)m_vulkan->m_SurfaceHeight, 1 };
    vkCmdBlitImage(commandBuffer.m_VkCommandBuffer, srcRT.m_ColorAttachments.back().GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitImageRegion, VK_FILTER_NEAREST);
}


//-----------------------------------------------------------------------------
void Application::ChangeSwapchainMode()
//-----------------------------------------------------------------------------
{
    gRotatedFinalBlit = m_RequestedRotatedFinalBlit;

    m_vulkan->m_UseRenderPassTransform = false;
    m_vulkan->m_UsePreTransform = gRotatedFinalBlit;

    m_vulkan->RecreateSwapChain();

    InitFramebuffersRenderPassesAndDrawables();
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
            if (m_fpCmdBlitImage2KHR)
            {
                ImGui::Checkbox("Rotated backbuffer Blit", &m_RequestedRotatedFinalBlit);
            }
            else
                ImGui::Text("Rotated blit not available");
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

    vkQueueWaitIdle(m_vulkan->m_VulkanQueue);

    m_SceneObject.clear();
    m_TonemapDrawable.reset();

    for (auto& texture : m_LoadedTextures)
    {
        ReleaseTexture(pVulkan, &texture.second);
    }

    ReleaseUniformBuffer( pVulkan, &m_ObjectVertUniform );
    ReleaseUniformBuffer( pVulkan, &m_ObjectFragUniform );

    m_GuiRT.Release();
    m_TonemapRT.Release();
    m_SceneRT.Release();

    m_MaterialManager.reset();
    m_ShaderManager.reset();

    m_Gui.reset();

    // Finally call into base class destroy
    ApplicationHelperBase::Destroy();
}
