// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#ifndef _VULKAN_VK_QCOM_RENDER_PASS_TRANSFORM_H_
#define _VULKAN_VK_QCOM_RENDER_PASS_TRANSFORM_H_

#include <vulkan/vulkan.h>

// VK_QCOM_render_pass_transform

#define VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM 2

#define VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM 1000282001
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM 1000282000

#define VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM_LEGACY 1000282000
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM_LEGACY	1000282001

typedef struct VkRenderPassTransformBeginInfoQCOM {
    VkStructureType                 sType;
    void* pNext;
    VkSurfaceTransformFlagBitsKHR   transform;
} VkRenderPassTransformBeginInfoQCOM;

typedef struct VkCommandBufferInheritanceRenderPassTransformInfoQCOM {
    VkStructureType                  sType;
    void* pNext;
    VkSurfaceTransformFlagBitsKHR    transform;
    VkRect2D                         renderArea;
} VkCommandBufferInheritanceRenderPassTransformInfoQCOM;

#endif // _VULKAN_VK_QCOM_RENDER_PASS_TRANSFORM_H_
