//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "imguiVulkan.hpp"
#include "vulkan/vulkan.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "system/os_common.h"
#include <cassert>

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    LOGE("VkResult %d\n", err);
    if (err < 0)
        assert(0);
}

//-----------------------------------------------------------------------------

GuiImguiVulkan::GuiImguiVulkan(Vulkan& vulkan, VkRenderPass renderPass)
    : GuiImguiPlatform()
    , m_Vulkan(vulkan)
    , m_RenderPass(renderPass)
{}

//-----------------------------------------------------------------------------

GuiImguiVulkan::~GuiImguiVulkan()
{
    vkDestroyDescriptorPool(m_Vulkan.m_VulkanDevice, m_DescriptorPool, nullptr);
    //We dont own the render pass// vkDestroyRenderPass(m_Vulkan.m_VulkanDevice, m_RenderPass, nullptr);

    ImGui_ImplVulkan_Shutdown();
}

//-----------------------------------------------------------------------------

bool GuiImguiVulkan::Initialize(uintptr_t windowHandle, uint32_t renderWidth, uint32_t renderHeight)
{
    // Call the platform (windows/android) specific implementation initialize...
    if (!GuiImguiPlatform::Initialize(windowHandle, m_Vulkan.m_SurfaceWidth, m_Vulkan.m_SurfaceHeight, renderWidth, renderHeight))
    {
        return false;
    }

    // Give Imgui its own descriptor pool
    {
        // Sizes from imgui example main.cpp
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        VkResult err = vkCreateDescriptorPool(m_Vulkan.m_VulkanDevice, &pool_info, nullptr, &m_DescriptorPool);
        check_vk_result(err);
        if (err != VK_SUCCESS)
        {
            return false;
        }
    }

    // Give imgui its own commandBuffers
    {
        m_CommandBuffer.reserve(m_Vulkan.m_SwapchainImageCount);
        for (uint32_t imageIdx = 0; imageIdx < m_Vulkan.m_SwapchainImageCount; ++imageIdx)
        {
            if (!m_CommandBuffer.emplace_back().Initialize(&m_Vulkan, "ImGui", VK_COMMAND_BUFFER_LEVEL_SECONDARY))
            {
                return false;
            }
        }
        if (!m_UploadCommandBuffer.Initialize(&m_Vulkan, "ImGui upload"))
        {
            return false;
        }
    }

    // Create the ImGui Vulkan implementation!
    {
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_Vulkan.GetVulkanInstance();
        init_info.PhysicalDevice = m_Vulkan.m_VulkanGpu;
        init_info.Device = m_Vulkan.m_VulkanDevice;
        init_info.QueueFamily = m_Vulkan.GetVulkanQueueIndx();
        init_info.Queue = m_Vulkan.m_VulkanQueue;
        init_info.PipelineCache = nullptr;// g_PipelineCache;
        init_info.DescriptorPool = m_DescriptorPool;
        init_info.Allocator = nullptr;// g_Allocator;
        init_info.MinImageCount = m_Vulkan.m_SwapchainImageCount;   //unused?
        init_info.ImageCount = m_Vulkan.m_SwapchainImageCount;
        init_info.CheckVkResultFn = check_vk_result;
        if (!ImGui_ImplVulkan_Init(&init_info, m_RenderPass))
        {
            vkDestroyDescriptorPool(m_Vulkan.m_VulkanDevice, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
            return false;
        }
    }

    // While we are here, use the new command buffer to upload the font.
    {
        if (!m_UploadCommandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        {
            return false;
        }

        if (!ImGui_ImplVulkan_CreateFontsTexture(m_UploadCommandBuffer.m_VkCommandBuffer))
        {
            return false;
        }
        if (!m_UploadCommandBuffer.End())
        {
            return false;
        }
        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &m_UploadCommandBuffer.m_VkCommandBuffer;
        VkResult err = vkQueueSubmit(m_Vulkan.m_VulkanQueue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(m_Vulkan.m_VulkanDevice);
        check_vk_result(err);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
        m_UploadCommandBuffer.Reset();
    }

    return true;
}

//-----------------------------------------------------------------------------

void GuiImguiVulkan::Update()
{
    ImGui_ImplVulkan_NewFrame();
    GuiImguiPlatform::Update();
}

//-----------------------------------------------------------------------------

VkCommandBuffer GuiImguiVulkan::Render(uint32_t frameIdx, VkFramebuffer frameBuffer)
{
    if (!m_CommandBuffer[frameIdx].Begin(frameBuffer, m_RenderPass, false, 0, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
    {
        return VK_NULL_HANDLE;
    }

    if (m_CommandBuffer[frameIdx].m_IsPrimary)
    {
        VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        info.renderPass = m_RenderPass;
        info.framebuffer = frameBuffer;
        info.renderArea.extent.width = m_Vulkan.m_SurfaceWidth;
        info.renderArea.extent.height = m_Vulkan.m_SurfaceHeight;
        info.clearValueCount = 0;
        info.pClearValues = nullptr;
        vkCmdBeginRenderPass(m_CommandBuffer[frameIdx].m_VkCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    Render(m_CommandBuffer[frameIdx].m_VkCommandBuffer);

    if (m_CommandBuffer[frameIdx].m_IsPrimary)
    {
        vkCmdEndRenderPass(m_CommandBuffer[frameIdx].m_VkCommandBuffer);
    }

    {
        VkResult err = vkEndCommandBuffer(m_CommandBuffer[frameIdx].m_VkCommandBuffer);
        check_vk_result(err);
    }
    return m_CommandBuffer[frameIdx].m_VkCommandBuffer;
}

void GuiImguiVulkan::Render(VkCommandBuffer cmdBuffer)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

