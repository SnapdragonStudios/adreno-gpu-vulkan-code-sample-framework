//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

///
/// @file application.hpp
/// @brief Application implementation for 'empty' application.
/// 
/// Most basic application that compiles and runs with the Vulkan Framework.
/// DOES NOT initialize Vulkan.
/// 

#include "main/applicationHelperBase.hpp"
#include "vulkan/renderTarget.hpp"
#include "memory/vulkan/uniform.hpp"
#include "vulkan/commandBuffer.hpp"
#include "helpers/module_interface.hpp"
#include <memory>

class Application : public ApplicationHelperBase
{
public:
    Application();
    ~Application() override;

///////////////////////////////////
public: // ApplicationHelperBase //
///////////////////////////////////

    bool Initialize(uintptr_t windowHandle, uintptr_t hInstance) override;

    /// @brief Ticked every frame (by the Framework)
    /// @param fltDiffTime time (in seconds) since the last call to Render.
    void Render(float fltDiffTime) override;

///////////////////////////////////
public: ///////////////////////////
///////////////////////////////////

    bool InitCommandBuffers();

    /*
    */
    bool InitGui(uintptr_t windowHandle);

    /*
    */
    void UpdateGui(float time_elapsed);

    /*
    */
    void InitializeModulesWithSelectedDevice();

    /*
    */
    void UpdateDeviceList();

    void UpdateCamera(float elapsedTime);
    bool UpdateUniforms(uint32_t bufferIdx);
    bool UpdateCommandBuffer(uint32_t bufferIdx);

    void AddSwapchainBlitToCmdBuffer(Wrap_VkCommandBuffer& commandBuffer, const CRenderTarget& srcRT, VkImage swapchainImage);

private:

    CRenderTargetArray<1>                   m_GuiRT;

    Wrap_VkCommandBuffer                    m_CommandBuffer[NUM_VULKAN_BUFFERS];

    std::vector<std::unique_ptr<ModuleInterface>> m_modules;

    std::vector<std::string> m_connected_devices;
    std::string              m_selected_device;
    float                    m_update_device_list_time_elapsed = 0.0f;
};
