//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

///
/// @file rotatedCopy.hpp
/// @brief Sample app for VK_QCOM_rotated_copy_commands extension.
/// 
/// Very simple application that uses VK_QCOM_rotated_copy_commands to blit the final image to the present framebuffer with pre-rotation.
/// 

#include "main/applicationHelperBase.hpp"
#include <map>
#include <string>
#include "tcb/span.hpp" // replace with c++20 <span>

// Forward declarations
class Drawable;
class ShaderManager;
class MaterialManager;
struct ImageInfo;


class Application : public ApplicationHelperBase
{
public:
    Application();
    ~Application() override;

    //
    // Override ApplicationHelperBase
    //
    /// Override Vulkan application configuration
    void PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration&) override;
    /// Override surface format selection.
    int PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR> formats) override;
    /// Override Application entry point!
    bool Initialize(uintptr_t windowHandle) override;

    bool LoadShaders();

    bool InitFramebuffersRenderPassesAndDrawables();
    bool LoadSceneDrawables( VkRenderPass renderPass, const tcb::span<const VkSampleCountFlagBits> renderPassMultisamples );
    std::unique_ptr<Drawable> InitFullscreenDrawable( const char* pShaderName, const std::map<const std::string, const VulkanTexInfo*>& inputLookup, const std::map<const std::string, const ImageInfo>& ImageAttachmentsLookup, VkRenderPass renderPass, uint32_t subpassIdx, const tcb::span<const VkSampleCountFlagBits> renderPassMultisamples);
    bool InitCommandBuffers();
    bool InitCamera() override;

    /// @brief Ticked every frame (by the Framework)
    /// @param fltDiffTime time (in seconds) since the last call to Render.
    void Render( float fltDiffTime ) override;

    void UpdateCamera( float elapsedTime );
    bool UpdateUniforms( uint32_t bufferIdx );
    bool UpdateCommandBuffer( uint32_t bufferIdx );

    //
    // Blit helpers
    //

    /// Adds a rotated blit command to the commandBuffer.
    /// Blits from the m_TonemapRT to the given swapchain image.
    void AddRotatedSwapchainBlitToCmdBuffer(Wrap_VkCommandBuffer& commandBuffer, const CRenderTarget& srcRT, VkImage swapchainImage);

    /// Adds a (non rotated) blit command to the commandBuffer.
    /// Blits from the m_TonemapRT to the given swapchain image.
    void AddSwapchainBlitToCmdBuffer(Wrap_VkCommandBuffer& commandBuffer, const CRenderTarget& srcRT, VkImage swapchainImage);


    void ChangeSwapchainMode();

    bool InitGui( uintptr_t windowHandle );
    void UpdateGui();

    /// Shutdown function
    void Destroy() override;

private:
    // Textures
    std::map<std::string, VulkanTexInfo>    m_LoadedTextures;
    VulkanTexInfo                           m_TexWhite;

    // Shaders
    std::unique_ptr<ShaderManager>          m_ShaderManager;

    // Materials
    std::unique_ptr<MaterialManager>        m_MaterialManager;

    // Drawable(s) for rendering the main scene object
    std::vector<Drawable>                   m_SceneObject;

    // Drawable for tonemapping
    std::unique_ptr<Drawable>               m_TonemapDrawable;

    // Uniform buffers
    struct ObjectVertUniform
    {
        glm::mat4 MVPMatrix;
        glm::mat4 ModelMatrix;
    };
    UniformT<ObjectVertUniform>             m_ObjectVertUniform;
    struct ObjectfragUniform
    {
        float unused;
    };
    UniformT<ObjectfragUniform>             m_ObjectFragUniform;

    /// Render target for 3d Objects.
    CRenderTargetArray<1>                   m_SceneRT;

    /// Render target for tonemap (also the composited ui)
    CRenderTargetArray<1>                   m_TonemapRT;

    /// Render target for GUI.
    CRenderTargetArray<1>                   m_GuiRT;

    /// Single primary command buffer
    Wrap_VkCommandBuffer                    m_CommandBuffer[NUM_VULKAN_BUFFERS];

    /// Modified by UI to request enable/disable of the final rotated blit
    bool                                    m_RequestedRotatedFinalBlit = false;

    // function pointers
    PFN_vkCmdBlitImage2KHR                  m_fpCmdBlitImage2KHR = nullptr;
};
