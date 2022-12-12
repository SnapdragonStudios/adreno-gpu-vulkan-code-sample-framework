//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "imguiWindows.hpp"
#define NOMINMAX
#include <Windows.h>
#include <imgui/imgui.h>
#include "imgui/backends/imgui_impl_win32.h"

// From WinMain.cpp
extern LRESULT(*PFN_Gui_WndProcHandler)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
// Imgui requests you extern this
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//
// Implementation of GuiImguiPlatform (for windows)
//

GuiImguiPlatform::GuiImguiPlatform()
{}

GuiImguiPlatform::~GuiImguiPlatform()
{
    PFN_Gui_WndProcHandler = nullptr;
}

bool GuiImguiPlatform::Initialize(uintptr_t windowHandle, uint32_t deviceWidth, uint32_t deviceHeight, uint32_t renderWidth, uint32_t renderHeight)
{
    if (!GuiImguiBase::Initialize(windowHandle, renderWidth, renderHeight))
    {
        return false;
    }

    if (!ImGui_ImplWin32_Init((void*)windowHandle))
    {
        return false;
    }

    PFN_Gui_WndProcHandler = &ImGui_ImplWin32_WndProcHandler;

    return true;
}

//-----------------------------------------------------------------------------

void GuiImguiPlatform::Update()
{
    ImGui_ImplWin32_NewFrame();
    GuiImguiBase::Update();
}

//-----------------------------------------------------------------------------
// Windows inputs are all handled by ImGui_ImplWin32_NewFrame
// No need to pass these events on to imGui (unlike on Android for instance!)
//-----------------------------------------------------------------------------

bool GuiImguiPlatform::TouchDownEvent(int iPointerID, float xPos, float yPos) { return false; }
bool GuiImguiPlatform::TouchMoveEvent(int iPointerID, float xPos, float yPos) { return false; }
bool GuiImguiPlatform::TouchUpEvent(int iPointerID, float xPos, float yPos) { return false; }
