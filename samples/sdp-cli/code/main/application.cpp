//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "application.hpp"
#include "helpers/console_helper.hpp"
#include "helpers/console_common.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "gui/imguiVulkan.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/meshLoader.hpp"
#include "material/drawable.hpp"
#include "material/materialManager.hpp"
#include "material/shader.hpp"
#include "material/shaderDescription.hpp"
#include "material/shaderManager.hpp"
#include "system/math_common.hpp"
#include "texture/textureManager.hpp"
#include "imgui.h"
#include "implot/implot.h"
#include <span>
#include <bitset>
#include <filesystem>

// Modules
#include "modules/sdp_cli/sdp_cli.hpp"

///
/// @brief Implementation of the Application entrypoint (called by the framework)
/// @return Pointer to Application (derived from @FrameworkApplicationBase).
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
bool Application::Initialize( uintptr_t windowHandle, uintptr_t hInstance )
//-----------------------------------------------------------------------------
{
    if( !ApplicationHelperBase::Initialize( windowHandle, hInstance ) )
    {
        return false;
    }

    auto* const pVulkan = GetVulkan();

    InitGui(windowHandle);

    InitCommandBuffers();

    // InitFramebuffersRenderPassesAndDrawables();

    InitCamera();

    UpdateDeviceList();

    if (m_connected_devices.size() >= 1)
    {
        m_selected_device = m_connected_devices[0];
        InitializeModulesWithSelectedDevice();
    }

    return true;
}

void Application::InitializeModulesWithSelectedDevice()
{
    m_modules.clear();

    // Register each module that will be used:
    m_modules.push_back(std::make_unique<SDP>(m_selected_device));

    // Initialize registered modules
    for (int i = m_modules.size()-1; i>=0; i--)
    {
        if(!m_modules[i]->Initialize())
        {
            m_modules.erase(m_modules.begin() + i);
        }
    }
}

void Application::UpdateDeviceList()
{
    m_connected_devices = GetConnectedDeviceList();
}

/*
* # Main Window Requirements:
* - If multiple devices, selector for which device to use (if changed, restart whole app)
* - Root mode or disable other parts (thermal)
* - Restart and shutdown options
* - Deploy files (thermal, SDP CLI and others)
* - Switch to other tabs
* - Tabs should have different colors if active (if capture is active, if whatever they have active has overhead, etc)
* 
* # Main Window:
* - Device info (Device name, Chipset, GPU, driver version, compiler version, Android version, DVT, is rooted)
* - CPU usage, frequency and thermal
* - Device overall thermal
* - Stalls PIE (SDP CLI)
* - Texture misses (SDP CLI)
* - FPS
* 
* # Long Usage Capture (accumulated)
* - CPU usage, frequency and thermal
* - Device overall thermal
* - GPU Frequency
* - GPU Thermal
* - FPS
* - 
*/

