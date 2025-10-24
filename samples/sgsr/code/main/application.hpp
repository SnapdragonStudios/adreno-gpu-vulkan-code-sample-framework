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
#pragma once

#include "main/applicationHelperBase.hpp"
#include "memory/vulkan/uniform.hpp"
#include "vulkan/commandBuffer.hpp"
#include <unordered_map>

enum RENDER_PASS
{
    RP_SGSR = 0,
    RP_HUD,
    RP_BLIT,
    NUM_RENDER_PASSES
};

// **********************
// Uniform Buffers
// **********************
struct SGSRFragUB
{
    glm::vec4 ViewportInfo;
    glm::vec4 TechniqueInfo;
};

// **********************
// Render Pass
// **********************
struct PassSetupInfo
{
    RenderPassInputUsage    ColorInputUsage;
    bool                    ClearDepthRenderPass;
    RenderPassOutputUsage   ColorOutputUsage;
    RenderPassOutputUsage   DepthOutputUsage;
    glm::vec4               ClearColor;
};

struct PassData
{
    // Pass internal data
    PassSetupInfo                       RenderPassSetup;
    std::vector<RenderContext<Vulkan>>  RenderContext;  // context per framebuffer (some passes might all point to the same framebuffers)

    // Render targed used by the underlying render pass
    // note: The blit pass uses the backbuffer targets directly instead this RT
    RenderTarget<Vulkan>                RenderTarget;
};

// **********************
// Application
// **********************
class Application : public ApplicationHelperBase
{
public:
    Application();
    ~Application() override;

    // ApplicationHelperBase
    virtual bool Initialize(uintptr_t windowHandle, uintptr_t hInstance) override;
    virtual void Destroy() override;
    virtual void Render(float fltDiffTime) override;
    virtual int PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat> surfaceFormat) override;
    virtual void PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& configuration) override;

private:

    // Application - Initialization
    bool InitializeCamera();
    bool LoadShaders();
    bool CreateRenderPasses();
    bool CreateRenderTargets();
    bool InitUniforms();
    bool InitGui(uintptr_t windowHandle);
    bool LoadMeshObjects();
    bool InitCommandBuffers();
    bool BuildCmdBuffers();

private:

    // Application - Frame
    void BeginRenderPass(uint32_t WhichBuffer, RENDER_PASS WhichPass, uint32_t WhichSwapchainImage);
    void AddPassCommandBuffer(uint32_t WhichBuffer, RENDER_PASS WhichPass);
    void EndRenderPass(uint32_t WhichBuffer, RENDER_PASS WhichPass);
    void UpdateGui();
    bool UpdateUniforms(uint32_t WhichBuffer);

private:

    // Render passes
    std::array< PassData, NUM_RENDER_PASSES> m_RenderPassData;

    // Per Frame Command lists
    std::array<CommandList, NUM_VULKAN_BUFFERS> m_CommandLists;

    // UBOs
    UniformT<SGSRFragUB> m_SGSRFragUniform;
    SGSRFragUB           m_SGSRFragUniformData;

    glm::vec2         m_scene_texture_original_size;
    float             m_sgsr_factor          = 1.0f;
    float             m_sgsr_scale_factor    = 1.0f;
    bool              m_sgsr_active          = false;
    bool              m_sgsr_edge_dir_active = false;
    Sampler<tGfxApi>  m_sgsr_sampler;

    // Drawables
    std::unique_ptr<Drawable> m_SGSRQuadDrawable;
    std::unique_ptr<Drawable> m_BlitQuadDrawable;

    // Shaders
    std::unique_ptr<ShaderManager> m_ShaderManager;

    // Materials
    std::unique_ptr<MaterialManager> m_MaterialManager;
};
