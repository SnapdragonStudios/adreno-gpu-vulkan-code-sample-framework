//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <memory>
#include <vector>
#include "imguiPlatform.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/commandBuffer.hpp"

///
/// @brief Vulkan specialized implementation of imgui rendering.
/// Derives from the Windows / Android (or whatever) platform specific class implementation.
/// @ingroup GUI
///
template<>
class GuiImguiGfx<Vulkan> : public GuiImguiPlatform
{
public:
    GuiImguiGfx(Vulkan&, VkRenderPass renderPass);
    ~GuiImguiGfx();

    bool Initialize(uintptr_t windowHandle, uint32_t renderWidth, uint32_t renderHeight) override;
    void Update() override;

    VkCommandBuffer Render(uint32_t frameIdx, VkFramebuffer frameBuffer);
    void Render(VkCommandBuffer cmdBuffer);

private:
    const VkRenderPass                  m_RenderPass;
    VkDescriptorPool                    m_DescriptorPool = VK_NULL_HANDLE;
    Wrap_VkCommandBuffer                m_UploadCommandBuffer;
    std::vector<Wrap_VkCommandBuffer>   m_CommandBuffer;
    Vulkan&                             m_GfxApi;      
};

