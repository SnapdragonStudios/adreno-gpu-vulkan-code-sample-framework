//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "vulkan/vulkan.hpp"
#include "frameworkApplicationBase.hpp"
#include "texture/vulkan/texture.hpp"
#include "vulkan/renderTarget.hpp"
#include "camera/camera.hpp"
#include "mesh/mesh.hpp"

class CameraControllerBase;
class Computable;
class Drawable;
class Traceable;
class ShaderManager;
class SemaphoreWait;
class TextureManager;
class TimerPoolBase;
class VertexFormat;
template<typename T_GFXAPI> class CommandListT;
template<typename T_GFXAPI> class GuiImguiGfx;
template<typename T_GFXAPI> class MaterialManagerT;
template<typename T_GFXAPI> class Mesh;
template<typename T_GFXAPI> class TextureManagerT;
template<typename T_GFXAPI> struct Uniform;
template<typename T_GFXAPI, typename T> struct UniformT;
template<typename T_GFXAPI, size_t T_NUM_BUFFERS> struct UniformArray;
template<typename T_GFXAPI, typename T, size_t T_NUM_BUFFERS> struct UniformArrayT;

class TouchStatus
{
public:
    TouchStatus() {m_isDown = false; m_xPos = 0.0f; m_yPos = 0.0f; m_xDownPos = 0.0f; m_yDownPos = 0.0f; };
    ~TouchStatus() { };

    bool    m_isDown;
    float   m_xPos;
    float   m_yPos;
    float   m_xDownPos;
    float   m_yDownPos;
};

/// Helper class that applications can be derived from.
/// Provides Camera and Drawable functionality to reduce code duplication of more 'boilderplate' application features.
class ApplicationHelperBase : public FrameworkApplicationBase
{
protected:
    ApplicationHelperBase() noexcept;
    ~ApplicationHelperBase() override;

    // This class is for Vulkan applications
    using tGfxApi = Vulkan;
    using MeshObject = Mesh<tGfxApi>;
    using Uniform = Uniform<tGfxApi>;
    template<size_t T_NUM_BUFFERS> using UniformArray = UniformArray<tGfxApi, T_NUM_BUFFERS>;
    template<typename T> using UniformT = UniformT<tGfxApi, T>;
    template<typename T, size_t T_NUM_BUFFERS> using UniformArrayT = UniformArrayT<tGfxApi, T, T_NUM_BUFFERS>;

    /// @brief Setp the scene camera and camera controller objects.
    /// @returns true on success
    virtual bool InitCamera();

    bool    Initialize(uintptr_t hWnd, uintptr_t hInstance) override;           ///< Override FrameworkApplicationBase::Initialize
    void    Destroy() override;                                                 ///< Override FrameworkApplicationBase::Destroy

    bool    SetWindowSize(uint32_t width, uint32_t height) override;            ///< Override FrameworkApplicationBase::SetWindowSize.  Passes new window size in to camera.
    
