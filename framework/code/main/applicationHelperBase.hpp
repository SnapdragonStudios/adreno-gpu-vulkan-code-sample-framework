//============================================================================================================
//
//
//                  Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "vulkan/vulkan.hpp"
#include "frameworkApplicationBase.hpp"
#include "camera/camera.hpp"
#include "vulkan/renderPass.hpp"
#include "material/vulkan/materialManager.hpp"
#include "memory/vulkan/uniform.hpp"
#include "mesh/mesh.hpp"
#include "texture/vulkan/texture.hpp"
#include "vulkan/renderContext.hpp"
#include "vulkan/renderTarget.hpp"

class CameraControllerBase;
class CommandListBase;
class ComputableBase;
class SemaphoreWait;
class ShaderManagerBase;
class TextureManagerBase;
class TimerPoolBase;
class Traceable;
class VertexElementData;
class VertexFormat;
template<typename T_GFXAPI> class Buffer;
template<typename T_GFXAPI> class CommandList;
template<typename T_GFXAPI> class Computable;
template<typename T_GFXAPI> class Drawable;
template<typename T_GFXAPI> class DrawableLoader;
template<typename T_GFXAPI> class DrawIndirectBuffer;
template<typename T_GFXAPI> class GuiImguiGfx;
template<typename T_GFXAPI> struct ImageInfo;
template<typename T_GFXAPI> class Material;
template<typename T_GFXAPI> class MaterialManager;
template<typename T_GFXAPI> class MaterialPass;
template<typename T_GFXAPI> class Mesh;
template<typename T_GFXAPI> struct PerFrameBuffer;
template<typename T_GFXAPI> class RenderPass;
template<typename T_GFXAPI> class RenderTarget;
template<typename T_GFXAPI> class ShaderManager;
template<typename T_GFXAPI> class TextureManager;
template<typename T_GFXAPI> struct Uniform;
template<typename T_GFXAPI, typename T> struct UniformT;
template<typename T_GFXAPI, size_t T_NUM_BUFFERS> struct UniformArray;
template<typename T_GFXAPI, typename T, size_t T_NUM_BUFFERS> struct UniformArrayT;

class TouchStatus
{
public:
    TouchStatus() = default;
    ~TouchStatus() { };
    TouchStatus( const TouchStatus& ) = delete;
    TouchStatus& operator=( const TouchStatus& ) = delete;
    TouchStatus( TouchStatus&& ) = default;
    TouchStatus& operator=( TouchStatus&& ) = default;

    bool        m_isDown = false;
    uint8_t     m_clicks = 0;               // 1 if we just did a quick 'click', 2 if we did a quick click followed by a quick release
    float       m_xPos = 0.0f;
    float       m_yPos = 0.0f;
    float       m_xDownPos = 0.0f;
    float       m_yDownPos = 0.0f;
    uint32_t    m_lastDownChangeTimeMs = 0;// Timestamp (in ms) of last 'down' or 'up'
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
    using CommandList = CommandList<tGfxApi>;
    using CommandBufferT = CommandList;
    using CommandBuffer = CommandBufferT;
    using Computable = Computable<tGfxApi>;
    using Drawable = Drawable<tGfxApi>;
    using DrawableLoader = DrawableLoader<tGfxApi>;
    using DrawIndirectBuffer = DrawIndirectBuffer<tGfxApi>;
    using ImageInfo = ImageInfo<tGfxApi>;
    using IndexBufferObject = IndexBuffer<tGfxApi>;
    using Material = Material<tGfxApi>;
    using MaterialPass = MaterialPass<tGfxApi>;
    using MaterialManager = MaterialManager<tGfxApi>;
    using Mesh = Mesh<tGfxApi>;
    using PerFrameTexInfo = MaterialManager::tPerFrameTexInfo;
    using PerFrameBuffer = PerFrameBuffer<tGfxApi>;
    using RenderContext = RenderContext<Vulkan>;
    using RenderPass = RenderPass<tGfxApi>;
    using RenderTarget = RenderTarget<tGfxApi>;
    using ShaderManager = ShaderManager<tGfxApi>;
    using Uniform = Uniform<tGfxApi>;
    using VertexBufferObject = VertexBuffer<tGfxApi>;
    using BufferVulkan = Buffer<tGfxApi>;
    using GuiImguiGfx = GuiImguiGfx<tGfxApi>;

    template<size_t T_NUM_BUFFERS> using UniformArray = UniformArray<tGfxApi, T_NUM_BUFFERS>;
    template<typename T> using UniformT = UniformT<tGfxApi, T>;
    template<typename T, size_t T_NUM_BUFFERS> using UniformArrayT = UniformArrayT<tGfxApi, T, T_NUM_BUFFERS>;

    /// @brief Setp the scene camera and camera controller objects.
    /// @returns true on success
    virtual bool InitCamera();

