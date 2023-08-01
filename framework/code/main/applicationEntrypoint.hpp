//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "frameworkApplicationBase.hpp"

//
// This is the only function the application HAS to define when linking with the Framework.
// Should create an application defined class derived from FrameworkApplicationBase.
//
extern FrameworkApplicationBase* Application_ConstructApplication();
