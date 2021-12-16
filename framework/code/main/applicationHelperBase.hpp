// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "frameworkApplicationBase.hpp"
#include "camera/camera.hpp"

class CameraController;
class CameraControllerTouch;
class Computable;
class Drawable;
class Wrap_VkCommandBuffer;


/// Helper class that applications can be derived from.
/// Provides Camera and Drawable functionality to reduce code duplication of more 'boilderplate' applicaiton features.
class ApplicationHelperBase : public FrameworkApplicationBase
{
protected:
    ApplicationHelperBase() : FrameworkApplicationBase() {}

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
    void    AddDrawableToCmdBuffers(const Drawable& drawable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses, uint32_t numVulkanBuffers);
    /// @brief Add the commands to dispatch this computable to a command buffer.
    /// Potentially adds multiple vkCmdDispatch (will make one per 'pass' in the Computable) and inserts appropiate memory barriers between passes.
    /// @param computable
    /// @param cmdBuffers pointer to array of commandbuffers we want to fill, assumed to be sized [numRenderPasses]
    /// @param numRenderPasses number of cmdBuffers to fill
    void    AddComputableToCmdBuffer(const Computable& computable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses);
    void    AddComputableToCmdBuffer( const Computable& computable, Wrap_VkCommandBuffer& cmdBuffer )
    {
        AddComputableToCmdBuffer( computable, &cmdBuffer, 1 );
    }

protected:
    // Scene Camera
    Camera                                  m_Camera;

    // Camera Controller
#if defined(OS_ANDROID) ///TODO: make this an option
    std::unique_ptr<CameraControllerTouch>  m_CameraController;
#else
    std::unique_ptr<CameraController>       m_CameraController;
#endif
};