    bool    Initialize(uintptr_t hWnd, uintptr_t hInstance) override;           ///< Override FrameworkApplicationBase::Initialize
    bool    ReInitialize(uintptr_t hWnd, uintptr_t hInstance) override;         ///< Override FrameworkApplicationBase::ReInitialize
    void    Destroy() override;                                                 ///< Override FrameworkApplicationBase::Destroy

    bool    SetWindowSize(uint32_t width, uint32_t height) override;            ///< Override FrameworkApplicationBase::SetWindowSize.  Passes new window size in to camera.
    
    /// May be called during initialize (from Vulkan.cpp, during inital Vulkan setup).
    /// @return index of the SurfaceFormat you want to use, or -1 for default.
    virtual int PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat>);

    /// May be called during initialize (from Vulkan.cpp, during inital Vulkan setup).
    /// @param configuration (if untouched, Vulkan will be setup using defaults).
    virtual void PreInitializeSetVulkanConfiguration( tGfxApi::AppConfiguration&);

    void    KeyDownEvent(uint32_t key) override;                                ///< Override FrameworkApplicationBase::KeyDownEvent
    void    KeyRepeatEvent(uint32_t key) override;                              ///< Override FrameworkApplicationBase::KeyRepeatEvent
    void    KeyUpEvent(uint32_t key) override;                                  ///< Override FrameworkApplicationBase::KeyUpEvent
    void    TouchDownEvent(int iPointerID, float xPos, float yPos) override;    ///< Override FrameworkApplicationBase::TouchDownEvent
    void    TouchMoveEvent(int iPointerID, float xPos, float yPos) override;    ///< Override FrameworkApplicationBase::TouchMoveEvent
    void    TouchUpEvent(int iPointerID, float xPos, float yPos) override;      ///< Override FrameworkApplicationBase::TouchUpEvent
    virtual void TouchDoubleClickEvent(int iPointerID);                         ///< Generated by ApplicationHelperBase (in ApplicationHelperBase::TouchUpEvent)

    /// @brief Add the commands to draw this drawable to the given commandbuffers.
    /// @param drawable the Drawable object we want to add commands for (may contain multiple DrawablePass)
    /// @param cmdBuffers pointer to array, assumed to be sized [numVulkanBuffers][numRenderPasses]
    /// @param numRenderPasses number of render passes in the array (the DrawablePass has the pass index that is written to) 
    /// @param numFrameBuffers number of buffers in the array (number of frames to build the command buffers for)
    void    AddDrawableToCmdBuffers(const Drawable& drawable, CommandList* cmdBuffers, uint32_t numRenderPasses, uint32_t numFrameBuffers, uint32_t startDescriptorSetIdx = 0) const;
    void    AddDrawableToCmdBuffer( const Drawable& drawable, CommandList& cmdBuffer, uint32_t startDescriptorSetIdx = 0 ) const
    {
        AddDrawableToCmdBuffers( drawable, &cmdBuffer, 1, 1, startDescriptorSetIdx );
    }

    /// @brief Add the commands to draw this drawable to the given commandbuffers.
    /// @param drawable the Drawable object we want to add commands for (may contain multiple DrawablePass)
    /// @param cmdBuffers pointer to array, assumed to be sized [numVulkanBuffers][numRenderPasses]
    /// @param numRenderPasses number of render passes in the array (the DrawablePass has the pass index that is written to) 
    /// @param whichVulkanBuffer which vulkan buffer to add to (not all vulkan buffers)
    /// @param dummy to distinguish other version of AddDrawableToCmdBuffers(..., uint32_t startDescriptorSetIdx = 0)
    void    AddDrawableToCmdBuffers(const Drawable& drawable, CommandList* cmdBuffers, uint32_t numRenderPasses, uint32_t whichVulkanBuffer, uint32_t startDescriptorSetIdx, float dummy) const;

    /// @brief Add the commands to dispatch this computable to a command buffer.
    /// Potentially adds multiple vkCmdDispatch (will make one per 'pass' in the Computable) and inserts appropiate memory barriers between passes.
    /// @param computable
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    /// @param startDescriptorSetIdx index of the first descriptor set to add
    void    AddComputableToCmdBuffer(const Computable& computable, CommandList* cmdBuffers, uint32_t numRenderPasses, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool = nullptr ) const;
    void    AddComputableToCmdBuffer(const Computable& computable, CommandList& cmdBuffer, TimerPoolBase* timerPool = nullptr) const
    {
        AddComputableToCmdBuffer( computable, &cmdBuffer, 1, 0, timerPool );
    }
    void    AddComputableToCmdBuffer(const ComputableBase& computable, CommandList* cmdBuffers, uint32_t numRenderPasses, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool = nullptr ) const;
    void    AddComputableToCmdBuffer(const ComputableBase& computable, CommandList& cmdBuffer, TimerPoolBase* timerPool = nullptr ) const
    {
        AddComputableToCmdBuffer( computable, &cmdBuffer, 1, 0, timerPool );
    }

    /// @brief Add command(s) for memory barriers on any buffers or images written in the Computable (that are not already barriered between passes inside the computable)
    /// @param computable 
    /// @param cmdBuffers pointer to array of commandbuffers we want to add the barrier to, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    void AddComputableOutputBarrierToCmdBuffer(const Computable& computable, CommandList* cmdBuffers, uint32_t numRenderPasses) const;
    void AddComputableOutputBarrierToCmdBuffer(const Computable& computable, CommandList& cmdBuffer) const
    {
        AddComputableOutputBarrierToCmdBuffer(computable, &cmdBuffer, 1);
    }
    void AddComputableOutputBarrierToCmdBuffer(const ComputableBase& computable, CommandList* cmdBuffers, uint32_t numRenderPasses) const;
    void AddComputableOutputBarrierToCmdBuffer(const ComputableBase& computable, CommandList& cmdBuffer) const
    {
        AddComputableOutputBarrierToCmdBuffer(computable, &cmdBuffer, 1);
    }

    /// @brief Add the commands to dispatch this tracable to a command buffer.
    /// Potentially adds multiple vkCmdTraceRaysKHR (will make one per 'pass' in the Tracable) and inserts appropiate memory barriers between passes.
    /// @param computable
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    /// @param startDescriptorSetIdx index of the first descriptor set to add (use when Computable 
    void    AddTraceableToCmdBuffer(const Traceable& traceable, CommandList* cmdBuffers, uint32_t numRenderPasses, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool = nullptr ) const;
    void    AddTraceableToCmdBuffer(const Traceable& traceable, CommandList& cmdBuffer, TimerPoolBase* timerPool = nullptr ) const
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
    Texture<tGfxApi> LoadKTXTexture(tGfxApi*, const char* filename, SamplerAddressMode = SamplerAddressMode::ClampEdge);
    Texture<tGfxApi> LoadKTXTexture(tGfxApi*, const char* filename, SamplerBase&);

    /// Mesh loader helper to load the first shape in a .gltf file (no materials).
    /// Returned MeshObject does not have an index buffer (3 verts per triangle) and data is in the MeshHelper::vertex_layout format.
    /// @returns true on success
    bool    LoadGLTF(const std::string& filename, uint32_t binding, Mesh* meshObject);

    /// @brief Helper to create a fullscreen (quad) drawable
    std::unique_ptr<Drawable> InitFullscreenDrawable( const char* pShaderName, const std::map<const std::string, const TextureBase*>& ColorAttachmentsLookup, const std::map<const std::string, const ImageInfo>& ImageAttachmentsLookup, const std::map<const std::string, const PerFrameBuffer>& UniformsLookup, const std::map<const std::string, VertexElementData> specializationConstants, const RenderPass& renderPass );

    /// @brief Get pointer to the framework Vulkan class
    Vulkan* GetVulkan() const { return static_cast<tGfxApi*>(m_gfxBase.get()); }
    /// @brief Get pointer to the framework graphicsApi Vulkan class (will get the Dx12 class when called from the Dx12 ApplicationHelperBase).  Can use FrameworkApplicationBase::GetGraphicsApiBase if you want the base class only!
    Vulkan* GetGfxApi() const { return static_cast<tGfxApi*>(m_gfxBase.get()); }
    /// @brief Get pointer to the framework gui (Vulkan) class
    auto* GetGui() const { return (GuiImguiGfx*)(m_Gui.get()); }

