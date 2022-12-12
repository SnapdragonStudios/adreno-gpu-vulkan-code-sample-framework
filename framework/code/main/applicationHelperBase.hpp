//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "frameworkApplicationBase.hpp"
#include "vulkan/renderTarget.hpp"
#include "camera/camera.hpp"

class CameraControllerBase;
class Computable;
class Drawable;
class Wrap_VkCommandBuffer;

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
    ApplicationHelperBase() : FrameworkApplicationBase() {}
    ~ApplicationHelperBase() override;

    /// @brief Setp the scene camera and camera controller objects.
    /// @returns true on success
    virtual bool InitCamera();

    bool    Initialize(uintptr_t windowHandle) override;                        ///< Override FrameworkApplicationBase::Initialize
    void    Destroy() override;                                                 ///< Override FrameworkApplicationBase::Destroy

    bool    SetWindowSize(uint32_t width, uint32_t height) override;            ///< Override FrameworkApplicationBase::SetWindowSize.  Passes new window size in to camera.
    
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
    void    AddDrawableToCmdBuffers(const Drawable& drawable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses, uint32_t numVulkanBuffers, uint32_t startDescriptorSetIdx = 0) const;
    /// @brief Add the commands to dispatch this computable to a command buffer.
    /// Potentially adds multiple vkCmdDispatch (will make one per 'pass' in the Computable) and inserts appropiate memory barriers between passes.
    /// @param computable
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    /// @param startDescriptorSetIdx index of the first descriptor set to add (use when Computable 
    void    AddComputableToCmdBuffer(const Computable& computable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses, uint32_t startDescriptorSetIdx) const;
    void    AddComputableToCmdBuffer( const Computable& computable, Wrap_VkCommandBuffer& cmdBuffer ) const
    {
        AddComputableToCmdBuffer( computable, &cmdBuffer, 1, 0 );
    }

    /// @brief Present the given queue to the swapchain
    /// Helper calls down into Vulkan::PresentQueue but also handles screen 'dump' (screenshot) if required/requested
    /// @returns false on present error
    bool    PresentQueue( const tcb::span<const VkSemaphore> WaitSemaphore, uint32_t SwapchainPresentIndx );
    /// Run the PresentQueue (simplified helper)
    bool    PresentQueue( VkSemaphore WaitSemaphore, uint32_t SwapchainPresentIndx )
    {
        auto WaitSemaphores = (WaitSemaphore != VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &WaitSemaphore, 1 ) : tcb::span<VkSemaphore>();
        return PresentQueue( WaitSemaphores, SwapchainPresentIndx );
    }

protected:
    // Scene Camera
    Camera                                  m_Camera;

    // Camera Controller
    std::unique_ptr<CameraControllerBase>   m_CameraController;

    std::vector<TouchStatus>                m_TouchStates;

    // Output backbuffer (framebuffer) helper
    CRenderTargetArray<NUM_VULKAN_BUFFERS>  m_BackbufferRenderTarget;


};


