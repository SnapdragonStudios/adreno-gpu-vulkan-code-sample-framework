//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <memory>
#include <vector>
#include "imguiPlatform.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/commandBuffer.hpp"
#include "vulkan/renderPass.hpp"

///
/// @brief Vulkan specialized implementation of imgui rendering.
/// Derives from the Windows / Android (or whatever) platform specific class implementation.
/// @ingroup GUI
///
template<>
class GuiImguiGfx<Vulkan> : public GuiImguiPlatform
{
public:
    GuiImguiGfx(Vulkan&, RenderPass<Vulkan> renderPass = {});
    ~GuiImguiGfx();

    bool Initialize(uintptr_t windowHandle, TextureFormat renderFormat, uint32_t renderWidth, uint32_t renderHeight) override;
    void Update() override;

    VkCommandBuffer Render(uint32_t frameIdx, VkFramebuffer frameBuffer);
    void Render(VkCommandBuffer cmdBuffer);

private:
    const RenderPass<Vulkan>            m_RenderPass {};  // if null, use dynamic rendering
    VkDescriptorPool                    m_DescriptorPool = VK_NULL_HANDLE;
    CommandListVulkan                   m_UploadCommandBuffer;
    std::vector<CommandListVulkan>      m_CommandBuffer;
    Vulkan&                             m_GfxApi;      
};

