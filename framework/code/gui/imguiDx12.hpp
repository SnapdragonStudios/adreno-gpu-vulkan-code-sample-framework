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
#include <wrl/client.h>
#include "imguiPlatform.hpp"

class Dx12;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
template<class T_GFXAPI> class CommandList;
template<class T_GFXAPI> class RenderPass;
template<class T_GFXAPI> class RenderTarget;

///
/// @brief DirectX 12 specialized implementation of imgui rendering.
/// Derives from the Windows (or whatever) platform specific class implementation.
/// @ingroup GUI
///
template<>
class GuiImguiGfx<Dx12> : public GuiImguiPlatform
{
public:
    GuiImguiGfx(Dx12&, const RenderPass<Dx12>&/*unused*/);
    ~GuiImguiGfx();

    bool Initialize(uintptr_t windowHandle, TextureFormat renderFormat, uint32_t renderWidth, uint32_t renderHeight) override;
    void Update() override;

    ID3D12GraphicsCommandList* Render(uint32_t frameIdx, RenderTarget<Dx12>& renderTarget);
    void Render( ID3D12GraphicsCommandList* cmdBuffer);

private:
    Dx12&                                           m_GfxApi;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_DescriptorHeap;
    std::vector<CommandList<Dx12>>                 m_CommandList;
    std::unique_ptr<CommandList<Dx12>>             m_UploadCommandList;
};
