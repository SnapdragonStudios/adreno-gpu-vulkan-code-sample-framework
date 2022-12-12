//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "applicationHelperBase.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "material/computable.hpp"
#include "material/drawable.hpp"
#include "vulkan/renderTarget.hpp"

extern "C" {
VAR(float, gCameraRotateSpeed, 0.25f, kVariableNonpersistent);
VAR(float, gCameraMoveSpeed, 4.0f, kVariableNonpersistent);
}; //extern "C"

//-----------------------------------------------------------------------------
ApplicationHelperBase::~ApplicationHelperBase()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::InitCamera()
//-----------------------------------------------------------------------------
{
    // Camera
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));

    // Camera Controller

#if defined(OS_ANDROID) ///TODO: make this an option
    typedef CameraControllerTouch           tCameraController;
#else
    typedef CameraController                tCameraController;
#endif

    auto cameraController = std::make_unique<tCameraController>();
    if (!cameraController->Initialize(m_WindowWidth, m_WindowHeight))
    {
        return false;
    }
    cameraController->SetRotateSpeed(gCameraRotateSpeed);
    cameraController->SetMoveSpeed(gCameraMoveSpeed);
    m_CameraController = std::move(cameraController);
    return true;
}

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::Initialize(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    if (!FrameworkApplicationBase::Initialize(windowHandle))
        return false;

    CRenderTargetArray<NUM_VULKAN_BUFFERS> backbuffer;
    if (!m_BackbufferRenderTarget.InitializeFromSwapchain(m_vulkan.get()))
        return false;

    return true;
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::Destroy()
//-----------------------------------------------------------------------------
{
    //m_BackbufferRenderTarget.HardReset();   // DO NOT destroy as this instance does not own its framebuffer (points to the vulkan backbuffers)
    FrameworkApplicationBase::Destroy();
}


//-----------------------------------------------------------------------------
bool ApplicationHelperBase::SetWindowSize(uint32_t width, uint32_t height)
//-----------------------------------------------------------------------------
{
    if (!FrameworkApplicationBase::SetWindowSize(width, height))
        return false;

    m_Camera.SetAspect(float(width) / float(height));
    if (m_CameraController)
    {
        m_CameraController->SetSize(width, height);
    }
    return true;
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddDrawableToCmdBuffers(const Drawable& drawable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses, uint32_t numVulkanBuffers, uint32_t startDescriptorSetIdx) const
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    const auto& drawablePasses = drawable.GetDrawablePasses();
    for (const auto& drawablePass : drawablePasses)
    {
        const auto passIdx = drawablePass.mPassIdx;
        assert(passIdx < numRenderPasses);

        for (uint32_t bufferIdx = 0; bufferIdx < numVulkanBuffers; ++bufferIdx)
        {
            Wrap_VkCommandBuffer& buffer = cmdBuffers[bufferIdx * numRenderPasses + passIdx];
            VkCommandBuffer cmdBuffer = buffer.m_VkCommandBuffer;
            assert(cmdBuffer != VK_NULL_HANDLE);

            // Add commands to bind the pipeline, buffers etc and issue the draw.
            drawable.DrawPass(cmdBuffer, drawablePass, drawablePass.mDescriptorSet.empty() ? 0 : (startDescriptorSetIdx + bufferIdx) % drawablePass.mDescriptorSet.size() );

            ++buffer.m_NumDrawCalls;
            buffer.m_NumTriangles += drawablePass.mNumVertices / 3;
        }
    }
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddComputableToCmdBuffer(const Computable& computable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numCmdBuffers, uint32_t startDescriptorSetIdx) const
//-----------------------------------------------------------------------------
{
    // LOGI("AddComputableToCmdBuffer() Entered...");

    assert(cmdBuffers != nullptr);

    for(uint32_t whichBuffer = 0; whichBuffer < numCmdBuffers; ++whichBuffer)
    {
        for (const auto& computablePass : computable.GetPasses())
        {
            computable.DispatchPass(cmdBuffers->m_VkCommandBuffer, computablePass, (whichBuffer + startDescriptorSetIdx) % (uint32_t)computablePass.GetVkDescriptorSets().size());
        }
        ++cmdBuffers;
    }
}

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::PresentQueue( const tcb::span<const VkSemaphore> WaitSemaphores, uint32_t SwapchainPresentIndx )
//-----------------------------------------------------------------------------
{
    if (!m_vulkan->PresentQueue( WaitSemaphores, SwapchainPresentIndx ))
        return false;

#if OS_WINDOWS
    {
        static int gHLMFrameNumber = -1;

        gHLMFrameNumber++;
        if (gHLMFrameNumber >= gHLMDumpFrame && gHLMFrameNumber < gHLMDumpFrame + gHLMDumpFrameCount && gHLMDumpFile!=nullptr && *gHLMDumpFile!='\0')
        {
            // TODO: Add code from DumpSwapchainImage, which was temporarily removed for modifications
            throw;
        }
    }
#endif // OS_WINDOWS

    return true;
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::KeyDownEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_CameraController->KeyDownEvent(key);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::KeyRepeatEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::KeyUpEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_CameraController->KeyUpEvent(key);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchDownEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    // Make sure we are big enough for this ID
    while (m_TouchStates.size() < iPointerID + 1)
    {
        TouchStatus NewEntry;
        m_TouchStates.push_back(NewEntry);
    }

    m_TouchStates[iPointerID].m_isDown = true;
    m_TouchStates[iPointerID].m_xPos = xPos;
    m_TouchStates[iPointerID].m_yPos = yPos;
    m_TouchStates[iPointerID].m_xDownPos = xPos;
    m_TouchStates[iPointerID].m_yDownPos = yPos;

    if (m_CameraController)
        m_CameraController->TouchDownEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchMoveEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    // Make sure we are big enough for this ID
    while (m_TouchStates.size() < iPointerID + 1)
    {
        TouchStatus NewEntry;
        m_TouchStates.push_back(NewEntry);
    }

    m_TouchStates[iPointerID].m_isDown = true;
    m_TouchStates[iPointerID].m_xPos = xPos;
    m_TouchStates[iPointerID].m_yPos = yPos;

    if (m_CameraController)
        m_CameraController->TouchMoveEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchUpEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    // Make sure we are big enough for this ID
    while (m_TouchStates.size() < iPointerID + 1)
    {
        TouchStatus NewEntry;
        m_TouchStates.push_back(NewEntry);
    }

    m_TouchStates[iPointerID].m_isDown = false;
    m_TouchStates[iPointerID].m_xPos = xPos;
    m_TouchStates[iPointerID].m_yPos = yPos;

    if (m_CameraController)
        m_CameraController->TouchUpEvent(iPointerID, xPos, yPos);
}
