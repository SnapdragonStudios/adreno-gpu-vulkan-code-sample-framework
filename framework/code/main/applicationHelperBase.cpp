// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "applicationHelperBase.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "material/computable.hpp"
#include "material/drawable.hpp"

//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddDrawableToCmdBuffers(const Drawable& drawable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses, uint32_t numVulkanBuffers)
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
            drawable.DrawPass(cmdBuffer, drawablePass, bufferIdx);

            ++buffer.m_NumDrawCalls;
            buffer.m_NumTriangles += drawablePass.mNumVertices / 3;
        }
    }
}


//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddComputableToCmdBuffer(const Computable& computable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numBuffers)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    LOGI("AddComputableToCmdBuffer() Entered...");

    assert(cmdBuffers != nullptr);

    for(uint32_t whichBuffer = 0; whichBuffer < numBuffers; ++whichBuffer)
    {
        uint32_t passIdx = 0;
        for (const auto& computablePass : computable.GetPasses())
        {
            // Add image barriers (if needed)
            const auto& imageMemoryBarriers = computablePass.GetVkImageMemoryBarriers();
            if (imageMemoryBarriers.size() > 0)
            {
                // Barrier on image memory, with correct layouts set.
                vkCmdPipelineBarrier(cmdBuffers->m_VkCommandBuffer,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // srcMask,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // dstMask,
                                      0,
                                      0, nullptr,
                                      0, nullptr,
                                      (uint32_t)imageMemoryBarriers.size(),
                                      imageMemoryBarriers.data());
            }

            // Bind the pipeline for this material
            vkCmdBindPipeline(cmdBuffers->m_VkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computablePass.mPipeline);

            // Bind everything the shader needs
            const auto& descriptorSets = computablePass.GetVkDescriptorSets();
            VkDescriptorSet descriptorSet = descriptorSets.size() >= 1 ? descriptorSets[whichBuffer] : descriptorSets[0];
            vkCmdBindDescriptorSets(cmdBuffers->m_VkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computablePass.mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            // Dispatch the compute task
            vkCmdDispatch(cmdBuffers->m_VkCommandBuffer, computablePass.GetDispatchGroupCount()[0], computablePass.GetDispatchGroupCount()[1], computablePass.GetDispatchGroupCount()[2]);

            ++passIdx;
        }
        ++cmdBuffers;
    }
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
    if (m_CameraController)
        m_CameraController->TouchDownEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchMoveEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_CameraController->TouchMoveEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchUpEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_CameraController->TouchUpEvent(iPointerID, xPos, yPos);
}