protected:
    // Scene Camera
    Camera                                  m_Camera;

    // Camera Controller
    std::unique_ptr<CameraControllerBase>   m_CameraController;

    std::vector<TouchStatus>                m_TouchStates;

    // Shaders
    std::unique_ptr<ShaderManager>         m_ShaderManager;

    // Materials
    std::unique_ptr<MaterialManager>       m_MaterialManager;

    // Output backbuffer (framebuffer) helper
    std::array<RenderTarget, NUM_VULKAN_BUFFERS>  m_BackbufferRenderTarget;
    std::array<RenderContext, NUM_VULKAN_BUFFERS> m_BackbufferRenderContext;

    // Texture manager
    std::unique_ptr<TextureManagerBase>         m_TextureManager;

    // Default samplers
    Sampler<tGfxApi>                       m_SamplerRepeat;
    Sampler<tGfxApi>                       m_SamplerEdgeClamp;
    Sampler<tGfxApi>                       m_SamplerMirroredRepeat;
};

//
// Additional helpers for app_config.txt parsing
//

template <>
inline void ReadFromText<VkSampleCountFlagBits>( VkSampleCountFlagBits* val, const char* const text )
{
    ReadFromText<uint32_t>((uint32_t*)(val), text);
}

template <>
inline void ReadFromText<Msaa>( Msaa* val, const char* const text )
{
    ReadFromText<uint32_t>((uint32_t*)(val), text);
}