    /// May be called during initialize (from Vulkan.cpp, during inital Vulkan setup).
    /// @return index of the SurfaceFormat you want to use, or -1 for default.
    virtual int PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat>);

    /// May be called during initialize (from Vulkan.cpp, during inital Vulkan setup).
    /// @param configuration (if untouched, Vulkan will be setup using defaults).
    virtual void PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration&);

    void    KeyDownEvent(uint32_t key) override;                                ///< Override FrameworkApplicationBase::KeyDownEvent
    void    KeyRepeatEvent(uint32_t key) override;                              ///< Override FrameworkApplicationBase::KeyRepeatEvent
    void    KeyUpEvent(uint32_t key) override;                                  ///< Override FrameworkApplicationBase::KeyUpEvent
    void    TouchDownEvent(int iPointerID, float xPos, float yPos) override;    ///< Override FrameworkApplicationBase::TouchDownEvent
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos) override;    ///< Override FrameworkApplicationBase::TouchMoveEvent
    void    TouchUpEvent(int iPointerID, float xPos, float yPos) override;      ///< Override FrameworkApplicationBase::TouchUpEvent

    /// @brief Add the commands to draw this drawable to the given commandbuffers.
    /// @param drawable the Drawable object we want to add commands for (may contain multiple DrawablePass)
    /// @param cmdBuffers pointer to array, assumed to be sized [numVulkanBuffers][numRenderPasses]
    /// @param numRenderPasses number of render passes in the array (the DrawablePass has the pass index that is written to) 
    /// @param numVulkanBuffers number of buffers in the array (number of frames to build the command buffers for)
    void    AddDrawableToCmdBuffers(const Drawable& drawable, CommandListT<tGfxApi>* cmdBuffers, uint32_t numRenderPasses, uint32_t numVulkanBuffers, uint32_t startDescriptorSetIdx = 0) const;
    /// @brief Add the commands to dispatch this computable to a command buffer.
    /// Potentially adds multiple vkCmdDispatch (will make one per 'pass' in the Computable) and inserts appropiate memory barriers between passes.
    /// @param computable
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    /// @param startDescriptorSetIdx index of the first descriptor set to add
    void    AddComputableToCmdBuffer(const Computable& computable, CommandListT<tGfxApi>* cmdBuffers, uint32_t numRenderPasses, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool = nullptr ) const;
    void    AddComputableToCmdBuffer( const Computable& computable, CommandListT<tGfxApi>& cmdBuffer, TimerPoolBase* timerPool = nullptr ) const
    {
        AddComputableToCmdBuffer( computable, &cmdBuffer, 1, 0, timerPool );
    }

    /// @brief Add command(s) for memory barriers on any buffers or images written in the Computable (that are not already barriered between passes inside the computable)
    /// @param computable 
    /// @param cmdBuffers pointer to array of commandbuffers we want to add the barrier to, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    void AddComputableOutputBarrierToCmdBuffer(const Computable& computable, CommandListT<tGfxApi>* cmdBuffers, uint32_t numRenderPasses) const;
    void AddComputableOutputBarrierToCmdBuffer(const Computable& computable, CommandListT<tGfxApi>& cmdBuffer) const
    {
        AddComputableOutputBarrierToCmdBuffer(computable, &cmdBuffer, 1);
    }

    /// @brief Add the commands to dispatch this tracable to a command buffer.
    /// Potentially adds multiple vkCmdTraceRaysKHR (will make one per 'pass' in the Tracable) and inserts appropiate memory barriers between passes.
    /// @param computable
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    /// @param startDescriptorSetIdx index of the first descriptor set to add (use when Computable 
    void    AddTraceableToCmdBuffer(const Traceable& traceable, CommandListT<tGfxApi>* cmdBuffers, uint32_t numRenderPasses, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool = nullptr ) const;
    void    AddTraceableToCmdBuffer(const Traceable& traceable, CommandListT<tGfxApi>& cmdBuffer, TimerPoolBase* timerPool = nullptr ) const
    {
        AddTraceableToCmdBuffer(traceable, &cmdBuffer, 1, 0, timerPool);
    }

    /// @brief Present the given queue to the swapchain
    /// Helper calls down into Vulkan::PresentQueue but also handles screen 'dump' (screenshot) if required/requested
    /// @returns false on present error
    bool    PresentQueue( const std::span<const VkSemaphore> WaitSemaphore, uint32_t SwapchainPresentIndx );
    /// Run the PresentQueue (simplified helper)
    bool    PresentQueue( VkSemaphore WaitSemaphore, uint32_t SwapchainPresentIndx )
    {
        auto WaitSemaphores = (WaitSemaphore != VK_NULL_HANDLE) ? std::span<VkSemaphore>( &WaitSemaphore, 1 ) : std::span<VkSemaphore>();
        return PresentQueue( WaitSemaphores, SwapchainPresentIndx );
    }
    /// Helper using the sempaphore wrappers
    bool    PresentQueue(const std::span<const SemaphoreWait> WaitSemaphores, uint32_t SwapchainPresentIndx);
    bool    PresentQueue(const SemaphoreWait& WaitSemaphore, uint32_t SwapchainPresentIndx);

    /// Texture loader helper to replace the (old) texture loading code
    TextureT<tGfxApi> LoadKTXTexture(tGfxApi*, AssetManager&, const char* filename, SamplerAddressMode = SamplerAddressMode::ClampEdge);
    TextureT<tGfxApi> LoadKTXTexture(tGfxApi*, AssetManager&, const char* filename, Sampler&);

    /// Mesh loader helper to load the first shape in a .gltf file (no materials).
    /// Returned MeshObject does not have an index buffer (3 verts per triangle) and data is in the MeshHelper::vertex_layout format.
    /// @returns true on success
    bool    LoadGLTF(const std::string& filename, uint32_t binding, Mesh<Vulkan>* meshObject);

    /// @brief Get pointer to the framework Vulkan class
    Vulkan* GetVulkan() const { return static_cast<tGfxApi*>(m_gfxBase.get()); }
    /// @brief Get pointer to the framework gui (Vulkan) class
    GuiImguiGfx<tGfxApi>* GetGui() const { return (GuiImguiGfx<tGfxApi>*)(m_Gui.get()); }

protected:
    // Scene Camera
    Camera                                  m_Camera;

    // Camera Controller
    std::unique_ptr<CameraControllerBase>   m_CameraController;

    std::vector<TouchStatus>                m_TouchStates;

    // Shaders
    std::unique_ptr<ShaderManager>          m_ShaderManager;

    // Materials
    std::unique_ptr<MaterialManagerT<tGfxApi>> m_MaterialManager;

    // Output backbuffer (framebuffer) helper
    CRenderTargetArray<NUM_VULKAN_BUFFERS>  m_BackbufferRenderTarget;

    // Texture manager
    std::unique_ptr<TextureManager>         m_TextureManager;

    // Default samplers
    SamplerT<tGfxApi>                       m_SamplerRepeat;
    SamplerT<tGfxApi>                       m_SamplerEdgeClamp;
    SamplerT<tGfxApi>                       m_SamplerMirroredRepeat;
};