//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    char szName[256];
    for( uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++ )
    {
        sprintf( szName, "CommandBuffer (%d of %d)", WhichBuffer + 1, NUM_VULKAN_BUFFERS );
        if( !m_CommandBuffer[WhichBuffer].Initialize( pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY ) )
        {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    const TextureFormat GuiColorFormat[]{ TextureFormat::R8G8B8A8_UNORM };
    const TEXTURE_TYPE GuiTextureTypes[]{ TT_RENDER_TARGET };

    if( !m_GuiRT.Initialize( pVulkan, gRenderWidth, gRenderHeight, GuiColorFormat, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "Gui RT", GuiTextureTypes ) )
    {
        return false;
    }

    m_Gui = std::make_unique<GuiImguiGfx<Vulkan>>(*pVulkan, m_GuiRT.m_RenderPass);
    if (!m_Gui->Initialize(windowHandle, m_GuiRT[0].m_Width, m_GuiRT[0].m_Height))
    {
        return false;
    }

    ImPlot::CreateContext();

    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateGui(float time_elapsed)
//-----------------------------------------------------------------------------
{
    if(!m_Gui)
    {
        return;
    }

    // Update Gui
    m_Gui->Update();

    ImGui::SetNextWindowSize(ImVec2((gRenderWidth * 3.0f) / 4.0f, 500.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2((float)gRenderWidth / 8.0f, gRenderHeight / 2.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SDP CLI Interface"))
    {
        if (ImGui::BeginCombo("Connected Devices", m_selected_device.c_str())) 
        {
            for (const auto& device : m_connected_devices) 
            {
                bool is_selected = (device == m_selected_device);
                if (ImGui::Selectable(device.c_str(), is_selected)) 
                {
                    m_selected_device = device;
                    InitializeModulesWithSelectedDevice();
                }

                if (is_selected) 
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginTabBar("Snapdragon Tabs")) 
        {
            for (auto& registered_module : m_modules)
            {
                if (ImGui::BeginTabItem(registered_module->GetModuleName())) 
                {
                    registered_module->Draw(time_elapsed);
                    ImGui::EndTabItem();
                }
            }

            if(ImGui::TabItemButton("Remove Tools", ImGuiTabItemFlags_Trailing))
            {
                auto connected_devices = GetConnectedDeviceList();
                if (connected_devices.size() == 1)
                {
                    for(auto& installed_module : m_modules)
                    {
                        installed_module->RemoveToolFromDevice();
                    }

                    InitializeModulesWithSelectedDevice();
                }
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // ImPlot::ShowDemoWindow();
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
    blitImageRegion.dstOffsets[1] = { (int)GetVulkan()->m_SurfaceWidth, (int)GetVulkan()->m_SurfaceHeight, 1 };
    vkCmdBlitImage(commandBuffer.m_VkCommandBuffer, srcRT.m_ColorAttachments.back().GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitImageRegion, VK_FILTER_NEAREST);
}

//-----------------------------------------------------------------------------
bool Application::UpdateCommandBuffer(uint32_t bufferIdx)
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    auto& commandBuffer = m_CommandBuffer[bufferIdx];

    if( !commandBuffer.Begin( m_GuiRT[0].m_FrameBuffer, m_GuiRT.m_RenderPass ) )
    {
        return false;
    }


    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width  = m_GuiRT[0].m_Width;
    Scissor.extent.height = m_GuiRT[0].m_Height;

    VkClearColorValue ClearColor[1] {0.1f, 0.0f, 0.0f, 0.0f};

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
        GetGui()->Render(commandBuffer.m_VkCommandBuffer);

        commandBuffer.EndRenderPass();
    }

    VkImage swapchainImage = pVulkan->m_SwapchainBuffers[bufferIdx].image;
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

    AddSwapchainBlitToCmdBuffer(commandBuffer, m_GuiRT[0], swapchainImage);

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
bool Application::UpdateUniforms( uint32_t bufferIdx )
//-----------------------------------------------------------------------------
{
#if 0
    ObjectVertUniform data{ };
    data.MVPMatrix = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix();
    data.ModelMatrix = glm::identity<glm::mat4>();
    UpdateUniformBuffer( GetVulkan(), m_ObjectVertUniform, data );
#endif
    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateCamera(float elapsedTime)
//-----------------------------------------------------------------------------
{
#if 0
    if (m_CameraController)
        m_Camera.UpdateController(elapsedTime, *m_CameraController);
    m_Camera.UpdateMatrices();
#endif
}

void Application::Render(float fltDiffTime)
{
    // Every x time, update device list
    // Ideally we should listed through ADB in case a device is connected instead doing a query
    // ever x seconds
    m_update_device_list_time_elapsed += fltDiffTime;
    if (m_update_device_list_time_elapsed > 2.5f)
    {
        m_update_device_list_time_elapsed = 0.0f;
        UpdateDeviceList();
    }

    for (auto& registered_module : m_modules)
    {
        registered_module->Update(fltDiffTime);
    }

    UpdateGui(fltDiffTime);

    UpdateCamera( fltDiffTime );

    auto* const pVulkan = GetVulkan();

    auto CurrentVulkanBuffer = pVulkan->SetNextBackBuffer();

    UpdateUniforms( CurrentVulkanBuffer.idx );

    UpdateCommandBuffer( CurrentVulkanBuffer.idx );

    // ... submit the command buffer to the device queue
    m_CommandBuffer[CurrentVulkanBuffer.idx].QueueSubmit( CurrentVulkanBuffer, pVulkan->m_RenderCompleteSemaphore );

    // and Present
    PresentQueue( pVulkan->m_RenderCompleteSemaphore, CurrentVulkanBuffer.swapchainPresentIdx );

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
