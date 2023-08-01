//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "imageWrapper.hpp"
#include "memory/memory.hpp"
#include "vulkan/vulkan.hpp"


//-----------------------------------------------------------------------------
Wrap_VkImage::Wrap_VkImage()
    : m_Usage{ MemoryUsage::Unknown }
    , m_ImageInfo{}
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
Wrap_VkImage::~Wrap_VkImage()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
bool Wrap_VkImage::Initialize(Vulkan* pVulkan, const VkImageCreateInfo& ImageInfo, MemoryUsage Usage, const char* pName)
//-----------------------------------------------------------------------------
{
    // If we have a name, save it
    if (pName != NULL)
    {
        m_Name = pName;
    }

    // Need Vulkan objects to release ourselves
    m_pVulkan = pVulkan;
    m_ImageInfo = ImageInfo;
    m_Usage = Usage;

    auto& memoryManager = pVulkan->GetMemoryManager();
    m_VmaImage = memoryManager.CreateImage(ImageInfo, Usage);

    if (m_VmaImage)
    {
        pVulkan->SetDebugObjectName(m_VmaImage.GetVkBuffer(), pName);
    }

    return !!m_VmaImage;
}

//-----------------------------------------------------------------------------
void Wrap_VkImage::Release()
//-----------------------------------------------------------------------------
{
    if (m_pVulkan)
    {
        auto& memoryManager = m_pVulkan->GetMemoryManager();
        memoryManager.Destroy(std::move(m_VmaImage));
    }
    m_pVulkan = nullptr;
    m_ImageInfo = {};
    m_Name.clear();
}
