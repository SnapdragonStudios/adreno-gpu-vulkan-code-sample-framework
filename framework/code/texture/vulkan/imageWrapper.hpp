//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <vulkan/vulkan.h>
#include "memory/vulkan/memoryMapped.hpp"

// Forward declarations
class Vulkan;
template<typename T_GFXAPI, typename T_BUFFER> class MemoryAllocatedBuffer;
enum class MemoryUsage;


/// @brief Wrapper around VkImage
/// Owns the image's memory buffer
class Wrap_VkImage
{
    // Functions
    Wrap_VkImage(const Wrap_VkImage&) = delete;
    Wrap_VkImage& operator=(const Wrap_VkImage&) = delete;
public:
    Wrap_VkImage();
    ~Wrap_VkImage();

    bool Initialize(Vulkan* pVulkan, const VkImageCreateInfo& ImageInfo, MemoryUsage TypeFlag, const char* pName = nullptr);
    void Release();

    const auto& GetImageInfo() const { return m_ImageInfo; }

    // Attributes
public:
    std::string                             m_Name;
    MemoryAllocatedBuffer<Vulkan, VkImage>  m_VmaImage;

private:
    Vulkan* m_pVulkan = nullptr;
    MemoryUsage                             m_Usage;
    VkImageCreateInfo                       m_ImageInfo;
};
