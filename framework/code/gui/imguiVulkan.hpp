// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "imguiPlatform.hpp"
#include "vulkan/vulkan_support.hpp"

// forward declarations
class Vulkan;

///
/// @brief Vulkan specific implementation of imgui rendering.
/// Derives from the Windows / Android (or whatever) platform specific class implementation.
/// @ingroup GUI
///
class GuiImguiVulkan : public GuiImguiPlatform
{
public:
    GuiImguiVulkan(Vulkan& vulkan, VkRenderPass renderPass);
    ~GuiImguiVulkan();

    bool Initialize(uintptr_t windowHandle) override;
    VkCommandBuffer Render(uint32_t frameIdx, VkFramebuffer frameBuffer) override;
    void Update() override;

private:
    const VkRenderPass                  m_RenderPass;
    VkDescriptorPool                    m_DescriptorPool = VK_NULL_HANDLE;
    Wrap_VkCommandBuffer                m_UploadCommandBuffer;
    std::vector<Wrap_VkCommandBuffer>   m_CommandBuffer;
    Vulkan&                             m_Vulkan;      
};

