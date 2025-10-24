//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "vulkan.hpp"
#include "extensionLib.hpp"
#include "system/os_common.h"
#include <cassert>
#include <cinttypes>

template<>
void VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eDevice>::Register(Vulkan& vulkan)
{
    vulkan.AddExtensionHooks(&m_InstanceFunctionPointerHook);
    vulkan.AddExtensionHooks(&m_DeviceFunctionPointerHook);
}

template<>
void VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eInstance>::Register(Vulkan& vulkan)
{
    vulkan.AddExtensionHooks(&m_InstanceFunctionPointerHook);
}
