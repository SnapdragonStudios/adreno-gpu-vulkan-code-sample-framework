//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "imguiBase.hpp"
#include "imgui.h"

bool GuiImguiBase::Initialize(uintptr_t windowHandle, uint32_t renderWidth, uint32_t renderHeight)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    return true;
}


void GuiImguiBase::Update()
{
    ImGui::NewFrame();
//    ImGui::ShowDemoWindow();
}

bool GuiImguiBase::WantCaptureMouse() const
{
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

bool GuiImguiBase::WantCaptureKeyboard() const
{
    const ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;
}
