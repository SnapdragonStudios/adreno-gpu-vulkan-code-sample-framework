// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#include "androidHardwareBuffer.hpp"

// Placeholder

#if 0

// Almost there!  Need to create the import structure to pull AHardwareBuffer in
VkImportAndroidHardwareBufferInfoANDROID ImportBuffInfo;
memset(&ImportBuffInfo, 0, sizeof(ImportBuffInfo));
ImportBuffInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
ImportBuffInfo.pNext = NULL;
ImportBuffInfo.buffer = pBuffer;

// When importing AHB memory for a texture, we have a VkImage we pass in.
// Not sure what happens if creating a vertex buffer. Hopefully it is as 
// simple as passing in VK_NULL_HANDLE to the "image" parameter
VkMemoryDedicatedAllocateInfo DedicatedInfo;
memset(&DedicatedInfo, 0, sizeof(DedicatedInfo));
DedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
DedicatedInfo.pNext = &ImportBuffInfo;
DedicatedInfo.image = smCurrentImage;
DedicatedInfo.buffer = VK_NULL_HANDLE;

#endif
