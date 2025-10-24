//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "imguiDx12.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "dx12/dx12.hpp"
#include "dx12/commandList.hpp"
#include "dx12/renderTarget.hpp"
#include "system/os_common.h"
#include <cassert>

// Forward declarations
using namespace Microsoft::WRL; // for ComPtr


//-----------------------------------------------------------------------------

GuiImguiGfx<Dx12>::GuiImguiGfx(Dx12& gfxApi, const RenderPass<Dx12>&/*unused*/)
    : GuiImguiPlatform()
    , m_GfxApi(gfxApi)
{}

//-----------------------------------------------------------------------------

GuiImguiGfx<Dx12>::~GuiImguiGfx()
{
    ImGui_ImplDX12_Shutdown();
}

//-----------------------------------------------------------------------------

bool GuiImguiGfx<Dx12>::Initialize(uintptr_t windowHandle, TextureFormat renderFormat, uint32_t renderWidth, uint32_t renderHeight)
{
    // Call the platform (windows/android) specific implementation initialize...
    if (!GuiImguiPlatform::Initialize(windowHandle, renderFormat, m_GfxApi.GetSurfaceWidth(), m_GfxApi.GetSurfaceHeight(), renderWidth, renderHeight))
    {
        return false;
    }

    // Make a descriptor heap just for the GUI
    ComPtr<ID3D12DescriptorHeap> descriptorHeap = m_GfxApi.CreateDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, nullptr );
    if (!descriptorHeap)
        return false;

    // Give imgui its own commandLists
    {
        m_CommandList.reserve( m_GfxApi.GetSwapchainBufferCount() );
        for (uint32_t imageIdx = 0; imageIdx < m_GfxApi.GetSwapchainBufferCount(); ++imageIdx)
        {
            if (!m_CommandList.emplace_back().Initialize( &m_GfxApi, "ImGui", CommandListBase::Type::Secondary ))
            {
                return false;
            }
        }
        m_UploadCommandList = std::make_unique<CommandList<Dx12>>();
        if (!m_UploadCommandList->Initialize( &m_GfxApi, "ImGui upload" ))
        {
            return false;
        }
    }

    // Create the ImGui Dx12 implementation!
    {
        const auto outputFormat = TextureFormatToDx( renderFormat );

        D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();

        if (!ImGui_ImplDX12_Init(m_GfxApi.GetDevice(),
                                 NUM_SWAPCHAIN_BUFFERS,
                                 outputFormat,
                                 m_GfxApi.GetShaderResourceViewDescriptorHeap().GetHeap() /*cbv_srv_heap*/,
                                 font_srv_cpu_desc_handle,
                                 font_srv_gpu_desc_handle))
        {
            return false;
        }
    }

    m_DescriptorHeap = std::move( descriptorHeap );

    return true;
}

//-----------------------------------------------------------------------------

void GuiImguiGfx<Dx12>::Update()
{
    ImGui_ImplDX12_NewFrame();
    GuiImguiPlatform::Update();
}

//-----------------------------------------------------------------------------

ID3D12GraphicsCommandList* GuiImguiGfx<Dx12>::Render(uint32_t frameIdx, RenderTarget<Dx12>& renderTarget)
{
    auto& commandList = m_CommandList[frameIdx];
    if (!commandList.Begin())
    {
        return nullptr;
    }

    //if (commandList.m_IsPrimary)
    //{
    //    VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    //    info.renderPass = m_RenderPass;
    //    info.framebuffer = frameBuffer;
    //    info.renderArea.extent.width = m_Vulkan.m_SurfaceWidth;
    //    info.renderArea.extent.height = m_Vulkan.m_SurfaceHeight;
    //    info.clearValueCount = 0;
    //    info.pClearValues = nullptr;
    //    vkCmdBeginRenderPass(m_CommandBuffer[frameIdx].m_VkCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    //}

    Render(commandList.Get());

    //if (m_CommandBuffer[frameIdx].m_IsPrimary)
    //{
    //    vkCmdEndRenderPass(m_CommandBuffer[frameIdx].m_VkCommandBuffer);
    //}

    commandList.End();
    return commandList.Get();
}

//-----------------------------------------------------------------------------

void GuiImguiGfx<Dx12>::Render(ID3D12GraphicsCommandList* commandList)
{
    auto* descriptorHeapPtr = m_DescriptorHeap.Get();
    commandList->SetDescriptorHeaps( 1, &descriptorHeapPtr);


    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

//-----------------------------------------------------------------------------

template class GuiImguiGfx<Dx12>;
