// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

///
/// @file application.hpp
/// @brief Application implementation for 'empty' application.
/// 
/// Most basic application that compiles and runs with the Vulkan Framework.
/// DOES NOT initialize Vulkan.
/// 

#include "main/frameworkApplicationBase.hpp"

class Application : public FrameworkApplicationBase
{
public:
    Application();
    ~Application() override;

    /// @brief Ticked every frame (by the Framework)
    /// @param fltDiffTime time (in seconds) since the last call to Render.
    void Render(float fltDiffTime) override;
};
