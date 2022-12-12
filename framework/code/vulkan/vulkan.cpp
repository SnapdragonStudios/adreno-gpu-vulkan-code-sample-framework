//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "vulkan.hpp"
#include "extension.hpp"
#include "extensionHelpers.hpp"
#include "system/os_common.h"
#include <cassert>
#include <map>
#include <algorithm>

#include "system/config.h"

// Functions whose pointers are in the instance
PFN_vkGetPhysicalDeviceSurfaceSupportKHR        fpGetPhysicalDeviceSurfaceSupportKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   fpGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR  fpGetPhysicalDeviceSurfaceCapabilities2KHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR        fpGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR   fpGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
PFN_vkGetPhysicalDeviceProperties2              fpGetPhysicalDeviceProperties2 = nullptr;
PFN_vkGetPhysicalDeviceFeatures2                fpGetPhysicalDeviceFeatures2 = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties2        fpGetPhysicalDeviceMemoryProperties2 = nullptr;

// Functions whose pointers are in the device
PFN_vkCreateSwapchainKHR                        fpCreateSwapchainKHR = nullptr;
PFN_vkDestroySwapchainKHR                       fpDestroySwapchainKHR = nullptr;
PFN_vkGetSwapchainImagesKHR                     fpGetSwapchainImagesKHR = nullptr;
//PFN_vkAcquireNextImageKHR                       fpAcquireNextImageKHR = nullptr;

#if defined(OS_ANDROID)
#if __ANDROID_API__ < 29
PFN_vkGetAndroidHardwareBufferPropertiesANDROID fpGetAndroidHardwareBufferPropertiesANDROID = nullptr;
PFN_vkGetImageMemoryRequirements2               fpGetImageMemoryRequirements2 = nullptr;
#endif // __ANDROID_API__ < 29
#endif // OS_ANDROID

#if defined(USES_VULKAN_DEBUG_LAYERS)
PFN_vkCreateDebugReportCallbackEXT              fpCreateDebugReportCallback = nullptr;
PFN_vkDestroyDebugReportCallbackEXT             fpDestroyDebugReportCallback = nullptr;
PFN_vkDebugReportMessageEXT                     fpDebugReportMessage = nullptr;

VkDebugReportCallbackEXT                        gDebugCallbackHandle = VK_NULL_HANDLE;
#endif // USES_VULKAN_DEBUG_LAYERS

// Global access to GetDeviceProcAddr
PFN_vkGetDeviceProcAddr                         fpGetDeviceProcAddr = nullptr;

// If running on HLM, we don't want to use validation layers.
VAR(bool, gEnableValidation, true, kVariableNonpersistent);

// Forward declaration of helper functions
bool CheckVkError(const char* pPrefix, VkResult CheckVal);


//-----------------------------------------------------------------------------
Vulkan::Vulkan()
//-----------------------------------------------------------------------------
{
#if defined(OS_WINDOWS)
    m_hInstance = 0;
    m_hWnd = 0;
#endif // defined(OS_WINDOWS)

    // Surface dimensions should be set by the platform specific code (eg the window size in Windows, screen size from Android).  Set some defaults.
    m_SurfaceWidth = 1280;
    m_SurfaceHeight = 720;

    m_SwapchainPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    m_UseRenderPassTransform = false; // default to not bothering the app with this (but potentially a surfaceflinger performance hit and may be outputting hdr surfaces through a 8bit composition buffer)
    m_UsePreTransform = false;  // default to letting the device (or m_UseRenderPassTransform) handle any rotation.  See m_UseRenderPassTransform for performance/hdr notes.

    m_SwapchainImageCount = 0;
    m_VulkanSwapchain = VK_NULL_HANDLE;
    m_SwapchainCurrentIndx = 0;
    // m_SwapchainDepth;

    // Vulkan Objects
    m_VulkanInstance = VK_NULL_HANDLE;
    m_VulkanGpuCount = 0;

    m_VulkanGpuIdx = 0;

    m_pDeviceLayerNames.clear();
    
    m_LayerKhronosValidationAvailable = false;
    m_ExtGlobalPriorityAvailable = false;
    m_ExtSwapchainColorspaceAvailable = false;
    m_ExtSurfaceCapabilities2Available = false;
    m_ExtRenderPassTransformAvailable = false;
    m_ExtRenderPassShaderResolveAvailable = false;
    m_ExtRenderPassTransformLegacy = false;
    m_ExtRenderPassTransformEnabled = false;
    m_ExtValidationFeaturesVersion = 0;
    m_ExtPortability = false;
#if defined (OS_ANDROID)
    m_ExtExternMemoryCapsAvailable = false;
    m_ExtAndroidExternalMemoryAvailable = false;
#endif // defined (OS_ANDROID)

    m_VulkanGraphicsQueueIndx = -1;
    m_VulkanGraphicsQueueSupportsCompute = false;
    m_VulkanComputeQueueIndx = -1;
    m_VulkanTransferQueueIndx = -1;

    m_VulkanDevice = VK_NULL_HANDLE;
    m_VulkanQueue = VK_NULL_HANDLE;
    m_VulkanComputeQueue = VK_NULL_HANDLE;

    m_VulkanSurfaceCaps = {};

    m_VulkanCmdPool = VK_NULL_HANDLE;
    m_VulkanComputeCmdPool = VK_NULL_HANDLE;
    m_SetupCmdBuffer = VK_NULL_HANDLE;

    m_VulkanQueryPool = VK_NULL_HANDLE;

    m_PipelineCache = VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
Vulkan::~Vulkan()
//-----------------------------------------------------------------------------
{
    DestroyFrameBuffers();
    DestroySwapchainRenderPass();
    DestroySwapChain();

    //DestroyMemoryManager();
    m_MemoryManager.Destroy();

    //DestroyCommandPools();
    vkDestroyCommandPool( m_VulkanDevice, m_VulkanComputeCmdPool, nullptr );
    vkDestroyCommandPool( m_VulkanDevice, m_VulkanCmdPool, nullptr );

    if (m_VulkanQueryPool != VK_NULL_HANDLE)
    {
        vkDestroyQueryPool(m_VulkanDevice, m_VulkanQueryPool, nullptr);
    }

    if (m_PipelineCache != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(m_VulkanDevice, m_PipelineCache, nullptr);
    }

    // Destroy Sync objects
    vkDestroyFence(m_VulkanDevice, m_Fence, nullptr );
    vkDestroySemaphore(m_VulkanDevice, m_RenderCompleteSemaphore, nullptr);

    // Destroy Device
    m_VulkanQueue = VK_NULL_HANDLE;
    m_VulkanComputeQueue = VK_NULL_HANDLE;
    vkDestroyDevice( m_VulkanDevice, nullptr );
    m_VulkanDevice = VK_NULL_HANDLE;

    //DestroyCompute();
    //DestroySurface();
    vkDestroySurfaceKHR( m_VulkanInstance, m_VulkanSurface, nullptr );

#if defined(USES_VULKAN_DEBUG_LAYERS)
    if(gEnableValidation)
        ReleaseDebugCallback();
#endif // USES_VULKAN_DEBUG_LAYERS

    //DestroyInstanceFunctions();
    //DestroyQueue();
    m_pVulkanQueueProps.clear();
        
    //DestroyPhysicalDevices();
    m_VulkanGpu = VK_NULL_HANDLE;
    m_VulkanGpuCount = 0;
    m_VulkanGpuIdx = 0;
    //DestroyInstance();

    // Debug/Validation Layers
    m_InstanceLayerProps.clear();
    m_InstanceExtensionProps.clear();
    m_InstanceLayerNames.clear();
    m_InstanceExtensionNames.clear();
}


//-----------------------------------------------------------------------------
bool Vulkan::Init(uintptr_t windowHandle, uintptr_t hInst, const Vulkan::tSelectSurfaceFormatFn& SelectSurfaceFormatFn, const Vulkan::tConfigurationFn& CustomConfigurationFn)
//-----------------------------------------------------------------------------
{
#if defined (OS_WINDOWS)
    m_hWnd = (HWND)windowHandle;
    m_hInstance = (HINSTANCE)hInst;
#elif defined (OS_ANDROID)
    m_pAndroidWindow = (ANativeWindow*)windowHandle;
#endif // defined (OS_WINDOWS|OS_ANDROID)

    if (CustomConfigurationFn)
        CustomConfigurationFn( m_ConfigOverride );

    if (!RegisterKnownExtensions())
        return false;

    if (!CreateInstance())
        return false;

    if (!InitInstanceFunctions())
        return false;

    if (!GetPhysicalDevices())
        return false;

    if (!InitQueue())
        return false;

#if defined(USES_VULKAN_DEBUG_LAYERS)
    if (gEnableValidation && m_LayerKhronosValidationAvailable && !InitDebugCallback())
        return false;
#endif // USES_VULKAN_DEBUG_LAYERS

    if (!InitSurface())
        return false;

    if (!InitCompute())
        return false;

    if (!InitDevice())
        return false;

    if (!InitSyncElements())
        return false;

    if (!InitCommandPools())
        return false;

    if (!InitQueryPools())
        return false;

    if (!InitMemoryManager())
        return false;

    InitPipelineCache();    //ok for this to fail!

    if (!QuerySurfaceCapabilities())
        return false;

    if (SelectSurfaceFormatFn)
    {
        int surfaceFormatIndex = SelectSurfaceFormatFn(m_SurfaceFormats);
        if (surfaceFormatIndex >= 0)
        {
            m_SurfaceFormat = m_SurfaceFormats[surfaceFormatIndex].format;
            if (m_SurfaceFormat == VK_FORMAT_UNDEFINED) // this is allowed in the spec
                m_SurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
            m_SurfaceColorSpace = m_SurfaceFormats[surfaceFormatIndex].colorSpace;
        }
    }

    if (!InitSwapChain())
        return false;

    if (!InitSwapchainRenderPass())
        return false;

    if (!InitFrameBuffers())
        return false;

    return true;
}

//-----------------------------------------------------------------------------
void Vulkan::Terminate()
//-----------------------------------------------------------------------------
{
    // Destroy the callback
#if defined(USES_VULKAN_DEBUG_LAYERS)
    if(gEnableValidation)
        ReleaseDebugCallback();
#endif // USES_VULKAN_DEBUG_LAYERS

}

//-----------------------------------------------------------------------------
bool Vulkan::ReInit(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
#if defined (OS_WINDOWS)
    return false;   ///TODO: implement ReInit on Windows
#elif defined (OS_ANDROID)
    if (m_pAndroidWindow != (ANativeWindow*)windowHandle)
    {
        return false;
    }
    return true;    // Success
#endif // defined (OS_WINDOWS|OS_ANDROID)
}
    
//-----------------------------------------------------------------------------
VkCompositeAlphaFlagBitsKHR Vulkan::GetBestVulkanCompositeAlpha()
//-----------------------------------------------------------------------------
{
    // Find a supported composite alpha format
    VkCompositeAlphaFlagBitsKHR DesiredCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // Win32 seems to support Opaque, but Android likes Inherit
    // Looking through to find the supported one seems the best way to handle it.
    VkCompositeAlphaFlagBitsKHR PossibleCompositAlphaFormats[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    uint32_t NumPossibleFormats = sizeof(PossibleCompositAlphaFormats) / sizeof(VkCompositeAlphaFlagBitsKHR);

    for (uint32_t WhichFormat = 0; WhichFormat < NumPossibleFormats; WhichFormat++)
    {
        VkCompositeAlphaFlagBitsKHR OneFormat = PossibleCompositAlphaFormats[WhichFormat];
        if (m_VulkanSurfaceCaps.supportedCompositeAlpha & OneFormat)
        {
            // Found a supported format
            DesiredCompositeAlpha = OneFormat;
            break;
        }
    }

    return DesiredCompositeAlpha;
}

//-----------------------------------------------------------------------------
VkFormat Vulkan::GetBestVulkanDepthFormat(bool NeedStencil)
//-----------------------------------------------------------------------------
{
    // Start with the highest depth format we can, and work down.
    // We could start with VK_FORMAT_UNDEFINED and then check if we
    // didn't find an optimized format, but that doesn't mean the format
    // is not supported, just not optimized.
    VkFormat DesiredDepthFormat = NeedStencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT;

    // Order of the highest precision depth formats, favoring the ones without stencil if available.
    const auto PossibleDepthFormats = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT
    };
    // Highest to lowest precision depth formats, with stencil bits.
    const auto PossibleDepthFormatsWithStencil = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT
    };

    if( NeedStencil )
    {
        for(const auto& DepthFormat: (NeedStencil ? PossibleDepthFormatsWithStencil : PossibleDepthFormats) )
        {
            VkFormatProperties OneFormatProps;
            vkGetPhysicalDeviceFormatProperties(m_VulkanGpu, DepthFormat, &OneFormatProps);

            // If depth/stencil supported with optimal tiling we have found what we want
            if (OneFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                DesiredDepthFormat = DepthFormat;
                break;
            }
        }
    }

    // TODO: Start with VK_FORMAT_UNDEFINED and check if we found something
    return DesiredDepthFormat;
}

//-----------------------------------------------------------------------------
bool Vulkan::FormatHasStencil( VkFormat format )
//-----------------------------------------------------------------------------
{
    switch( format ) {
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool Vulkan::FormatHasDepth( VkFormat format )
//-----------------------------------------------------------------------------
{
    switch (format) {
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool Vulkan::FormatIsCompressed( VkFormat format )
//-----------------------------------------------------------------------------
{
    switch( format ) {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool Vulkan::FormatIsSrgb( VkFormat format )
//-----------------------------------------------------------------------------
{
    switch (format) {
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
bool Vulkan::IsTextureFormatSupported( VkFormat format ) const
//-----------------------------------------------------------------------------
{
    switch( format ) {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return m_VulkanGpuFeatures.Base.features.textureCompressionBC;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        return m_VulkanGpuFeatures.Base.features.textureCompressionETC2;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return m_VulkanGpuFeatures.Base.features.textureCompressionASTC_LDR;
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
        // disable all the PVR formats (not supported on any of our target hardware)
        return false;
    default:
        // Assume everything else is supported.
        return true;
    }
}


//-----------------------------------------------------------------------------
void Vulkan::SetImageLayout(VkImage image,
    VkCommandBuffer cmdBuffer,
    VkImageAspectFlags aspect,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcMask,
    VkPipelineStageFlags dstMask,
    uint32_t baseMipLevel,
    uint32_t mipLevelCount,
    uint32_t baseLayer,
    uint32_t layerCount)
    //-----------------------------------------------------------------------------
{
    VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;    //  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;   //  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange.aspectMask = aspect;
    imageMemoryBarrier.subresourceRange.baseMipLevel = baseMipLevel;
    imageMemoryBarrier.subresourceRange.levelCount = mipLevelCount;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = baseLayer;
    imageMemoryBarrier.subresourceRange.layerCount = layerCount;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = 0;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    switch (oldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // There is no need to barrier from 'Undefined'.  We need to know what we are coming from in order to barrier.
        // If we are clearing the image in the upcoming pipeline then Undefined oldLayout is valid, but no need to create a barrier!
        //assert(0);
        break;
    case VK_IMAGE_LAYOUT_GENERAL:
        // Old layout is General, assume this comes from Compute
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Old layout is color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Old layout is depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        LOGE("Don't know how to set image layout if starting with depth/stencil read only!");
        assert(0);
        return;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Old layout is shader read (sampler, input attachment)
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Old layout is transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Only allowed as initial layout!
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        // LOGE("Don't know how to set image layout if starting with VK_IMAGE_LAYOUT_PRESENT_SRC_KHR!");
        // return;
        break;

    default:
        // Compiler warning for enumeration not handled in switch
        break;
    }

    // ********************************
    // New Layout
    // ********************************
    switch (newLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Don't know what to do with this!
        LOGE("Don't know how to set image layout if transitioning to undefined!");
        assert(0);
        // return;
        break;

    case VK_IMAGE_LAYOUT_GENERAL:
        // New layout is 'general' (for compute storage buffer)
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        destStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // New layout is color attachment
        // Make sure any writes to the color buffer hav been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        destStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        if (oldLayout != VK_IMAGE_LAYOUT_UNDEFINED)
        {
            // This causes a validation warning if layout is undefined
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // New layout is depth attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        destStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // New layout is shader read (sampler, input attachment)
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        destStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // New layout is transfer source (copy, blit)
        // Make sure any reads from and writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        destStageFlags = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;

        // Make sure any reads from and writes to the image have been finished
        // imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
        // imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // New layout is transfer destination (copy, blit)
        // Make sure any reads from and writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        destStageFlags = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;

        // Make sure any copyies to the image have been finished
        // imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        destStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;

    default:
        // Compiler warning for enumeration not handled in switch
        break;
    }


    // Barrier on image memory, with correct layouts set.
    vkCmdPipelineBarrier(cmdBuffer,
        srcStageFlags,     // srcMask,
        destStageFlags,    // dstMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageMemoryBarrier);
}

//-----------------------------------------------------------------------------
bool Vulkan::RegisterKnownExtensions()
//-----------------------------------------------------------------------------
{
    //
    // Add any Vulkan Device Extensions we must have (expected to be a short list)
    //

    for ( auto& appExtension : m_ConfigOverride.AdditionalVulkanDeviceExtensions )
    {
        auto insertIt = m_DeviceExtensions.m_Extensions.insert( std::move( appExtension ) );
        assert( insertIt.second );  // check we didnt add a duplicate!
    }

    // This extension says the device must be able to present images directly to the screen.
    m_DeviceExtensions.AddExtension( VK_KHR_SWAPCHAIN_EXTENSION_NAME, VulkanExtension::eRequired );

    //
    // Add extensions we would always LIKE to initialize (if they exist).
    //
    m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_KHR_shader_float16_int8>( VulkanExtension::eOptional );
    m_ExtKhrDrawIndirectCount = m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_KHR_draw_indirect_count>( VulkanExtension::eOptional );
#if defined(USES_VULKAN_DEBUG_LAYERS)
    m_ExtDebugUtils  = m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_EXT_debug_utils>( VulkanExtension::eOptional );
    m_ExtDebugMarker = m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_EXT_debug_marker>( VulkanExtension::eOptional );
#else
    m_ExtDebugUtils  = m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_EXT_debug_utils>( VulkanExtension::eUninitialized );
    m_ExtDebugMarker = m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_EXT_debug_marker>( VulkanExtension::eUninitialized );
#endif
    m_DeviceExtensions.AddExtension( VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME, VulkanExtension::eOptional );
    m_ExtHdrMetadata = m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_EXT_hdr_metadata>( VulkanExtension::eOptional );
    m_DeviceExtensions.AddExtension( VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME, VulkanExtension::eOptional );
    m_DeviceExtensions.AddExtension( "VK_QCOM_render_pass_transform", VulkanExtension::eOptional);
    // This extension allows us to set  VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM (enable if available)
    m_DeviceExtensions.AddExtension( "VK_QCOM_render_pass_shader_resolve", VulkanExtension::eOptional);
    m_DeviceExtensions.AddExtension<ExtensionHelper::Ext_VK_KHR_portability_subset>( VulkanExtension::eOptional );

    m_SubgroupProperties = m_Vulkan11ProvidedExtensions.AddExtension<ExtensionHelper::Vulkan_SubgroupPropertiesHook>( VulkanExtension::eRequired );
    m_StorageFeatures = m_Vulkan11ProvidedExtensions.AddExtension<ExtensionHelper::Vulkan_StorageFeaturesHook>( VulkanExtension::eRequired );

#if defined (OS_ANDROID)
    m_DeviceExtensions.AddExtension( VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME, VulkanExtension::eOptional );
#endif

    m_ExtRenderPass2 = m_DeviceExtensions.GetExtension<ExtensionHelper::Ext_VK_KHR_create_renderpass2>();
    m_ExtFragmentShadingRate = m_DeviceExtensions.GetExtension<ExtensionHelper::Ext_VK_KHR_fragment_shading_rate>();

    // Now we have a list of all the extensions we know about we can ask them to Register themselves with whatever 'hooks' they require.
    m_DeviceExtensions.RegisterAll( *this );
    m_Vulkan11ProvidedExtensions.RegisterAll( *this );

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::CreateInstance()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ********************************
    // Debug/Validation/Whatever Layers
    // ********************************
    InitInstanceExtensions();

    // ********************************
    // Create the Vulkan Instance
    // ********************************

    // We specify the Vulkan version our application was built with,
    // as well as names and versions for our application and engine,
    // if applicable. This allows the driver to gain insight to what
    // is utilizing the vulkan driver, and serve appropriate versions.
    VkApplicationInfo AppInfoStruct{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    AppInfoStruct.pApplicationName = "VkFrameworkSample";
    AppInfoStruct.applicationVersion = 0;
    AppInfoStruct.pEngineName = "VkFrameworkEngine";
    AppInfoStruct.engineVersion = 0;
    AppInfoStruct.apiVersion = VK_MAKE_VERSION(1, 1, 0);    // VK_API_VERSION = VK_MAKE_VERSION(1, 1, 0). 

    // Creation information for the instance points to details about
    // the application, and also the list of extensions to enable.
    VkInstanceCreateInfo InstanceInfoStruct {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    InstanceInfoStruct.flags = 0;
    InstanceInfoStruct.pApplicationInfo = &AppInfoStruct;
    InstanceInfoStruct.enabledLayerCount = (uint32_t) m_InstanceLayerNames.size();
    InstanceInfoStruct.ppEnabledLayerNames = m_InstanceLayerNames.data();
    InstanceInfoStruct.enabledExtensionCount = (uint32_t) m_InstanceExtensionNames.size();
    InstanceInfoStruct.ppEnabledExtensionNames = m_InstanceExtensionNames.data();

    //
    // Potentially add Validation layer feature settings.
    //
    VkValidationFeaturesEXT ValidationFeaturesStruct { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
    if (m_ExtValidationFeaturesVersion >= 2)    // spec version 1 does not support 'best practices'
    {
#if (VK_EXT_VALIDATION_FEATURES_SPEC_VERSION < 4)   // if our header does not define the spec version 4 then add the 'missing' enable
#define VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT (4)
#endif
        std::vector<VkValidationFeatureEnableEXT> enables;
#if defined(VULKAN_VALIDATION_ENABLE_BEST_PRACTICES)
        enables.push_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#endif // VALIDATION_ENABLE_BEST_PRACTICES
#if defined(VULKAN_VALIDATION_ENABLE_SYNCHRONIZATION)
        if (m_ExtValidationFeaturesVersion >= 4) // spec versions before 4 do not support synchronization validation
        {
            enables.push_back((VkValidationFeatureEnableEXT)VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
        }
#endif // VULKAN_VALIDATION_ENABLE_SYNCHRONIZATION
        if (!enables.empty())
        {
            ValidationFeaturesStruct.enabledValidationFeatureCount = (uint32_t) enables.size();
            ValidationFeaturesStruct.pEnabledValidationFeatures = enables.data();
        }
        InstanceInfoStruct.pNext = &ValidationFeaturesStruct;
    }

    uint32_t MajorVersion = VK_VERSION_MAJOR(AppInfoStruct.apiVersion);
    uint32_t MinorVersion = VK_VERSION_MINOR(AppInfoStruct.apiVersion);
    uint32_t PatchVersion = VK_VERSION_PATCH(AppInfoStruct.apiVersion);
    LOGI("Requesting Vulkan version %d.%d.%d", MajorVersion, MinorVersion, PatchVersion);

    LOGI("Requesting Layers:");
    for (const auto& LayerName : m_InstanceLayerNames)
    {
        LOGI("    %s", LayerName);
    }

    LOGI("Requesting Extensions:");
    for (const auto& ExtensionName: m_InstanceExtensionNames)
    {
        LOGI("    %s", ExtensionName);
    }

    LOGI("Creating Vulkan Instance...");

    LOGI("    InstanceInfo.enabledLayerCount: %d", InstanceInfoStruct.enabledLayerCount);
    for (uint32_t Which = 0; Which < InstanceInfoStruct.enabledLayerCount; Which++)
    {
        LOGI("        %d: %s", Which, InstanceInfoStruct.ppEnabledLayerNames[Which]);
    }

    LOGI("    InstanceInfo.enabledExtensionCount: %d", InstanceInfoStruct.enabledExtensionCount);
    for (uint32_t Which = 0; Which < InstanceInfoStruct.enabledExtensionCount; Which++)
    {
        LOGI("        %d: %s", Which, InstanceInfoStruct.ppEnabledExtensionNames[Which]);
    }

    // The main Vulkan instance is created with the creation infos above.
    // We do not specify a custom memory allocator for instance creation.
    RetVal = vkCreateInstance(&InstanceInfoStruct, nullptr, &m_VulkanInstance);

    // Vulkan API return values can expose further information on a failure.
    // For instance, INCOMPATIBLE_DRIVER may be returned if the API level
    // an application is built with, exposed through VkApplicationInfo, is
    // newer than the driver present on a device.
    if (RetVal == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        LOGE("Cannot find a compatible Vulkan installable client driver: vkCreateInstance Failure");
        return false;
    }
    else if (RetVal == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        LOGE("Cannot find a specified extension library: vkCreateInstance Failure");
        return false;
    }
    else if (!CheckVkError("vkCreateInstance()", RetVal))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
void Vulkan::InitInstanceExtensions()
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    // ********************************
    // Look for Instance Validation Layers
    // ********************************
    {
        uint32_t NumInstanceLayerProps = 0;
        RetVal = vkEnumerateInstanceLayerProperties(&NumInstanceLayerProps, nullptr);
        if (!CheckVkError("vkEnumerateInstanceLayerProperties(nullptr)", RetVal))
        {
            return;
        }

        LOGI("Found %d Vulkan Instance Layer Properties", NumInstanceLayerProps);
        if (NumInstanceLayerProps > 0)
        {
            // Allocate memory for the structures...
            m_InstanceLayerProps.resize(NumInstanceLayerProps);

            // ... then read the structures from the driver
            RetVal = vkEnumerateInstanceLayerProperties(&NumInstanceLayerProps, m_InstanceLayerProps.data());
            if (!CheckVkError("vkEnumerateInstanceLayerProperties()", RetVal))
            {
                return;
            }

            for (uint32_t uiIndx = 0; uiIndx < NumInstanceLayerProps; uiIndx++)
            {
                const VkLayerProperties& rOneItem = m_InstanceLayerProps[uiIndx];
                LOGI("    %d: %s => %s", uiIndx, rOneItem.layerName, rOneItem.description);

                // Need to check that we found specific extensions
                if(!strcmp("VK_LAYER_KHRONOS_validation", rOneItem.layerName))
                {
                    m_LayerKhronosValidationAvailable = true;
                }
            }
        }
    }

    // ********************************
    // Look for Instance Extensions
    // ********************************
    {
        uint32_t NumInstanceExtensionProps = 0;
        RetVal = vkEnumerateInstanceExtensionProperties(nullptr, &NumInstanceExtensionProps, nullptr);
        if (!CheckVkError("vkEnumerateInstanceExtensionProperties(nullptr)", RetVal))
        {
            return;
        }

        LOGI("Found %d Vulkan Instance Extension Properties", NumInstanceExtensionProps);
        if (NumInstanceExtensionProps > 0)
        {
            // Allocate memory for the structures...
            m_InstanceExtensionProps.resize(NumInstanceExtensionProps);

            // ... then read the structures from the driver
            RetVal = vkEnumerateInstanceExtensionProperties(nullptr, &NumInstanceExtensionProps, m_InstanceExtensionProps.data());
            if (!CheckVkError("vkEnumerateInstanceExtensionProperties()", RetVal))
            {
                return;
            }
        }

        // Now add in any instance extensions exposed by layers (that we care about)
        if (m_LayerKhronosValidationAvailable)
        {
            uint32_t NumValidationLayerInstanceExtensionProps = 0;

            RetVal = vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &NumValidationLayerInstanceExtensionProps, nullptr);
            if (!CheckVkError("vkEnumerateInstanceExtensionProperties(VK_LAYER_KHRONOS_validation)", RetVal))
            {
                return;
            }

            LOGI("Found %d Vulkan VK_LAYER_KHRONOS_validation Instance Extension Properties", NumValidationLayerInstanceExtensionProps);
            if (NumValidationLayerInstanceExtensionProps > 0)
            {
                // Allocate memory for the structures...
                m_InstanceExtensionProps.resize(NumInstanceExtensionProps + NumValidationLayerInstanceExtensionProps);

                // ... then read the structures from the driver
                RetVal = vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &NumValidationLayerInstanceExtensionProps, &m_InstanceExtensionProps[NumInstanceExtensionProps]);
                if (!CheckVkError("vkEnumerateInstanceExtensionProperties(VK_LAYER_KHRONOS_validation)", RetVal))
                {
                    return;
                }
            }
            NumInstanceExtensionProps += NumValidationLayerInstanceExtensionProps;
        }

        if (NumInstanceExtensionProps > 0)
        {
            for (uint32_t uiIndx = 0; uiIndx < NumInstanceExtensionProps; uiIndx++)
            {
                const VkExtensionProperties& rOneItem = m_InstanceExtensionProps[uiIndx];
                LOGI("    %d: %s (%u)", uiIndx, rOneItem.extensionName, rOneItem.specVersion);

                if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, rOneItem.extensionName))
                {
                    // Found one of the extensions we are looking for
                }
                else if (!strcmp(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, rOneItem.extensionName))
                {
                    m_ExtSwapchainColorspaceAvailable = true;
                }
                else if (!strcmp(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, rOneItem.extensionName))
                {
                    m_ExtSurfaceCapabilities2Available = true;
                }
                else if (!strcmp(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, rOneItem.extensionName))
                {
                    m_ExtValidationFeaturesVersion = rOneItem.specVersion;
                }
#if defined (OS_ANDROID)
                else if (!strcmp("VK_KHR_external_memory_capabilities", rOneItem.extensionName))
                {
                    m_ExtExternMemoryCapsAvailable = true;
                }
#endif // defined (OS_ANDROID)
#if defined(USES_VULKAN_DEBUG_LAYERS)
                else if (!strcmp( VK_EXT_DEBUG_UTILS_EXTENSION_NAME, rOneItem.extensionName ))
                {
                    m_ExtDebugUtilsAvailable = true;
                }
#endif // USES_VULKAN_DEBUG_LAYERS
            }
        }
    }

    // Add layers we want for debugging
#if defined(USES_VULKAN_DEBUG_LAYERS)
    if (gEnableValidation && m_LayerKhronosValidationAvailable)
    {
        m_InstanceLayerNames.push_back( "VK_LAYER_KHRONOS_validation" );
    }
#endif // USES_VULKAN_DEBUG_LAYERS

    // Add the extensions we need
    m_InstanceExtensionNames.push_back( VK_KHR_SURFACE_EXTENSION_NAME );

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    m_InstanceExtensionNames.push_back( VK_KHR_WIN32_SURFACE_EXTENSION_NAME );
#endif // VK_USE_PLATFORM_WIN32_KHR

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    m_InstanceExtensionNames.push_back( VK_KHR_ANDROID_SURFACE_EXTENSION_NAME );
#endif // VK_USE_PLATFORM_ANDROID_KHR

#if defined(USES_VULKAN_DEBUG_LAYERS)
    // If this is NOT set, we cannot use the debug extensions! (Find vkCreateDebugReportCallback)
    if (gEnableValidation && m_LayerKhronosValidationAvailable)
    {
        m_InstanceExtensionNames.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
    }
    if (gEnableValidation && m_ExtDebugUtilsAvailable)
    {
        m_InstanceExtensionNames.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
    }
#endif // USES_VULKAN_DEBUG_LAYERS

    // This extension may enable more than the default VK_COLOR_SPACE_SRGB_NONLINEAR_KHR color space (enable if available)
    if (m_ExtSwapchainColorspaceAvailable)
    {
        m_InstanceExtensionNames.push_back( VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME );
    }

    // This extension allows us to call VkPhysicalDeviceSurfaceInfo2KHR (enable if available)
    if (m_ExtSurfaceCapabilities2Available)
    {
        m_InstanceExtensionNames.push_back( VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME );
    }
    
    // This extension allows us to use VkValidationFeaturesEXT (>0 if available)
#if defined(USES_VULKAN_DEBUG_LAYERS)
    if (gEnableValidation && m_ExtValidationFeaturesVersion > 4)
    {
        m_InstanceExtensionNames.push_back( VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME );
    }
#endif // defined(USES_VULKAN_DEBUG_LAYERS)

    m_InstanceExtensionNames.push_back( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME );   // Needed by Nvidia VK_KHR_deferred_host_operations

#if defined (OS_ANDROID)
    // This extension allow us to use Android Hardware Buffers
    // The basic problem is that there is a device extension requirement chain:
    //  VK_ANDROID_external_memory_android_hardware_buffer (Requires)
    //      VK_KHR_external_memory (Requires)
    //          VK_KHR_external_memory_capabilities <= Instance Extension!!!
    // So we do the instance extensions before we get to handling the device extensions!
    // Rather than rework the whole thing and check the device extensions here, just request it here.
    if (m_ExtExternMemoryCapsAvailable)
    {
        // LOGE("VK_KHR_external_memory_capabilities is added as instance extension %d", m_NumInstanceExtensions);
        m_InstanceExtensionNames.push_back( "VK_KHR_external_memory_capabilities" );
    }
#endif // defined (OS_ANDROID)

    {
        // Sort alphabetically (can search using std::lower_bound)
        std::sort(std::begin(m_InstanceExtensionNames), std::end(m_InstanceExtensionNames), [](auto a, auto b) { return strcmp(a, b) < 0; });
        size_t outIdx = 0;
        for (size_t inIdx = 0; inIdx < m_InstanceExtensionNames.size(); ++inIdx)
            if (inIdx == 0 || strcmp(m_InstanceExtensionNames[outIdx-1], m_InstanceExtensionNames[inIdx]) != 0)
                ++outIdx;

        // Remove duplicates
        m_InstanceExtensionNames.resize(outIdx);
    }
    {
        // Sort alphabetically (can search using std::lower_bound)
        std::sort(std::begin(m_InstanceLayerNames), std::end(m_InstanceLayerNames), [](auto a, auto b) { return strcmp(a, b) < 0; });
        size_t outIdx = 0;
        for (size_t inIdx = 0; inIdx < m_InstanceLayerNames.size(); ++inIdx)
            if (inIdx == 0 || strcmp(m_InstanceLayerNames[outIdx-1], m_InstanceLayerNames[inIdx]) != 0)
                ++outIdx;

        // Remove duplicates
        m_InstanceLayerNames.resize(outIdx);
    }
}

//-----------------------------------------------------------------------------
bool Vulkan::GetPhysicalDevices()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // Query number of physical devices available
    RetVal = vkEnumeratePhysicalDevices(m_VulkanInstance, &m_VulkanGpuCount, nullptr);
    if (!CheckVkError("vkEnumeratePhysicalDevices()", RetVal))
    {
        return false;
    }

    if (m_VulkanGpuCount == 0)
    {
        LOGE("Could not find a Vulkan GPU!!!!");
        return false;
    }

    LOGI("Found %d Vulkan GPU[s]", m_VulkanGpuCount);

    // Allocate space the the correct number of devices, before requesting their data
    std::vector<VkPhysicalDevice> pDevices;
    pDevices.resize(m_VulkanGpuCount, VK_NULL_HANDLE);

    RetVal = vkEnumeratePhysicalDevices(m_VulkanInstance, &m_VulkanGpuCount, pDevices.data());
    if (!CheckVkError("vkEnumeratePhysicalDevices()", RetVal))
    {
        return false;
    }

    // Query and display the available GPU devices and determine the 'best' GPU to use.
    {
        std::vector<PhysicalDeviceFeatures> DeviceFeatures;
        std::vector<PhysicalDeviceProperties> DeviceProperties;
        DeviceFeatures.reserve( pDevices.size() );
        DeviceProperties.reserve( pDevices.size() );

        for (VkPhysicalDevice device : pDevices)
        {
            QueryPhysicalDeviceFeatures( device, DeviceFeatures.emplace_back() );
            QueryPhysicalDeviceProperties( device, DeviceFeatures.back(), DeviceProperties.emplace_back() );
        }

        DumpDeviceInfo( DeviceFeatures, DeviceProperties );

        m_VulkanGpuIdx = (uint32_t) GetBestVulkanPhysicalDeviceId( DeviceProperties );
    }

    LOGI("Using Vulkan Device (GPU): %d", m_VulkanGpuIdx);

    m_VulkanGpu = pDevices[m_VulkanGpuIdx];

    // ********************************
    // Debug/Validation/Whatever Layers and extensions
    // ********************************
    if (!InitDeviceExtensions())
    {
        return false;
    }

    // Query the available features and properties for this device.
    PhysicalDeviceFeatures AvailableFeatures{};
    QueryPhysicalDeviceFeatures( m_VulkanGpu, AvailableFeatures );
    QueryPhysicalDeviceProperties( m_VulkanGpu, AvailableFeatures, m_VulkanGpuProperties );

    // Get Memory information and properties - this is required later, when we begin
    // allocating buffers to store data.
    if (fpGetPhysicalDeviceMemoryProperties2)
        fpGetPhysicalDeviceMemoryProperties2(m_VulkanGpu, &m_PhysicalDeviceMemoryProperties);
    else
        vkGetPhysicalDeviceMemoryProperties(m_VulkanGpu, &m_PhysicalDeviceMemoryProperties.memoryProperties);

    // VK_QCOM_render_pass_transform had 2 versions, switch to 'legacy' if on an older driver verson.
    m_ExtRenderPassTransformLegacy = (m_ExtRenderPassTransformAvailable && (m_VulkanGpuProperties.Base.properties.driverVersion < VK_MAKE_VERSION(512, 467, 0)));
    // Internal builds have version 366 hardcoded, use the supported Vulkan version instead (the switch to legacy before 1.1.126.0 may be incorrect, 1.1.124.0 cartainly was legacy though and 1.1.128 is not!)
    m_ExtRenderPassTransformLegacy = m_ExtRenderPassTransformLegacy && ((m_VulkanGpuProperties.Base.properties.driverVersion != VK_MAKE_VERSION(512, 366, 0) || m_VulkanGpuProperties.Base.properties.apiVersion < VK_MAKE_VERSION(1, 1, 126)));

    if( m_ExtRenderPassTransformLegacy )
    {
        LOGI( "VK_QCOM_render_pass_transform legacy values" );
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitDeviceExtensions()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ********************************
    // Look for Device Validation Layers
    // ********************************
    {
        uint32_t NumDeviceLayerProps = 0;
        RetVal = vkEnumerateDeviceLayerProperties(m_VulkanGpu, &NumDeviceLayerProps, nullptr);
        if (!CheckVkError("vkEnumerateDeviceLayerProperties(nullptr)", RetVal))
        {
            return false;
        }

        LOGI("Found %u Vulkan Device Layer Properties", NumDeviceLayerProps);
        if (NumDeviceLayerProps > 0)
        {
            // Allocate memory for the structures...
            m_pDeviceLayerProps.resize(NumDeviceLayerProps, {});

            // ... then read the structures from the driver
            RetVal = vkEnumerateDeviceLayerProperties(m_VulkanGpu, &NumDeviceLayerProps, m_pDeviceLayerProps.data());
            if (!CheckVkError("vkEnumerateInstanceLayerProperties()", RetVal))
            {
                return false;
            }

            for (uint32_t uiIndx = 0; uiIndx < (uint32_t) m_pDeviceLayerProps.size(); uiIndx++)
            {
                VkLayerProperties* pOneItem = &m_pDeviceLayerProps[uiIndx];
                LOGI("    %d: %s => %s", uiIndx, pOneItem->layerName, pOneItem->description);
            }
        }
    }

    // ********************************
    // Look for Device Extensions
    // ********************************
    {
        uint32_t NumDeviceExtensionProps = 0;
        RetVal = vkEnumerateDeviceExtensionProperties(m_VulkanGpu, nullptr, &NumDeviceExtensionProps, nullptr);
        if (!CheckVkError("vkEnumerateDeviceExtensionProperties(nullptr)", RetVal))
        {
            return false;
        }

        LOGI("Found %u Vulkan Device Extension Properties", NumDeviceExtensionProps );
        if (NumDeviceExtensionProps > 0)
        {
            // Allocate memory for the structures...
            std::vector<VkExtensionProperties> DeviceExtensionProps;
            DeviceExtensionProps.resize( NumDeviceExtensionProps );

            // ... then read the structures from the driver
            RetVal = vkEnumerateDeviceExtensionProperties(m_VulkanGpu, nullptr, &NumDeviceExtensionProps, DeviceExtensionProps.data());
            if (!CheckVkError("vkEnumerateDeviceExtensionProperties()", RetVal))
            {
                return false;
            }

            for (uint32_t uiIndx = 0; uiIndx < (uint32_t) DeviceExtensionProps.size(); uiIndx++)
            {
                VkExtensionProperties* pOneItem = &DeviceExtensionProps[uiIndx];
                LOGI("    %d: %s", uiIndx, pOneItem->extensionName);

                // Need to check if we found specific device extensions
                {
                    std::string extensionName = pOneItem->extensionName;
                    auto* pExtension = m_DeviceExtensions.GetExtension( extensionName );
                    if ( pExtension )
                    {
                        // Found an already known extension, see if we want to do anything with it.
                        switch ( pExtension->Status )
                        {
                        case VulkanExtension::eOptional:
                        case VulkanExtension::eRequired:
                            pExtension->Status = VulkanExtension::eLoaded;  // strictly not 'yet' loaded but will be shortly
                            break;
                        case VulkanExtension::eLoaded:
                        case VulkanExtension::eUninitialized:
                            break;
                        }
                        pExtension->Version = pOneItem->specVersion;
                    }
                    else
                    {
                        // Add to the list of known extensions (assume we dont want to load it)
                        m_DeviceExtensions.AddExtension( extensionName, VulkanExtension::eUninitialized, pOneItem->specVersion );
                    }
                }
            }
        }
    }

    // Do a final check of any extensions we 'required' but that the driver is not reporting as available to be loaded.
    for(const auto& extension: m_DeviceExtensions.m_Extensions )
        if ( extension.second->Status == VulkanExtension::eRequired )
        {
            LOGE( "Required Vulkan extension \"%s\" was not found", extension.first.c_str() );
            return false;
        }

    // Cache off any 'has loaded' flags that we want to hold on to.
    m_ExtGlobalPriorityAvailable = HasLoadedVulkanDeviceExtension( VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME );
    m_ExtRenderPassTransformAvailable = HasLoadedVulkanDeviceExtension( "VK_QCOM_render_pass_transform" );
    m_ExtRenderPassShaderResolveAvailable = HasLoadedVulkanDeviceExtension( "VK_QCOM_render_pass_shader_resolve" );
    m_ExtPortability = HasLoadedVulkanDeviceExtension( "VK_KHR_portability_subset" );
#if defined (OS_ANDROID)
    m_ExtAndroidExternalMemoryAvailable = HasLoadedVulkanDeviceExtension(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
#endif

    // Create a vector of all the extensions (names) we are going to load as part of InitDevice()
    std::vector<const char*> DeviceExtensionNames;
    for (const auto& e : m_DeviceExtensions.m_Extensions)
    {
        if (e.second->Status == VulkanExtension::eLoaded)
            DeviceExtensionNames.push_back(e.first.c_str());
    }

    // Add layers we want for debugging

    // Add any extensions that we only need if other extensions are available (ie dependancies).
#if defined (OS_ANDROID)
    if (m_ExtAndroidExternalMemoryAvailable)
    {
        // Validation Error:
        // Validation: [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387] Object : VK_NULL_HANDLE(Type = 1) | 
        // Missing extensions required by the device extension VK_ANDROID_external_memory_android_hardware_buffer : 
        // VK_KHR_sampler_ycbcr_conversion, 
        // VK_KHR_external_memory, 
        // VK_EXT_queue_family_foreign.
        // The Vulkan spec states : All required extensions for each extension in the 
        // VkDeviceCreateInfo::ppEnabledExtensionNames list must also be present in that list. 
        // (https ://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCreateDevice-ppEnabledExtensionNames-01387)
        DeviceExtensionNames.push_back("VK_KHR_sampler_ycbcr_conversion");
        DeviceExtensionNames.push_back("VK_KHR_external_memory");
        DeviceExtensionNames.push_back("VK_EXT_queue_family_foreign");

        // Validation Error:
        // Validation: [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387] Object : VK_NULL_HANDLE(Type = 1) | 
        // Missing extensions required by the device extension VK_KHR_sampler_ycbcr_conversion : 
        // VK_KHR_maintenance1, 
        // VK_KHR_bind_memory2, 
        // VK_KHR_get_memory_requirements2.
        // The Vulkan spec states : All required extensions for each extension in the 
        // VkDeviceCreateInfo::ppEnabledExtensionNames list must also be present in that list. 
        // (https ://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCreateDevice-ppEnabledExtensionNames-01387)
        DeviceExtensionNames.push_back("VK_KHR_maintenance1");
        DeviceExtensionNames.push_back("VK_KHR_bind_memory2");
        DeviceExtensionNames.push_back("VK_KHR_get_memory_requirements2");

        // Validation Error:
        // Validation: [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387] Object : VK_NULL_HANDLE(Type = 1) | 
        // Missing extension required by the device extension VK_KHR_external_memory : 
        // VK_KHR_external_memory_capabilities.
        // The Vulkan spec states : All required extensions for each extension in the 
        // VkDeviceCreateInfo::ppEnabledExtensionNames list must also be present in that list. 
        // (https ://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCreateDevice-ppEnabledExtensionNames-01387)
        // THIS IS AN INSTANCE EXTENSION! NOT A DEVICE EXTENSION!

        // Must add extension in InitInstanceExtensions()
        // LOGE("VK_KHR_external_memory_capabilities is added as instance extension %d", m_NumInstanceExtensions);
        // m_pInstanceExtensionNames[m_NumInstanceExtensions++] = (char*)"VK_KHR_external_memory_capabilities";

    }
#endif // defined (OS_ANDROID)

#if (OS_ANDROID) && defined(ANDROID_HARDWARE_BUFFER_SUPPORT)
    // Need support for External Memory
    DeviceExtensionNames.push_back("VK_KHR_external_memory_fd");
    DeviceExtensionNames.push_back("VK_KHR_external_memory");
    DeviceExtensionNames.push_back("VK_KHR_descriptor_update_template");
#endif // defined (OS_ANDROID)

    // Sort alphabetically (so we can search using std::lower_bound).  Also remove duplicates.
    {
        std::sort(std::begin(DeviceExtensionNames), std::end(DeviceExtensionNames), [](auto a, auto b) { return strcmp(a, b) < 0; });
        // Remove duplicates
        auto endIt = std::unique(std::begin(DeviceExtensionNames), std::end(DeviceExtensionNames), [](auto a, auto b) { return strcmp(a, b) == 0; });
        DeviceExtensionNames.resize(endIt - std::begin(DeviceExtensionNames));
    }

    for(const auto& n: DeviceExtensionNames)
        m_DeviceExtensions.m_Extensions[n]->Status = VulkanExtension::eLoaded;

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitQueue()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // Before we create our main Vulkan device, we must ensure our physical device
    // has queue families which can perform the actions we require. For this, we request
    // the number of queue families, and their properties.
    uint32_t VulkanQueuePropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_VulkanGpu, &VulkanQueuePropertiesCount, nullptr);
    LOGI("Found %d Vulkan Device Queues Properties", VulkanQueuePropertiesCount);

    if (VulkanQueuePropertiesCount == 0)
    {
        LOGE("No Vulkan Device Queues Present!");
        return false;
    }

    // Allocate memory for the structures...
    m_pVulkanQueueProps.resize(VulkanQueuePropertiesCount);

    // ... then read the structures from the driver
    vkGetPhysicalDeviceQueueFamilyProperties(m_VulkanGpu, &VulkanQueuePropertiesCount, m_pVulkanQueueProps.data());

    for (uint32_t uiIndx = 0; uiIndx < VulkanQueuePropertiesCount; uiIndx++)
    {
        // Need to check that we found specific flag
        VkQueueFamilyProperties* pOneItem = &m_pVulkanQueueProps[uiIndx];
        if (pOneItem->queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            LOGI("    %d: Supports GRAPHICS", uiIndx);
        }
        else
        {
            LOGI("    %d: Does NOT Support GRAPHICS", uiIndx);
        }

        if (pOneItem->queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            LOGI("    %d: Supports COMPUTE", uiIndx);
        }
        else
        {
            LOGI("    %d: Does NOT Support COMPUTE", uiIndx);
        }

        if (pOneItem->queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            LOGI("    %d: Supports TRANSFER", uiIndx);
        }
        else
        {
            LOGI("    %d: Does NOT Support TRANSFER", uiIndx);
        }

        if (m_ConfigOverride.NumTimerQueries.value_or(0) > 0)
        {
            if (pOneItem->timestampValidBits > 0)
            {
                LOGI("    %d: Supports TimeStamps (%d valid bits)", uiIndx, pOneItem->timestampValidBits);
            }
            else
            {
                LOGI("    %d: Does NOT Support TimeStamps", uiIndx);
            }
        }
    }


    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitInstanceFunctions()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

#if defined (OS_WINDOWS)
    // TODO: How do we define functions we want?

    // These functions are part of the instance so we must query for them using
    // "vkGetInstanceProcAddr" once the instance has been created
    fpGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    if (fpGetPhysicalDeviceSurfaceSupportKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceSurfaceSupportKHR");
    }

    fpGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    if (fpGetPhysicalDeviceSurfaceCapabilitiesKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    }

    fpGetPhysicalDeviceSurfaceCapabilities2KHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
    if (fpGetPhysicalDeviceSurfaceCapabilities2KHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceSurfaceCapabilities2KHR");
    }

    fpGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    if (fpGetPhysicalDeviceSurfaceFormatsKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceSurfaceFormatsKHR");
    }

    fpGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    if (fpGetPhysicalDeviceSurfacePresentModesKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceSurfacePresentModesKHR");
    }

    fpGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetSwapchainImagesKHR");
    if (fpGetSwapchainImagesKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetSwapchainImagesKHR");
    }
#endif // defined (OS_WINDOWS)

    fpGetPhysicalDeviceMemoryProperties2 = (PFN_vkGetPhysicalDeviceMemoryProperties2) vkGetInstanceProcAddr( m_VulkanInstance, "vkGetPhysicalDeviceMemoryProperties2" );
    if (fpGetPhysicalDeviceMemoryProperties2 == nullptr)
    {
        LOGE( "Unable to get function pointer from instance: vkGetPhysicalDeviceMemoryProperties2" );
    }
    LOGI( "Initialized function pointer from instance: vkGetPhysicalDeviceMemoryProperties2" );

#if defined (OS_ANDROID)
#if __ANDROID_API__ < 29
    fpGetAndroidHardwareBufferPropertiesANDROID = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetAndroidHardwareBufferPropertiesANDROID");
    if (fpGetAndroidHardwareBufferPropertiesANDROID == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetAndroidHardwareBufferPropertiesANDROID");
    }
    LOGI("Initialized function pointer from instance: vkGetAndroidHardwareBufferPropertiesANDROID");
    
    fpGetImageMemoryRequirements2 = (PFN_vkGetImageMemoryRequirements2)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetImageMemoryRequirements2");
    if (fpGetImageMemoryRequirements2 == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetImageMemoryRequirements2");
    }
    LOGI("Initialized function pointer from instance: vkGetImageMemoryRequirements2");
#endif // __ANDROID_API__ < 29

#endif // defined (OS_ANDROID)

    fpGetPhysicalDeviceSurfaceCapabilities2KHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
    if (fpGetPhysicalDeviceSurfaceCapabilities2KHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceSurfaceCapabilities2KHR");
    }

    fpGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceProperties2");
    if (fpGetPhysicalDeviceProperties2 == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceProperties2");
    }

    fpGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetPhysicalDeviceFeatures2");
    if (fpGetPhysicalDeviceFeatures2 == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetPhysicalDeviceProperties2");
    }

    // Call any registered extensions for instance funtion pointer lookups.
    {
        VulkanInstanceFunctionPointerLookup fn { m_VulkanInstance };
        m_VulkanInstanceFunctionPointerLookupExtensions.PushExtensions( &fn );
    }

    return true;
}

#if defined(USES_VULKAN_DEBUG_LAYERS)
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugReportFlagsEXT Flags, VkDebugReportObjectTypeEXT ObjectType, uint64_t SrcObj, size_t Location, int32_t Code, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    // Flags
    bool IsError = false;
    char* pFlagString = (char*)"Unknown";
    if (Flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        pFlagString = (char*)"Info";
        return false;   // Don't want to be spammed with this type of message!
    }
    if (Flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        pFlagString = (char*)"Warning";
    }
    if (Flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        pFlagString = (char*)"Perf";
    }
    if (Flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        IsError = true;
        pFlagString = (char*)"Error";
    }
    if (Flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
        pFlagString = (char*)"Debug";
        return false;   // Don't want to be spammed with this type of message!
    }

    // ObjectType is what caused the error, not where
    char* pObjectName;
    switch (ObjectType)
    {
    case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT:               pObjectName = (char*)"UNKNOWN";                break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT:              pObjectName = (char*)"INSTANCE";               break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:       pObjectName = (char*)"PHYSICAL_DEVICE";        break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT:                pObjectName = (char*)"DEVICE";                 break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT:                 pObjectName = (char*)"QUEUE";                  break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT:             pObjectName = (char*)"SEMAPHORE";              break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT:        pObjectName = (char*)"COMMAND_BUFFER";         break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT:                 pObjectName = (char*)"FENCE";                  break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT:         pObjectName = (char*)"DEVICE_MEMORY";          break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT:                pObjectName = (char*)"BUFFER";                 break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT:                 pObjectName = (char*)"IMAGE";                  break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT:                 pObjectName = (char*)"EVENT";                  break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT:            pObjectName = (char*)"QUERY_POOL";             break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT:           pObjectName = (char*)"BUFFER_VIEW";            break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT:            pObjectName = (char*)"IMAGE_VIEW";             break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT:         pObjectName = (char*)"SHADER_MODULE";          break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT:        pObjectName = (char*)"PIPELINE_CACHE";         break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:       pObjectName = (char*)"PIPELINE_LAYOUT";        break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT:           pObjectName = (char*)"RENDER_PASS";            break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT:              pObjectName = (char*)"PIPELINE";               break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT: pObjectName = (char*)"DESCRIPTOR_SET_LAYOUT";  break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT:               pObjectName = (char*)"SAMPLER";                break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:       pObjectName = (char*)"DESCRIPTOR_POOL";        break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT:        pObjectName = (char*)"DESCRIPTOR_SET";         break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT:           pObjectName = (char*)"FRAMEBUFFER";            break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT:          pObjectName = (char*)"COMMAND_POOL";           break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:           pObjectName = (char*)"SURFACE_KHR";            break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:         pObjectName = (char*)"SWAPCHAIN_KHR";          break;
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_EXT:          pObjectName = (char*)"DEBUG_REPORT";           break;
    default:                                                    pObjectName = (char*)"Unknown";                break;

    }

    // SrcObj gives handle to object of type "ObjectType" above.  Can use it to access the object if we want.

    // Code: Not sure what this is

    char szBuffer[2048];    // Bumped to 2048 based on Vulkan validation errors
    snprintf(szBuffer, sizeof(szBuffer) - 1, "Vulkan Debug: (%s) %s => %s: %s", pFlagString, pObjectName, pLayerPrefix, pMessage);
    szBuffer[sizeof(szBuffer) - 1] = 0;

    LOGE("********** Validation Message - Start **********");
    if (IsError)
    {
        LOGE("%s", szBuffer);
    }
    else
    {
        LOGI("%s", szBuffer);
    }
    LOGE("********** Validation Message - End **********");

    // if (IsError)
    //     LOGE("Vulkan Debug: (%s) %s => %s: %s", pFlagString, pObjectName, pLayerPrefix, pMessage);
    // else
    //     LOGI("Vulkan Debug: (%s) %s => %s: %s", pFlagString, pObjectName, pLayerPrefix, pMessage);

    // Return "True" to cause layer to bail out and command is NOT sent to Vulkan
    // Return "False" to send it to the Vulkan layer (behave like final product)
    // Note: returning "false" may still not give correct behaviour for errors from private/unknown extensions (such as QCOM_render_pass_transform)
    return false;
}
#endif // USES_VULKAN_DEBUG_LAYERS

#if defined(USES_VULKAN_DEBUG_LAYERS)
//-----------------------------------------------------------------------------
bool Vulkan::InitDebugCallback()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // Get the function pointers needed for debug
    fpCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_VulkanInstance, "vkCreateDebugReportCallbackEXT");
    if (fpCreateDebugReportCallback == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkCreateDebugReportCallbackEXT");
        return false;
    }

    fpDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_VulkanInstance, "vkDestroyDebugReportCallbackEXT");
    if (fpDestroyDebugReportCallback == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkDestroyDebugReportCallbackEXT");
        return false;
    }

    fpDebugReportMessage = (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(m_VulkanInstance, "vkDebugReportMessageEXT");
    if (fpDebugReportMessage == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkDebugReportMessageEXT");
        return false;
    }

    VkDebugReportCallbackCreateInfoEXT CallbackInfo {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};

    VkDebugReportFlagsEXT LocalFlags = 0;
    LocalFlags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
    LocalFlags |= VK_DEBUG_REPORT_WARNING_BIT_EXT;
    LocalFlags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    LocalFlags |= VK_DEBUG_REPORT_ERROR_BIT_EXT;
    LocalFlags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;

    CallbackInfo.flags = LocalFlags;
    CallbackInfo.pfnCallback = VulkanDebugCallback;
    CallbackInfo.pUserData = nullptr;

    RetVal = fpCreateDebugReportCallback(m_VulkanInstance, &CallbackInfo, nullptr, &gDebugCallbackHandle);
    if (!CheckVkError("fpCreateDebugReportCallback()", RetVal))
    {
        return false;
    }

    // TODO: This is never released (Call fpDestroyDebugReportCallback)

    return true;
}

#endif // USES_VULKAN_DEBUG_LAYERS

#if defined(USES_VULKAN_DEBUG_LAYERS)
//-----------------------------------------------------------------------------
bool Vulkan::ReleaseDebugCallback()
//-----------------------------------------------------------------------------
{
    if (gDebugCallbackHandle != VK_NULL_HANDLE)
    {
        fpDestroyDebugReportCallback(m_VulkanInstance, gDebugCallbackHandle, nullptr);
    }

    gDebugCallbackHandle = VK_NULL_HANDLE;

    return true;
}
#endif // USES_VULKAN_DEBUG_LAYERS

//-----------------------------------------------------------------------------
bool Vulkan::InitSurface()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ********************************
    // Create surface
    // ********************************
    LOGI("Creating Vulkan Surface...");

#if defined (OS_WINDOWS)

    VkWin32SurfaceCreateInfoKHR SurfaceInfoStruct {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    SurfaceInfoStruct.flags = 0;
    SurfaceInfoStruct.hinstance = m_hInstance;
    SurfaceInfoStruct.hwnd = m_hWnd;
    RetVal = vkCreateWin32SurfaceKHR(m_VulkanInstance, &SurfaceInfoStruct, nullptr, &m_VulkanSurface);

#elif defined (OS_ANDROID)

    VkAndroidSurfaceCreateInfoKHR SurfaceInfoStruct {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    SurfaceInfoStruct.flags = 0;
    SurfaceInfoStruct.window = m_pAndroidWindow;
    RetVal = vkCreateAndroidSurfaceKHR(m_VulkanInstance, &SurfaceInfoStruct, nullptr, &m_VulkanSurface);

#endif // defined (OS_WINDOWS|OS_ANDROID)

    if (!CheckVkError("vkCreateXXXXSurfaceKHR()", RetVal))
    {
        return false;
    }

    // ********************************
    // Look for queue that supports presenting
    // ********************************
    // Iterate over each queue to learn whether it supports presenting:
    VkBool32* pSupportFlags = new VkBool32[m_pVulkanQueueProps.size()];

    for (uint32_t uiIndx = 0; uiIndx < m_pVulkanQueueProps.size(); uiIndx++)
    {
#if defined (OS_WINDOWS)
        fpGetPhysicalDeviceSurfaceSupportKHR(m_VulkanGpu, uiIndx, m_VulkanSurface, &pSupportFlags[uiIndx]);
#elif defined (OS_ANDROID)
        vkGetPhysicalDeviceSurfaceSupportKHR(m_VulkanGpu, uiIndx, m_VulkanSurface, &pSupportFlags[uiIndx]);
#endif // defined (OS_WINDOWS|OS_ANDROID)
    }

    // Look for graphics and present queues and hope there is one that supports both
    uint32_t GraphicsIndx = -1;
    uint32_t PresentIndx = -1;
    for (uint32_t uiIndx = 0; uiIndx < m_pVulkanQueueProps.size(); uiIndx++)
    {
        if (pSupportFlags[uiIndx] == VK_TRUE && PresentIndx == -1)
        {
            // This gives us a back up of at least the index.
            PresentIndx = uiIndx;
        }

        if ((m_pVulkanQueueProps[uiIndx].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            // If this is the first one that supports graphics grab it
            if (GraphicsIndx == -1)
                GraphicsIndx = uiIndx;

            if (pSupportFlags[uiIndx] == VK_TRUE)
            {
                // Found a queue that supports both graphics and present.  Grab it
                GraphicsIndx = uiIndx;
                PresentIndx = uiIndx;
                break;
            }
        }
    }

    // Look specifically for a transfer queue
    for (uint32_t uiIndx = 0; uiIndx < m_pVulkanQueueProps.size(); uiIndx++)
    {
        if ((m_pVulkanQueueProps[uiIndx].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0 &&
            (m_pVulkanQueueProps[uiIndx].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
            (m_pVulkanQueueProps[uiIndx].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
        {
            // Found a dedicated transfer queue
            LOGI("Found dedicated transfer queue (%d).", uiIndx);
            m_VulkanTransferQueueIndx = uiIndx;
            break;
        }
    }

    // No longer need the allocated memory
    delete[] pSupportFlags;
    pSupportFlags = nullptr;

    // If we didn't find either queue or they are not the same then we have a problem!
    if (GraphicsIndx == -1 || PresentIndx == -1 || GraphicsIndx != PresentIndx)
    {
        LOGE("Unable to find a queue that supports both graphics and present!");
        return false;
    }

    // We now have the queue we can use
    m_VulkanGraphicsQueueIndx = GraphicsIndx;

    // If no dedicated transfer queue just use the graphics queue
    if (m_VulkanTransferQueueIndx == -1)
    {
        LOGI("No dedicated transfer queue found. Using the graphics queue (%d).", m_VulkanGraphicsQueueIndx);
        m_VulkanTransferQueueIndx = m_VulkanGraphicsQueueIndx;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitCompute()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // See if the present queue supports compute.
    if (m_pVulkanQueueProps[m_VulkanGraphicsQueueIndx].queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
        m_VulkanGraphicsQueueSupportsCompute = true;
    }

    // Look for a queue that supports Compute (but not graphics)
    for (uint32_t i = 0; i < m_pVulkanQueueProps.size(); ++i)
    {
        if ((m_pVulkanQueueProps[i].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == VK_QUEUE_COMPUTE_BIT)
        {
            m_VulkanComputeQueueIndx = i;
            break;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitDevice()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ********************************
    // Query the available device features
    // ********************************
    PhysicalDeviceFeatures AvailableFeatures{};
    QueryPhysicalDeviceFeatures(m_VulkanGpu, AvailableFeatures);

    // Check for and enable the Vulkan Device features that we need to use.
    if (!m_ExtPortability && AvailableFeatures.Base.features.shaderStorageImageExtendedFormats == VK_FALSE)
    {
        LOGE("Vulkan physical device must support shaderStorageImageExtendedFormats");
        return false;
    }
    if (!m_ExtPortability && AvailableFeatures.Base.features.sampleRateShading == VK_FALSE)
    {
        LOGE("Vulkan physical device must support sampleRateShading");
        return false;
    }

    m_VulkanGpuFeatures = {};
    m_VulkanGpuFeatures.Base.features.geometryShader = AvailableFeatures.Base.features.geometryShader;
    m_VulkanGpuFeatures.Base.features.shaderStorageImageExtendedFormats = AvailableFeatures.Base.features.shaderStorageImageExtendedFormats;
    m_VulkanGpuFeatures.Base.features.sampleRateShading = AvailableFeatures.Base.features.sampleRateShading;
    m_VulkanGpuFeatures.Base.features.samplerAnisotropy = AvailableFeatures.Base.features.samplerAnisotropy;
    m_VulkanGpuFeatures.Base.features.shaderImageGatherExtended = AvailableFeatures.Base.features.shaderImageGatherExtended;
    m_VulkanGpuFeatures.Base.features.multiDrawIndirect = AvailableFeatures.Base.features.multiDrawIndirect;
    m_VulkanGpuFeatures.Base.features.drawIndirectFirstInstance = AvailableFeatures.Base.features.drawIndirectFirstInstance;

    m_VulkanGpuFeatures.Base.features.textureCompressionETC2 = AvailableFeatures.Base.features.textureCompressionETC2;
    m_VulkanGpuFeatures.Base.features.textureCompressionASTC_LDR = AvailableFeatures.Base.features.textureCompressionASTC_LDR;
    m_VulkanGpuFeatures.Base.features.textureCompressionBC = AvailableFeatures.Base.features.textureCompressionBC;

    m_VulkanGpuFeatures.Base.features.shaderFloat64 = AvailableFeatures.Base.features.shaderFloat64;
    m_VulkanGpuFeatures.Base.features.shaderInt64 = AvailableFeatures.Base.features.shaderInt64;
    m_VulkanGpuFeatures.Base.features.shaderInt16 = AvailableFeatures.Base.features.shaderInt16;

    // ********************************
    // Create the Device
    // ********************************
    float GraphicsPriority = 1.0f;
    float AsycComputePriority = 0.0f;   // values of 1 and 0 are gauranteed by the spec (and required), more than that needs discreteQueuePriorities check.

    VkDeviceQueueGlobalPriorityCreateInfoEXT DeviceQueueGlobalPriorityInfo {VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT};
    VkDeviceQueueCreateInfo DeviceQueueInfoStructs[2] = {};
    DeviceQueueInfoStructs[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    DeviceQueueInfoStructs[0].flags = 0;
    DeviceQueueInfoStructs[0].queueFamilyIndex = m_VulkanGraphicsQueueIndx;
    DeviceQueueInfoStructs[0].queueCount = 1;
    DeviceQueueInfoStructs[0].pQueuePriorities = &GraphicsPriority;
    if (m_VulkanComputeQueueIndx >= 0)
    {
        DeviceQueueInfoStructs[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        DeviceQueueInfoStructs[1].flags = 0;
        DeviceQueueInfoStructs[1].queueFamilyIndex = m_VulkanComputeQueueIndx;
        DeviceQueueInfoStructs[1].queueCount = 1;
        DeviceQueueInfoStructs[1].pQueuePriorities = &AsycComputePriority;

        // Attach VkDeviceQueueGlobalPriorityCreateInfoEXT
        if (m_ExtGlobalPriorityAvailable)
        {
            DeviceQueueGlobalPriorityInfo.globalPriority = m_ConfigOverride.AsyncQueuePriority.value_or(VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT);
            DeviceQueueInfoStructs[1].pNext = &DeviceQueueGlobalPriorityInfo;
        }
    }

    std::vector<const char*> DeviceExtensionNames;
    DeviceExtensionNames.reserve(m_DeviceExtensions.m_Extensions.size());
    for (const auto& e : m_DeviceExtensions.m_Extensions)
        if (e.second->Status == VulkanExtension::eLoaded)
            DeviceExtensionNames.push_back(e.first.c_str());

    VkDeviceCreateInfo DeviceInfoStruct = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    DeviceInfoStruct.pNext = &m_VulkanGpuFeatures.Base;             // Hardcoded features enabled through the 'pNext chain'
    DeviceInfoStruct.flags = 0;
    DeviceInfoStruct.queueCreateInfoCount = m_VulkanComputeQueueIndx >= 0 ? 2 : 1;
    DeviceInfoStruct.pQueueCreateInfos = DeviceQueueInfoStructs;
    DeviceInfoStruct.enabledLayerCount = (uint32_t) m_pDeviceLayerNames.size();
    DeviceInfoStruct.ppEnabledLayerNames = m_pDeviceLayerNames.data();
    DeviceInfoStruct.enabledExtensionCount = (uint32_t) DeviceExtensionNames.size();
    DeviceInfoStruct.ppEnabledExtensionNames = DeviceExtensionNames.data();
    m_DeviceCreateInfoExtensions.PushExtensions(&DeviceInfoStruct);   // Push registered features into the 'pNext chain'

    // TODO: Should this be checked against the queue to make sure timers are supported?
    VkPhysicalDeviceHostQueryResetFeaturesEXT ResetInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT };
    if (m_ConfigOverride.NumTimerQueries.value_or(0) > 0)
    {
        // Timer queries requested.  Insert into the pNext chain
        ResetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
        ResetInfo.pNext = (void *)DeviceInfoStruct.pNext;
        ResetInfo.hostQueryReset = VK_TRUE;
        DeviceInfoStruct.pNext = &ResetInfo;
    }

    LOGI("Creating Vulkan Device...");
    LOGI("    DeviceQueueInfo[0].queueFamilyIndex: %d", DeviceQueueInfoStructs[0].queueFamilyIndex);
    LOGI("    DeviceQueueInfo[0].queuePriority: %0.2f", DeviceQueueInfoStructs[0].pQueuePriorities[0]);
    if (m_VulkanComputeQueueIndx >= 0)
    {
        LOGI("    DeviceQueueInfo[1].queueFamilyIndex: %d", DeviceQueueInfoStructs[1].queueFamilyIndex);
        LOGI("    DeviceQueueInfo[1].queuePriority: %0.2f", DeviceQueueInfoStructs[1].pQueuePriorities[0]);
    }
    LOGI("    DeviceInfo.DeviceLayers: %d", DeviceInfoStruct.enabledLayerCount);
    for (uint32_t Which = 0; Which < DeviceInfoStruct.enabledLayerCount; Which++)
    {
        LOGI("        %d: %s", Which, DeviceInfoStruct.ppEnabledLayerNames[Which]);
    }

    LOGI("    DeviceInfo.DeviceExtensions: %d", DeviceInfoStruct.enabledExtensionCount);
    for (uint32_t Which = 0; Which < DeviceInfoStruct.enabledExtensionCount; Which++)
    {
        LOGI("        %d: %s", Which, DeviceInfoStruct.ppEnabledExtensionNames[Which]);
    }

    RetVal = vkCreateDevice(m_VulkanGpu, &DeviceInfoStruct, nullptr, &m_VulkanDevice);
    if (!CheckVkError("vkCreateDevice()", RetVal))
    {
        return false;
    }

    // Pop the extensions back off the VkPhysicalDeviceFeatures2 chain (most registered extensions will do nothing here)
    m_DeviceCreateInfoExtensions.PopExtensions(&DeviceInfoStruct);

    // ********************************
    // Get (Device) Function Pointers
    // ********************************
    // These functions are part of the device so we must query for them using
    // "vkGetDeviceProcAddr" once the device has been created
    if (!fpGetDeviceProcAddr)
    {
        fpGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(m_VulkanInstance, "vkGetDeviceProcAddr");
        if (fpGetDeviceProcAddr == nullptr)
        {
            LOGE("Unable to get function pointer from instance: vkGetInstanceProcAddr");
            return false;
        }
    }

    // Call any extensions that registered for device function pointer lookups.
    {
        VulkanDeviceFunctionPointerLookup fn { m_VulkanDevice, fpGetDeviceProcAddr };
        m_VulkanDeviceFunctionPointerLookupExtensions.PushExtensions( &fn );
    }


    // VK_KHR_swapchain functions
#if defined (OS_WINDOWS)
    fpCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)fpGetDeviceProcAddr(m_VulkanDevice, "vkCreateSwapchainKHR");
    if (fpCreateSwapchainKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkCreateSwapchainKHR");
    }

    fpDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)fpGetDeviceProcAddr(m_VulkanDevice, "vkDestroySwapchainKHR");
    if (fpDestroySwapchainKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkDestroySwapchainKHR");
    }

    fpGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)fpGetDeviceProcAddr(m_VulkanDevice, "vkGetSwapchainImagesKHR");
    if (fpGetSwapchainImagesKHR == nullptr)
    {
        LOGE("Unable to get function pointer from instance: vkGetSwapchainImagesKHR");
    }
#endif // defined (OS_WINDOWS)

    // ********************************
    // Create the Device Queue
    // ********************************
    vkGetDeviceQueue(m_VulkanDevice, m_VulkanGraphicsQueueIndx, 0, &m_VulkanQueue);

    // ********************************
    // Create the Compute Device Queue
    // ********************************
    if (m_VulkanComputeQueueIndx >= 0)
    {
        vkGetDeviceQueue(m_VulkanDevice, m_VulkanComputeQueueIndx, 0, &m_VulkanComputeQueue);
    }

    // ********************************
    // Get Supported Formats
    // ********************************
    uint32_t NumFormats;
#if defined (OS_WINDOWS)
    RetVal = fpGetPhysicalDeviceSurfaceFormatsKHR(m_VulkanGpu, m_VulkanSurface, &NumFormats, nullptr);
#elif defined (OS_ANDROID)
    RetVal = vkGetPhysicalDeviceSurfaceFormatsKHR(m_VulkanGpu, m_VulkanSurface, &NumFormats, nullptr);
#endif // defined (OS_WINDOWS|OS_ANDROID)
    if (!CheckVkError("vkGetPhysicalDeviceSurfaceFormatsKHR()", RetVal))
    {
        return false;
    }

    LOGI("Found %d Vulkan Supported Formats", NumFormats);

    if (NumFormats > 0)
    {
        // Allocate memory for the formats...
        m_SurfaceFormats.resize(NumFormats);

        // ... then read the structures from the driver
#if defined (OS_WINDOWS)
        RetVal = fpGetPhysicalDeviceSurfaceFormatsKHR(m_VulkanGpu, m_VulkanSurface, &NumFormats, m_SurfaceFormats.data());
#elif defined (OS_ANDROID)
        RetVal = vkGetPhysicalDeviceSurfaceFormatsKHR(m_VulkanGpu, m_VulkanSurface, &NumFormats, m_SurfaceFormats.data());
#endif // defined (OS_WINDOWS|OS_ANDROID)
        if (!CheckVkError("vkGetPhysicalDeviceSurfaceFormatsKHR()", RetVal))
        {
            return false;
        }
        if (NumFormats == 0)
        {
            return false;
        }

        for (uint32_t uiIndx = 0; uiIndx < NumFormats; uiIndx++)
        {
            const VkSurfaceFormatKHR& FormatKHR = m_SurfaceFormats[uiIndx];
            LOGI("    %d: (%d) %s", uiIndx, FormatKHR.format, VulkanFormatString(FormatKHR.format));
            LOGI("    %d: (%d) %s", uiIndx, FormatKHR.colorSpace, VulkanColorSpaceString(FormatKHR.colorSpace));
        }

        // If the surface come back as VK_FORMAT_UNDEFINED and there is only one then
        // it doesn't have a preferred format
        if (NumFormats == 1 && m_SurfaceFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            m_SurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
        }
        else
        {
            // Just take the first one by default
            m_SurfaceFormat = m_SurfaceFormats[0].format;
        }

        // Taking the first colorspace
        m_SurfaceColorSpace = m_SurfaceFormats[0].colorSpace;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitSyncElements()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // We have semaphores for rendering and backbuffer signalling.
    VkSemaphoreCreateInfo semCreateInfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semCreateInfo.flags = 0;

    RetVal = vkCreateSemaphore(m_VulkanDevice, &semCreateInfo, nullptr, &m_RenderCompleteSemaphore);
    if (!CheckVkError("vkCreateSemaphore()", RetVal))
    {
        return false;
    }

    // Create a fence to use for syncing the layout changes
    VkFenceCreateInfo fenceCreateInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    RetVal = vkCreateFence(m_VulkanDevice, &fenceCreateInfo, nullptr, &m_Fence);
    if (!CheckVkError("vkCreateFence()", RetVal))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitCommandPools()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ********************************
    // Create the command pool
    // ********************************
    LOGI("Creating Vulkan Command Pool...");

    VkCommandPoolCreateInfo CmdPoolInfoStruct {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    CmdPoolInfoStruct.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // Must set this flag in order to reset a command buffer from this pool
    CmdPoolInfoStruct.queueFamilyIndex = m_VulkanGraphicsQueueIndx;

    RetVal = vkCreateCommandPool(m_VulkanDevice, &CmdPoolInfoStruct, nullptr, &m_VulkanCmdPool);
    if (!CheckVkError("vkCreateCommandPool()", RetVal))
    {
        return false;
    }

    VkCommandBufferAllocateInfo cmdBuffInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdBuffInfo.commandPool = m_VulkanCmdPool;
    cmdBuffInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBuffInfo.commandBufferCount = 1;

    // Allocate a command pool for Compute.
    if (m_VulkanComputeQueueIndx >= 0)
    {
        VkCommandPoolCreateInfo CmdPoolInfoStruct {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        CmdPoolInfoStruct.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // Must set this flag in order to reset a command buffer from this pool
        CmdPoolInfoStruct.queueFamilyIndex = m_VulkanComputeQueueIndx;

        RetVal = vkCreateCommandPool(m_VulkanDevice, &CmdPoolInfoStruct, nullptr, &m_VulkanComputeCmdPool);
        if (!CheckVkError("vkCreateCommandPool()", RetVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitQueryPools()
//-----------------------------------------------------------------------------
{
    if (m_ConfigOverride.NumTimerQueries.value_or(0) == 0)
    {
        // Did not ask for timer support, so the init is "true" so above keeps going
        return true;
    }

    VkResult RetVal = VK_SUCCESS;
    uint32_t NumQueries = m_ConfigOverride.NumTimerQueries.value_or(0);


    // ********************************
    // Create the query pool
    // ********************************
    LOGI("Creating Vulkan Query Pool...");

    // Need the number of query counts, but have not gotten the swapchain image count.
    // Was basing the number off the number of swapchains.
    // For now, overkill it!
    uint32_t StubSwapImageCount = 10;
    VkQueryPoolCreateInfo QueryInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    QueryInfo.flags = 0;
    QueryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    QueryInfo.queryCount = StubSwapImageCount * NumQueries; // Can't use m_SwapchainImageCount
    QueryInfo.pipelineStatistics = 0;

    RetVal = vkCreateQueryPool(m_VulkanDevice, &QueryInfo, nullptr, &m_VulkanQueryPool);
    if (!CheckVkError("vkCreateQueryPool()", RetVal))
    {
        return false;
    }

    // These all need to be reset before they can be used
    // Later: This function is crashing trying to access 0x00000000.  I have tried "vkResetQueryPool"
    // and "vkResetQueryPoolEXT". Both fail. I tried querying for the function pointer. Failed.
    // Just use the command version: vkCmdResetQueryPool
    // vkResetQueryPool(m_VulkanDevice, m_VulkanQueryPool, 0, QueryInfo.queryCount);

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitMemoryManager()
//-----------------------------------------------------------------------------
{
    return m_MemoryManager.Initialize(m_VulkanGpu, m_VulkanDevice, m_VulkanInstance, HasLoadedVulkanDeviceExtension("VK_KHR_buffer_device_address"));
}

//-----------------------------------------------------------------------------
bool Vulkan::InitPipelineCache()
//-----------------------------------------------------------------------------
{
    assert(m_PipelineCache == VK_NULL_HANDLE);
    VkPipelineCacheCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    auto RetVal = vkCreatePipelineCache( m_VulkanDevice, &CreateInfo, nullptr, &m_PipelineCache );
    if (!CheckVkError( "vkCreatePipelineCache()", RetVal ))
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
bool Vulkan::QuerySurfaceCapabilities(VkSurfaceCapabilitiesKHR& outVulkanSurfaceCaps)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    if (m_ExtSurfaceCapabilities2Available)
    {
        // Have the extension for vkGetPhysicalDeviceSurfaceCapabilities2KHR.
        // Ideally we could query the VkHdrMetadataEXT for this surface and use that as part of colormapping/output.
        // Doesnt seem to be supported for Qualcomm or Nvidia drivers ('undocumented' feature on AMD)
        VkPhysicalDeviceSurfaceInfo2KHR SurfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
        SurfaceInfo.surface = m_VulkanSurface;
        VkSurfaceCapabilities2KHR SurfaceCapabilities = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };

        RetVal = fpGetPhysicalDeviceSurfaceCapabilities2KHR(m_VulkanGpu, &SurfaceInfo, &SurfaceCapabilities);
        if (!CheckVkError("vkGetPhysicalDeviceSurfaceCapabilities2KHR()", RetVal))
        {
            return false;
        }

        outVulkanSurfaceCaps = SurfaceCapabilities.surfaceCapabilities;
    }
    else
    {
#if defined (OS_WINDOWS)
        RetVal = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(m_VulkanGpu, m_VulkanSurface, &outVulkanSurfaceCaps);
#elif defined (OS_ANDROID)
        RetVal = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_VulkanGpu, m_VulkanSurface, &outVulkanSurfaceCaps);
#endif // defined (OS_WINDOWS|OS_ANDROID)

        if (!CheckVkError("vkGetPhysicalDeviceSurfaceCapabilitiesKHR()", RetVal))
        {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::QuerySurfaceCapabilities()
//-----------------------------------------------------------------------------
{
    if (!QuerySurfaceCapabilities(m_VulkanSurfaceCaps))
        return false;

    LOGI("Vulkan Surface Capabilities:");
    LOGI("    minImageCount: %d", m_VulkanSurfaceCaps.minImageCount);
    LOGI("    maxImageCount: %d", m_VulkanSurfaceCaps.maxImageCount);
    LOGI("    currentExtent: %dx%d", m_VulkanSurfaceCaps.currentExtent.width, m_VulkanSurfaceCaps.currentExtent.height);
    LOGI("    minImageExtent: %dx%d", m_VulkanSurfaceCaps.minImageExtent.width, m_VulkanSurfaceCaps.minImageExtent.height);
    LOGI("    maxImageExtent: %dx%d", m_VulkanSurfaceCaps.maxImageExtent.width, m_VulkanSurfaceCaps.maxImageExtent.height);
    LOGI("    maxImageArrayLayers: %d", m_VulkanSurfaceCaps.maxImageArrayLayers);
    LOGI("    supportedTransforms: 0x%08x", m_VulkanSurfaceCaps.supportedTransforms);
    LOGI("    currentTransform: 0x%08x", m_VulkanSurfaceCaps.currentTransform);

    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR");
    if (m_VulkanSurfaceCaps.currentTransform & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR)
        LOGI("        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR");

    LOGI("    supportedCompositeAlpha: 0x%08x", m_VulkanSurfaceCaps.supportedCompositeAlpha);
    LOGI("    supportedUsageFlags: 0x%08x", m_VulkanSurfaceCaps.supportedUsageFlags);

    return true;
}


//-----------------------------------------------------------------------------
bool Vulkan::InitSwapChain()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // ********************************
    // Presentation Modes
    // ********************************
    uint32_t NumPresentModes;

#if defined (OS_WINDOWS)
    RetVal = fpGetPhysicalDeviceSurfacePresentModesKHR(m_VulkanGpu, m_VulkanSurface, &NumPresentModes, nullptr);
#elif defined (OS_ANDROID)
    RetVal = vkGetPhysicalDeviceSurfacePresentModesKHR(m_VulkanGpu, m_VulkanSurface, &NumPresentModes, nullptr);
#endif // defined (OS_WINDOWS|OS_ANDROID)

    if (!CheckVkError("vkGetPhysicalDeviceSurfacePresentModesKHR()", RetVal))
    {
        return false;
    }

    std::vector<VkPresentModeKHR> PresentModes;
    PresentModes.resize(NumPresentModes);

#if defined (OS_WINDOWS)
    RetVal = fpGetPhysicalDeviceSurfacePresentModesKHR(m_VulkanGpu, m_VulkanSurface, &NumPresentModes, PresentModes.data());
#elif defined (OS_ANDROID)
    RetVal = vkGetPhysicalDeviceSurfacePresentModesKHR(m_VulkanGpu, m_VulkanSurface, &NumPresentModes, PresentModes.data());
#endif // defined (OS_WINDOWS|OS_ANDROID)
    if (!CheckVkError("vkGetPhysicalDeviceSurfacePresentModesKHR()", RetVal))
    {
        return false;
    }

    LOGI("Supported Present Modes:");
    for (uint32_t WhichMode = 0; WhichMode < NumPresentModes; WhichMode++)
    {
        switch (PresentModes[WhichMode])
        {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            LOGI("    VK_PRESENT_MODE_IMMEDIATE_KHR");
            break;
        case VK_PRESENT_MODE_MAILBOX_KHR:
            LOGI("    VK_PRESENT_MODE_MAILBOX_KHR");
            break;
        case VK_PRESENT_MODE_FIFO_KHR:
            LOGI("    VK_PRESENT_MODE_FIFO_KHR");
            break;
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            LOGI("    VK_PRESENT_MODE_FIFO_RELAXED_KHR");
            break;
        default:
            LOGI("    Unknown! (%d)", PresentModes[WhichMode]);
            break;
        }
    }

    // FIFO: is the same as VSync mode.  It also makes the
    // swapchain images come back in order (0, 1, 2, 3, 0, 1, ...)
    // Mailbox: is similar to double buffering. One is displayed and one is queued.
    // The problem with mailbox is that the swapchain order that is returned is not
    // sequential if there are more than two images
    // Immediate: Requests are applied immediately and tearing may occur.
    // Unless there is a vsync, the GPU is rendering frames that are not displayed.
    // TODO: Need a way to allow application to set this
    VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;   // FIFO should always be available as a fall-back

    for (uint32_t uiIndx = 0; uiIndx < NumPresentModes; uiIndx++)
    {
        if (m_ConfigOverride.PresentMode.has_value() && m_ConfigOverride.PresentMode.value() == PresentModes[uiIndx])
        {
            // Found the app selected presentation mode!
            SwapchainPresentMode = m_ConfigOverride.PresentMode.value();
            break;
        }
        else if (PresentModes[uiIndx] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            // Mailbox is our 1st choice if we dont have (or can't find) an app selected override.
            SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
        else if ((SwapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (PresentModes[uiIndx] == VK_PRESENT_MODE_IMMEDIATE_KHR))
        {
            // Immediate is our 2nd choice if we dont have (or can't find) an app selected override.
            SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    switch (SwapchainPresentMode)
    {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        LOGI("Setting SwapChain Present Mode: VK_PRESENT_MODE_IMMEDIATE_KHR");
        break;
    case VK_PRESENT_MODE_MAILBOX_KHR:
        LOGI("Setting SwapChain Present Mode: VK_PRESENT_MODE_MAILBOX_KHR");
        break;
    case VK_PRESENT_MODE_FIFO_KHR:
        LOGI("Setting SwapChain Present Mode: VK_PRESENT_MODE_FIFO_KHR");
        break;
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        LOGI("Setting SwapChain Present Mode: VK_PRESENT_MODE_FIFO_RELAXED_KHR");
        break;
    default:
        LOGI("Setting SwapChain Present Mode: Unknown! (%d)", SwapchainPresentMode);
        break;
    }

    //
    // Swapchain Extent (grab this again as the swapchain rotate may have swapped the dimensions)
    //
    VkExtent2D SwapchainExtent;

    // If the surface capabilities comes back with current extent set to -1 it means
    // the size is undefined and we can set to what we want.
    // If it comes back as something it had better match what we set it to.
    if (m_VulkanSurfaceCaps.currentExtent.width == (uint32_t)-1 || m_VulkanSurfaceCaps.currentExtent.height == (uint32_t)-1 ||
        m_VulkanSurfaceCaps.currentExtent.width == 0 || m_VulkanSurfaceCaps.currentExtent.height == 0)
    {
        SwapchainExtent.width = m_SurfaceWidth;
        SwapchainExtent.height = m_SurfaceHeight;
        LOGI("Surface Caps returned an extent with at least one dimension invalid!  Setting to %dx%d", SwapchainExtent.width, SwapchainExtent.height);
    }
    else
    {
        SwapchainExtent = m_VulkanSurfaceCaps.currentExtent;
    }

    // Take the surface width from the Vulkan surface
    m_SurfaceWidth = SwapchainExtent.width;
    m_SurfaceHeight = SwapchainExtent.height;

    LOGI("Vulkan surface size: %dx%d", m_SurfaceWidth, m_SurfaceHeight);

    uint32_t DesiredSwapchainImages = NUM_VULKAN_BUFFERS;
    if (SwapchainPresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
        // In mailbox mode we request the number of buffers the application needs and Vulkan will want one or two more!  https://github.com/KhronosGroup/Vulkan-Docs/issues/909
        DesiredSwapchainImages = std::max((uint32_t)3, m_VulkanSurfaceCaps.minImageCount);
    }

    if (m_VulkanSurfaceCaps.minImageCount > DesiredSwapchainImages)
    {
        LOGE("****************************************");
        LOGE("Minimum image count (%d) is greater than NUM_VULKAN_BUFFERS (%d)!", m_VulkanSurfaceCaps.minImageCount, NUM_VULKAN_BUFFERS);
        LOGE("You will have all sorts of problems!");
        LOGE("****************************************");
        assert(0);
    }

    if ((m_VulkanSurfaceCaps.maxImageCount > 0) && (DesiredSwapchainImages > m_VulkanSurfaceCaps.maxImageCount))
    {
        LOGE("****************************************");
        LOGE("We desired %d swapchain images but surface limits us to %d!", DesiredSwapchainImages, m_VulkanSurfaceCaps.maxImageCount);
        LOGE("You will have all sorts of problems!");
        LOGE("****************************************");
        DesiredSwapchainImages = m_VulkanSurfaceCaps.maxImageCount;
    }

    // If we have the render pass transform extension (and want to use it), then make the pre-transform match the current transform and enable flag to setup renderpasses/commandbuffers appropriately.
    // Otherwise we want to leave the pretransform alone and let the hardware do the rotation, if supported.
    {
        if( m_ExtRenderPassTransformAvailable && m_LayerKhronosValidationAvailable )
        {
            LOGE( "Disabling QCOM_Render_Pass_Transform as it is not supported while Validation layers are enabled" );
            m_ExtRenderPassTransformAvailable = false;
        }

        VkSurfaceTransformFlagsKHR DesiredPreTransform;
        LOGI( "QCOM_Render_Pass_Transform - ExtRenderPassTransformAvailable=%s UseRenderPassTransform=%s", m_ExtRenderPassTransformAvailable ? "True" : "False", m_UseRenderPassTransform ? "True" : "False" );
        if (m_UseRenderPassTransform && m_ExtRenderPassTransformAvailable)
        {
            DesiredPreTransform = m_VulkanSurfaceCaps.currentTransform;
            m_ExtRenderPassTransformEnabled = (m_VulkanSurfaceCaps.currentTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) || (m_VulkanSurfaceCaps.currentTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR);
        }
        else if (m_UsePreTransform)
        {
            DesiredPreTransform = m_VulkanSurfaceCaps.currentTransform;
        }
        else
        {
            m_ExtRenderPassTransformEnabled = false;
            if (m_VulkanSurfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            {
                DesiredPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            }
            else
            {
                DesiredPreTransform = m_VulkanSurfaceCaps.currentTransform;
            }
        }
        if ((DesiredPreTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
            || (DesiredPreTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR))
        {
            LOGI("Vulkan surface extents swapped (width <-> height)");
            std::swap(SwapchainExtent.width, SwapchainExtent.height);
        }

        m_SwapchainPreTransform = (VkSurfaceTransformFlagBitsKHR)DesiredPreTransform;
    }

    // ********************************
    // Swapchain
    // ********************************
    VkSwapchainCreateInfoKHR SwapchainInfo {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    SwapchainInfo.flags = 0;
    SwapchainInfo.surface = m_VulkanSurface;
    SwapchainInfo.minImageCount = DesiredSwapchainImages;
    SwapchainInfo.imageFormat = m_SurfaceFormat;
    SwapchainInfo.imageColorSpace = m_SurfaceColorSpace;
    SwapchainInfo.imageExtent = SwapchainExtent;
    SwapchainInfo.imageArrayLayers = 1;

    // Image usage changes if we can blit it
    SwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkFormatProperties FormatProps;
    vkGetPhysicalDeviceFormatProperties(m_VulkanGpu, m_SurfaceFormat, &FormatProps);
    if ((FormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
    {
        SwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if ((FormatProps.optimalTilingFeatures & (VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR)))
    {
        SwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;    // screenshottable
    }

    SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapchainInfo.queueFamilyIndexCount = 0;
    SwapchainInfo.pQueueFamilyIndices = nullptr;
    SwapchainInfo.preTransform = m_SwapchainPreTransform;

    // Get the supported composite alpha flag
    SwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    SwapchainInfo.compositeAlpha = GetBestVulkanCompositeAlpha();

    SwapchainInfo.presentMode = SwapchainPresentMode;
    SwapchainInfo.clipped = true;
    SwapchainInfo.oldSwapchain = m_VulkanSwapchain;

    LOGI("Trying to get %d swapchain images...", DesiredSwapchainImages);
#if defined (OS_WINDOWS)
    RetVal = fpCreateSwapchainKHR(m_VulkanDevice, &SwapchainInfo, nullptr, &m_VulkanSwapchain);
#elif defined (OS_ANDROID)
    RetVal = vkCreateSwapchainKHR(m_VulkanDevice, &SwapchainInfo, nullptr, &m_VulkanSwapchain);
#endif // defined (OS_WINDOWS|OS_ANDROID)
    if (!CheckVkError("vkCreateSwapchainKHR()", RetVal))
    {
        return false;
    }

#if defined (OS_WINDOWS)
    RetVal = fpGetSwapchainImagesKHR(m_VulkanDevice, m_VulkanSwapchain, &m_SwapchainImageCount, nullptr);
#elif defined (OS_ANDROID)
    RetVal = vkGetSwapchainImagesKHR(m_VulkanDevice, m_VulkanSwapchain, &m_SwapchainImageCount, nullptr);
#endif // defined (OS_WINDOWS|OS_ANDROID)
    if (!CheckVkError("vkGetSwapchainImagesKHR(nullptr)", RetVal))
    {
        return false;
    }

    LOGI("SwapChain has %d images", m_SwapchainImageCount);
    if (m_SwapchainImageCount == 0)
    {
        LOGE("    Initialization Failed!  Could not get swapchain images");
        return false;
    }
    m_SwapchainCurrentIndx = 0;

    if ((m_VulkanSurfaceCaps.maxImageCount > 0) && (m_SwapchainImageCount > m_VulkanSurfaceCaps.maxImageCount))
    {
        LOGE("****************************************");
        LOGE("We asked for %d swapchain images but got back %d!", DesiredSwapchainImages, m_SwapchainImageCount);
        LOGE("****************************************");
        // m_SwapchainImageCount = m_VulkanSurfaceCaps.maxImageCount;
    }

    if (m_SwapchainImageCount > NUM_VULKAN_BUFFERS)
    {
        LOGE("****************************************");
        LOGE("vkGetSwapchainImagesKHR() returned %d swapchain images, but NUM_VULKAN_BUFFERS = %d!", m_SwapchainImageCount, NUM_VULKAN_BUFFERS);
        LOGE("Therefore, setting m_SwapchainImageCount to NUM_VULKAN_BUFFERS!");
        LOGE("****************************************");

        // Later: I can't just set m_SwapchainImageCount = NUM_VULKAN_BUFFERS!
        // I get error "Vulkan Error (VK_INCOMPLETE) from vkGetSwapchainImagesKHR()"
    }

    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(m_SwapchainImageCount);

#if defined (OS_WINDOWS)
    RetVal = fpGetSwapchainImagesKHR(m_VulkanDevice, m_VulkanSwapchain, &m_SwapchainImageCount, swapchainImages.data());
#elif defined (OS_ANDROID)
    RetVal = vkGetSwapchainImagesKHR(m_VulkanDevice, m_VulkanSwapchain, &m_SwapchainImageCount, swapchainImages.data());
#endif // defined (OS_WINDOWS|OS_ANDROID)
    if (!CheckVkError("vkGetSwapchainImagesKHR()", RetVal))
    {
        return false;
    }

    m_pSwapchainBuffers.resize(m_SwapchainImageCount, {});

    for (uint32_t uiIndx = 0; uiIndx < m_SwapchainImageCount; uiIndx++)
    {
        m_pSwapchainBuffers[uiIndx].image = swapchainImages[uiIndx];

        // Render loop will expect image to have been used before and in
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        // layout and will change to COLOR_ATTACHMENT_OPTIMAL, so init the image
        // to that state

        VkImageViewCreateInfo ImageViewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ImageViewInfo.flags = 0;
        ImageViewInfo.image = m_pSwapchainBuffers[uiIndx].image;
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewInfo.format = m_SurfaceFormat;
        ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageViewInfo.subresourceRange.baseMipLevel = 0;
        ImageViewInfo.subresourceRange.levelCount = 1;
        ImageViewInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewInfo.subresourceRange.layerCount = 1;

        RetVal = vkCreateImageView(m_VulkanDevice, &ImageViewInfo, nullptr, &m_pSwapchainBuffers[uiIndx].view);
        if (!CheckVkError("vkCreateImageView()", RetVal))
        {
            return false;
        }

        // Create the wait fences, one per swapchain image
        VkFenceCreateInfo FenceInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        RetVal = vkCreateFence(m_VulkanDevice, &FenceInfo, nullptr, &m_pSwapchainBuffers[uiIndx].fence);
        if (!CheckVkError("vkCreateFence()", RetVal))
        {
            return false;
        }

        // Create semaphores for backbuffer signalling, one per swapchain.
        VkSemaphoreCreateInfo semCreateInfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semCreateInfo.flags = 0;

        RetVal = vkCreateSemaphore(m_VulkanDevice, &semCreateInfo, nullptr, &m_pSwapchainBuffers[uiIndx].semaphore);
        if (!CheckVkError("vkCreateSemaphore()", RetVal))
        {
            return false;
        }

    }   // Each swapchain image


    // ********************************
    // Setup Depth
    // ********************************
    m_SwapchainDepth.format = m_ConfigOverride.SwapchainDepthFormat.value_or( GetBestVulkanDepthFormat() );

    if (m_SwapchainDepth.format != VK_FORMAT_UNDEFINED)
    {
        VkImageCreateInfo ImageInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ImageInfo.flags = 0;
        ImageInfo.imageType = VK_IMAGE_TYPE_2D;
        ImageInfo.format = m_SwapchainDepth.format;
        ImageInfo.extent.width = SwapchainExtent.width;
        ImageInfo.extent.height = SwapchainExtent.height;
        ImageInfo.extent.depth = 1; // Spec says for VK_IMAGE_TYPE_2D depth must be 1
        ImageInfo.mipLevels = 1;
        ImageInfo.arrayLayers = 1;
        ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        m_SwapchainDepth.image = m_MemoryManager.CreateImage( ImageInfo, MemoryManager::MemoryUsage::GpuExclusive );
        if( !m_SwapchainDepth.image )
        {
            LOGE( "Error creating swapchain back buffer depth image" );
            return false;
        }

        // Create the view for this image
        VkImageViewCreateInfo ImageViewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ImageViewInfo.flags = 0;
        ImageViewInfo.format = m_SwapchainDepth.format;
        ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        if (Vulkan::FormatHasStencil(m_SwapchainDepth.format))
            ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        else
            ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        ImageViewInfo.subresourceRange.baseMipLevel = 0;
        ImageViewInfo.subresourceRange.levelCount = 1;
        ImageViewInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewInfo.subresourceRange.layerCount = 1;
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewInfo.image = m_SwapchainDepth.image.GetVkBuffer();

        RetVal = vkCreateImageView(m_VulkanDevice, &ImageViewInfo, nullptr, &m_SwapchainDepth.view);
        if (!CheckVkError("vkCreateImageView()", RetVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitSwapchainRenderPass()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // The renderpass defines the attachments to the framebuffer object that gets
    // used in the pipelines. We have two attachments, the colour buffer, and the
    // depth buffer. The operations and layouts are set to defaults for this type
    // of attachment.
    uint32_t numAttachmentDescriptions = 0;
    std::array<VkAttachmentDescription, 2> attachmentDescriptions;
    attachmentDescriptions.fill( {} );
    attachmentDescriptions[0].flags                 = 0;
    attachmentDescriptions[0].format                = m_SurfaceFormat;
    attachmentDescriptions[0].samples               = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp                = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp               = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].stencilLoadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[0].stencilStoreOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[0].initialLayout         = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescriptions[0].finalLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    ++numAttachmentDescriptions;

    const bool HasDepth = m_SwapchainDepth.format != VK_FORMAT_UNDEFINED;

    if( HasDepth )
    {
        attachmentDescriptions[1].flags                 = 0;
        attachmentDescriptions[1].format                = m_SwapchainDepth.format;
        attachmentDescriptions[1].samples               = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescriptions[1].loadOp                = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescriptions[1].storeOp               = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[1].stencilLoadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[1].stencilStoreOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[1].initialLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentDescriptions[1].finalLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        ++numAttachmentDescriptions;
    }

    // We have references to the attachment offsets, stating the layout type.
    VkAttachmentReference colorReference = {};
    colorReference.attachment                       = 0;
    colorReference.layout                           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment                       = 1;
    depthReference.layout                           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // There can be multiple subpasses in a renderpass, but this example has only one.
    // We set the color and depth references at the grahics bind point in the pipeline.
    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint            = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.flags                        = 0;
    subpassDescription.inputAttachmentCount         = 0;
    subpassDescription.pInputAttachments            = nullptr;
    subpassDescription.colorAttachmentCount         = 1;
    subpassDescription.pColorAttachments            = &colorReference;
    subpassDescription.pResolveAttachments          = nullptr;
    subpassDescription.pDepthStencilAttachment      = HasDepth ? &depthReference : nullptr;
    subpassDescription.preserveAttachmentCount      = 0;
    subpassDescription.pPreserveAttachments         = nullptr;

    // Dependencies
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies.fill( {} );
    // dependencies[0] is an acquire dependency
    // dependencies[1] is a present dependency
    // We use subpass dependencies to define the color image layout transitions rather than
    // explicitly do them in the command buffer, as it is more efficient to do it this way.

    // Before we can use the back buffer from the swapchain, we must change the
    // image layout from the PRESENT mode to the COLOR_ATTACHMENT mode.
    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask   = 0;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // After writing to the back buffer of the swapchain, we need to change the
    // image layout from the COLOR_ATTACHMENT mode to the PRESENT mode which
    // is optimal for sending to the screen for users to see the completed rendering.
    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = 0;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // The renderpass itself is created with the number of subpasses, and the
    // list of attachments which those subpasses can reference.
    VkRenderPassCreateInfo renderPassCreateInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassCreateInfo.attachmentCount            = numAttachmentDescriptions;
    renderPassCreateInfo.pAttachments               = attachmentDescriptions.data();
    renderPassCreateInfo.subpassCount               = 1;
    renderPassCreateInfo.pSubpasses                 = &subpassDescription;
    renderPassCreateInfo.dependencyCount            = (uint32_t) dependencies.size();
    renderPassCreateInfo.pDependencies              = dependencies.data();
    renderPassCreateInfo.flags                      = (m_ExtRenderPassTransformEnabled) ? VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM : 0;

    RetVal = vkCreateRenderPass(m_VulkanDevice, &renderPassCreateInfo, nullptr, &m_SwapchainRenderPass);
    if (!CheckVkError("vkCreateRenderPass()", RetVal))
    {
        return false;
    }


    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::Create2SubpassRenderPass(const tcb::span<const VkFormat> InternalColorFormats, const tcb::span<const VkFormat> OutputColorFormats, VkFormat InternalDepthFormat, const tcb::span<const VkSampleCountFlagBits> InternalMsaa/*two passes*/, VkSampleCountFlagBits OutputMsaa, VkRenderPass* pRenderPass/*out*/)
//-----------------------------------------------------------------------------
{
    assert(pRenderPass && *pRenderPass == VK_NULL_HANDLE);  // check not already allocated and that we have a location to place the renderpass handle
    assert(!InternalColorFormats.empty());                // not supporting a depth only pass
    assert(!OutputColorFormats.empty());
    assert( InternalMsaa.size() == 2 );

    //const bool NeedsResolve = InternalMsaa != OutputMsaa;

    // Color attachments and Depth attachment
    std::vector<VkAttachmentDescription> PassAttachDescs;
    PassAttachDescs.reserve(InternalColorFormats.size() + OutputColorFormats.size() + 2);

    // Each subpass needs a reference to its attachments
    std::array<VkSubpassDescription, 2> SubpassDesc{
        VkSubpassDescription {0/*flags*/, VK_PIPELINE_BIND_POINT_GRAPHICS},
        VkSubpassDescription {0/*flags*/, VK_PIPELINE_BIND_POINT_GRAPHICS} };
    std::vector<VkAttachmentReference> ColorReferencesPass0;
    std::vector<VkAttachmentReference> ResolveReferencesPass0;
    ColorReferencesPass0.reserve(InternalColorFormats.size());
    ResolveReferencesPass0.reserve(InternalColorFormats.size());

    std::vector<VkAttachmentReference> InputReferencesPass1;
    std::vector<VkAttachmentReference> ColorReferencesPass1;
    std::vector<VkAttachmentReference> ResolveReferencesPass1;
    InputReferencesPass1.reserve(InternalColorFormats.size());
    ColorReferencesPass1.reserve(OutputColorFormats.size());
    ResolveReferencesPass1.reserve(OutputColorFormats.size());

    const bool HasDepth = InternalDepthFormat != VK_FORMAT_UNDEFINED;

    bool Pass0NeedsResolve = InternalMsaa[1] != InternalMsaa[0];
    bool Pass1NeedsResolve = OutputMsaa != InternalMsaa[1];

    //
    // First Pass Color Attachments (what is written by the first pass)
    // Also setup as inputs to the second pass.
    for (const auto& ColorFormat : InternalColorFormats)
    {
        // Pass0 color and depth buffers setup to clear on load, discard on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass0 = { 0/*flags*/,
            ColorFormat/*format*/,
            InternalMsaa[0]/*samples*/,
            VK_ATTACHMENT_LOAD_OP_CLEAR/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL/*finalLayout*/ };
        PassAttachDescs.push_back(AttachmentDescPass0);

        ColorReferencesPass0.push_back({ (uint32_t)PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL/*output of first pass*/ });
        InputReferencesPass1.push_back({ (uint32_t)PassAttachDescs.size() - 1, Pass0NeedsResolve ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*input of second pass*/ });
    }

    if (Pass0NeedsResolve)
    {
        //
        // First pass Resolve Attachments (intermediate)
        for (const auto& ColorFormat : InternalColorFormats)
        {
            // Pass1 color buffers to resolve to at end of pass.
            VkAttachmentDescription AttachmentDescResolvePass0 = { 0, ColorFormat/*format*/,
                OutputMsaa/*samples*/,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
                VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
                VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*initialLayout*/,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/ };

            PassAttachDescs.push_back( AttachmentDescResolvePass0 );
            ResolveReferencesPass0.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } );
        }
        //
        // Setup the resolve (first subpass)
        SubpassDesc[0].pResolveAttachments = ResolveReferencesPass0.data();
    }

    //
    // Second Pass Color Attachments (these are the outputs from the second pass)
    for (const auto& ColorFormat : OutputColorFormats)
    {
        // Pass1 color buffers setup to, store on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass1 = { 0, ColorFormat/*format*/,
            (!Pass1NeedsResolve) ? OutputMsaa : InternalMsaa[1]/*samples*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
            (!Pass1NeedsResolve) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/ };
        PassAttachDescs.push_back(AttachmentDescPass1);

        ColorReferencesPass1.push_back({ (uint32_t)PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    }

    if (Pass1NeedsResolve)
    {
        //
        // Second pass Resolve Attachments (output)
        for (const auto& ColorFormat : OutputColorFormats)
        {
            // Pass1 color buffers to resolve to at end of pass.
            VkAttachmentDescription AttachmentDescResolvePass1 = { 0, ColorFormat/*format*/,
                OutputMsaa/*samples*/,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
                VK_ATTACHMENT_STORE_OP_STORE/*storeOp*/,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
                VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
                VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/ };

            PassAttachDescs.push_back( AttachmentDescResolvePass1 );
            ResolveReferencesPass1.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } );
        }
        //
        // Setup the resolve (second subpass)
        SubpassDesc[1].pResolveAttachments = ResolveReferencesPass1.data();
    }


    //
    // Depth Attachment (cleared at start of pass, written by first subpass, discarded after pass)
    VkAttachmentReference DepthReference = {};
    VkAttachmentReference DepthReferencePass1 = {};
    if (HasDepth)
    {
        VkAttachmentDescription AttachmentDescDepthPass0 = { 0/*flags*/,
            InternalDepthFormat/*format*/,
            InternalMsaa[0]/*samples*/,
            VK_ATTACHMENT_LOAD_OP_CLEAR/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL/*finalLayout*/ };

        PassAttachDescs.push_back(AttachmentDescDepthPass0);
        DepthReference = { (uint32_t)PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    }

    //
    // Subpass dependencies
    std::array<VkSubpassDependency, 1> PassDependencies = {};
    if (ColorReferencesPass0.size() > 0)
    {
        SubpassDesc[0].colorAttachmentCount = (uint32_t)ColorReferencesPass0.size();
        SubpassDesc[0].pColorAttachments = ColorReferencesPass0.data();
        SubpassDesc[1].inputAttachmentCount = (uint32_t)InputReferencesPass1.size();
        SubpassDesc[1].pInputAttachments = InputReferencesPass1.data();
    }
    if (ColorReferencesPass1.size() > 0)
    {
        SubpassDesc[1].colorAttachmentCount = (uint32_t)ColorReferencesPass1.size();
        SubpassDesc[1].pColorAttachments = ColorReferencesPass1.data();
    }

    // Only first pass writes the Depth
    if (HasDepth)
    {
        SubpassDesc[0].pDepthStencilAttachment = &DepthReference;
    }

    // Color subpass (takes output of first subpass as input to fragment shader)
    PassDependencies[0].srcSubpass = 0;
    PassDependencies[0].dstSubpass = 1;
    PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    PassDependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[0].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Now ready to actually create the render pass
    VkRenderPassCreateInfo RenderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    RenderPassInfo.flags = 0;
    RenderPassInfo.attachmentCount = (uint32_t)PassAttachDescs.size();
    RenderPassInfo.pAttachments = PassAttachDescs.data();
    RenderPassInfo.subpassCount = (uint32_t)SubpassDesc.size();
    RenderPassInfo.pSubpasses = SubpassDesc.data();
    RenderPassInfo.dependencyCount = (uint32_t)PassDependencies.size();
    RenderPassInfo.pDependencies = PassDependencies.data();

    VkResult RetVal = vkCreateRenderPass(m_VulkanDevice, &RenderPassInfo, NULL, pRenderPass);
    if (!CheckVkError("vkCreateRenderPass()", RetVal))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::Create2SubpassRenderPass( const tcb::span<const VkFormat> InternalColorFormats,
    const tcb::span<const VkFormat> OutputColorFormats,
    VkFormat InternalDepthFormat,
    VkSampleCountFlagBits InternalPassMsaa, /* same for both passes*/
    VkSampleCountFlagBits OutputMsaa,
    VkRenderPass* pRenderPass/*out*/ )
//-----------------------------------------------------------------------------
{
    const VkSampleCountFlagBits InternalPassMsaa2[2] = { InternalPassMsaa, InternalPassMsaa };
    return Create2SubpassRenderPass( InternalColorFormats , OutputColorFormats , InternalDepthFormat, InternalPassMsaa2, OutputMsaa, pRenderPass );
}

//-----------------------------------------------------------------------------
bool Vulkan::CreateSubpassShaderResolveRenderPass( const tcb::span<const VkFormat> InternalColorFormats, const tcb::span<const VkFormat> OutputColorFormats, VkFormat InternalDepthFormat, VkSampleCountFlagBits InternalMsaa, VkSampleCountFlagBits OutputMsaa, VkRenderPass* pRenderPass/*out*/ )
//-----------------------------------------------------------------------------
{
    assert( pRenderPass && *pRenderPass == VK_NULL_HANDLE );  // check not already allocated and that we have a location to place the renderpass handle
    assert( !InternalColorFormats.empty() );                // not supporting a depth only pass
    assert( !OutputColorFormats.empty() );

    if (InternalMsaa == OutputMsaa)
        // no resolve needed - return 'error'
        return false;
    if (!GetExtRenderPassShaderResolveAvailable())
        // hardware/driver cannot do shader resolve - return 'error'
        return false;

    // Color attachments and Depth attachment
    std::vector<VkAttachmentDescription> PassAttachDescs;
    PassAttachDescs.reserve( InternalColorFormats.size() + OutputColorFormats.size() + 2 );

    // Each subpass needs a reference to its attachments
    std::array<VkSubpassDescription, 2> SubpassDesc {
        VkSubpassDescription {0/*flags*/, VK_PIPELINE_BIND_POINT_GRAPHICS},
        VkSubpassDescription {0/*flags*/, VK_PIPELINE_BIND_POINT_GRAPHICS} };
    std::vector<VkAttachmentReference> ColorReferencesPass0;
    ColorReferencesPass0.reserve( InternalColorFormats.size() );

    std::vector<VkAttachmentReference> InputReferencesPass1;
    std::vector<VkAttachmentReference> ColorReferencesPass1;
    InputReferencesPass1.reserve( InternalColorFormats.size() );
    ColorReferencesPass1.reserve( OutputColorFormats.size() );

    const bool HasDepth = InternalDepthFormat != VK_FORMAT_UNDEFINED;
    const bool HasPass2ReadDepth = HasDepth && false;

    //
    // First Pass Color Attachments (what is written by the first pass)
    // Also setup as inputs to the second pass.
    for (const auto& ColorFormat : InternalColorFormats)
    {
        // Pass0 color and depth buffers setup to clear on load, discard on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass0 = { 0/*flags*/,
            ColorFormat/*format*/,
            InternalMsaa/*samples*/,
            VK_ATTACHMENT_LOAD_OP_CLEAR/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL/*finalLayout*/ };
        PassAttachDescs.push_back( AttachmentDescPass0 );

        ColorReferencesPass0.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL/*output of first pass*/ } );
        InputReferencesPass1.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*input of second pass*/ } );
    }

    //
    // Second Pass Color Attachments (these are the outputs from the second pass)
    for (const auto& ColorFormat : OutputColorFormats)
    {
        // Pass1 color buffers setup to, store on end (of entire pass).
        VkAttachmentDescription AttachmentDescPass1 = { 0, ColorFormat/*format*/,
            OutputMsaa/*samples*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/ };
        PassAttachDescs.push_back( AttachmentDescPass1 );

        ColorReferencesPass1.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } );
    }

    //
    // Depth Attachment (cleared at start of pass, written by first subpass, discarded after pass)
    VkAttachmentReference DepthReference = {};
    VkAttachmentReference DepthReferencePass1 = {};
    if (HasDepth)
    {
        VkAttachmentDescription AttachmentDescDepthPass0 = { 0/*flags*/,
            InternalDepthFormat/*format*/,
            InternalMsaa/*samples*/,
            VK_ATTACHMENT_LOAD_OP_CLEAR/*loadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*storeOp*/,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
            VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
            VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
            HasPass2ReadDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL/*finalLayout*/ };

        PassAttachDescs.push_back( AttachmentDescDepthPass0 );
        DepthReference = { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        if (HasPass2ReadDepth)
            InputReferencesPass1.push_back( { (uint32_t) PassAttachDescs.size() - 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL/*input of second pass*/ } );
    }

    // Shader Resolve extension (VK_QCOM_render_pass_shader_resolve) is available, use it. 
    assert( OutputMsaa == VK_SAMPLE_COUNT_1_BIT );
    SubpassDesc[1].flags = 0x00000008/*VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM*/ | 0x00000004/*VK_SUBPASS_DESCRIPTION_FRAGMENT_REGION_BIT_QCOM*/;

    //
    // Subpass dependencies
    std::array<VkSubpassDependency, 1> PassDependencies = {};
    if (ColorReferencesPass0.size() > 0)
    {
        SubpassDesc[0].colorAttachmentCount = (uint32_t) ColorReferencesPass0.size();
        SubpassDesc[0].pColorAttachments = ColorReferencesPass0.data();
        SubpassDesc[1].inputAttachmentCount = (uint32_t) InputReferencesPass1.size();
        SubpassDesc[1].pInputAttachments = InputReferencesPass1.data();
    }
    if (ColorReferencesPass1.size() > 0)
    {
        SubpassDesc[1].colorAttachmentCount = (uint32_t) ColorReferencesPass1.size();
        SubpassDesc[1].pColorAttachments = ColorReferencesPass1.data();
    }

    // Only first pass writes the Depth
    if (HasDepth)
    {
        SubpassDesc[0].pDepthStencilAttachment = &DepthReference;
    }

    // Color subpass (takes output of first subpass as input to fragment shader)
    PassDependencies[0].srcSubpass = 0;
    PassDependencies[0].dstSubpass = 1;
    PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    PassDependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[0].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Now ready to actually create the render pass
    VkRenderPassCreateInfo RenderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    RenderPassInfo.flags = 0;
    RenderPassInfo.attachmentCount = (uint32_t) PassAttachDescs.size();
    RenderPassInfo.pAttachments = PassAttachDescs.data();
    RenderPassInfo.subpassCount = (uint32_t) SubpassDesc.size();
    RenderPassInfo.pSubpasses = SubpassDesc.data();
    RenderPassInfo.dependencyCount = (uint32_t) PassDependencies.size();
    RenderPassInfo.pDependencies = PassDependencies.data();

    VkResult RetVal = vkCreateRenderPass( m_VulkanDevice, &RenderPassInfo, NULL, pRenderPass );
    if (!CheckVkError( "vkCreateRenderPass()", RetVal ))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::InitFrameBuffers()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    //The framebuffer objects reference the renderpass, and allow
    // the references defined in that renderpass to now attach to views.
    // The views in this example are the colour view, which is our swapchain image,
    // and (optionally) the depth buffer created manually earlier.
    std::array<VkImageView, 2> attachments;
    attachments.fill( {} );
    uint32_t numAttachments = 1;
    if( m_SwapchainDepth.format != VK_FORMAT_UNDEFINED )
    {
        attachments[1] = m_SwapchainDepth.view;
        ++numAttachments;
    }

    VkFramebufferCreateInfo framebufferCreateInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferCreateInfo.renderPass = m_SwapchainRenderPass;
    framebufferCreateInfo.attachmentCount = numAttachments;
    framebufferCreateInfo.pAttachments = attachments.data();
    framebufferCreateInfo.width = m_SurfaceWidth;
    framebufferCreateInfo.height = m_SurfaceHeight;
    if ((m_SwapchainPreTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) || (m_SwapchainPreTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR))
    {
        std::swap(framebufferCreateInfo.width, framebufferCreateInfo.height);
    }
    framebufferCreateInfo.layers = 1;

    LOGI("Creating %d frame buffers... (%d x %d)", m_SwapchainImageCount, framebufferCreateInfo.width, framebufferCreateInfo.height);

    assert(m_pSwapchainFrameBuffers.empty());
    m_pSwapchainFrameBuffers.resize(m_SwapchainImageCount, VK_NULL_HANDLE);

    // Reusing the framebufferCreateInfo to create mSwapchainImageCount framebuffers,
    // only the attachment to the relevent image view changes each time.
    for (uint32_t WhichImage = 0; WhichImage < m_SwapchainImageCount; WhichImage++)
    {
        attachments[0] = m_pSwapchainBuffers[WhichImage].view;

        RetVal = vkCreateFramebuffer(m_VulkanDevice, &framebufferCreateInfo, nullptr, &m_pSwapchainFrameBuffers[WhichImage]);
        if (!CheckVkError("vkCreateFramebuffer()", RetVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
void Vulkan::DestroySwapChain()
//-----------------------------------------------------------------------------
{
    if( m_SwapchainDepth.format != VK_FORMAT_UNDEFINED )
    {
        m_MemoryManager.Destroy( std::move( m_SwapchainDepth.image ) );
        m_SwapchainDepth.format = VK_FORMAT_UNDEFINED;
        vkDestroyImageView( m_VulkanDevice, m_SwapchainDepth.view, nullptr );
        m_SwapchainDepth.view = VK_NULL_HANDLE;
    }

    for (auto& swapchainBuffer : m_pSwapchainBuffers)
    {
        vkDestroySemaphore(m_VulkanDevice, swapchainBuffer.semaphore, nullptr);
        vkDestroyFence(m_VulkanDevice, swapchainBuffer.fence, nullptr);
        vkDestroyImageView(m_VulkanDevice, swapchainBuffer.view, nullptr);
        swapchainBuffer.image = VK_NULL_HANDLE; // images are owned by the m_VulkanSwapchain
    }
    m_pSwapchainBuffers.clear();
    
#if defined (OS_WINDOWS)
    fpDestroySwapchainKHR(m_VulkanDevice, m_VulkanSwapchain, nullptr);
#elif defined (OS_ANDROID)
    vkDestroySwapchainKHR(m_VulkanDevice, m_VulkanSwapchain, nullptr);
#endif // defined (OS_WINDOWS|OS_ANDROID)
    m_VulkanSwapchain = VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
void Vulkan::DestroySwapchainRenderPass()
//-----------------------------------------------------------------------------
{
    if (m_SwapchainRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_VulkanDevice, m_SwapchainRenderPass, nullptr);
        m_SwapchainRenderPass = VK_NULL_HANDLE;
    }
}

//-----------------------------------------------------------------------------
void Vulkan::DestroyFrameBuffers()
//-----------------------------------------------------------------------------
{
    for (auto& framebuffer : m_pSwapchainFrameBuffers)
    {
        vkDestroyFramebuffer(m_VulkanDevice, framebuffer, nullptr);
    }
    m_pSwapchainFrameBuffers.clear();
}

//-----------------------------------------------------------------------------
bool Vulkan::CreateRenderPass(tcb::span<const VkFormat> ColorFormats, VkFormat DepthFormat, VkSampleCountFlagBits Msaa, RenderPassInputUsage ColorInputUsage, RenderPassOutputUsage ColorOutputUsage, bool ShouldClearDepth, RenderPassOutputUsage DepthOutputUsage, VkRenderPass* pRenderPass/*out*/, tcb::span<const VkFormat> ResolveFormats)
//-----------------------------------------------------------------------------
{
    assert(pRenderPass && *pRenderPass == VK_NULL_HANDLE);  // check not already allocated and that we have a location to place the renderpass handle
    VkResult RetVal = VK_SUCCESS;

    // Color attachments and Depth attachment
    std::vector<VkAttachmentDescription> PassAttachDescs;
    PassAttachDescs.reserve(ColorFormats.size()+1);

    bool bPresentPass = false;
    bool bHasDepth = DepthFormat != VK_FORMAT_UNDEFINED;

    // Color Attachment
    for(const VkFormat ColorFormat: ColorFormats)
    {
        assert(ColorFormat != VK_FORMAT_UNDEFINED);

        PassAttachDescs.push_back({
            /*flags*/ 0,
            /*format*/ ColorFormat,
            /*samples*/ Msaa,
            /*loadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*storeOp*/ VK_ATTACHMENT_STORE_OP_STORE,
            /*stencilLoadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*stencilStoreOp*/ VK_ATTACHMENT_STORE_OP_DONT_CARE,
            /*initialLayout*/ VK_IMAGE_LAYOUT_UNDEFINED,
            /*finalLayout*/ VK_IMAGE_LAYOUT_UNDEFINED
            });
        auto& PassAttachDesc = PassAttachDescs.back();

        switch( ColorInputUsage ) {
        case RenderPassInputUsage::Clear:
            PassAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            PassAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case RenderPassInputUsage::Load:
            PassAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            PassAttachDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassInputUsage::DontCare:
            PassAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            PassAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        };

        switch (ColorOutputUsage) {
        case RenderPassOutputUsage::Discard:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // not sure why Vulkan spec says this can't be VK_IMAGE_LAYOUT_UNDEFINED - we aren't storing it afterall!
            break;
        case RenderPassOutputUsage::Store:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreReadOnly:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreTransferSrc:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case RenderPassOutputUsage::Clear:
            assert(0); // currently unsupported
            break;
        case RenderPassOutputUsage::Present:
            if (ResolveFormats.empty())
            {
                // Nothing to resolve - this is the output to present
                PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            else
            {
                // Dont care what happens to this pass once the resolve has happened
                PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            bPresentPass = true;
            break;
        }
    }

    // Depth Attachment
    if (bHasDepth)
    {
        PassAttachDescs.push_back({
            /*flags*/ 0,
            /*format*/ DepthFormat,
            /*samples*/ Msaa,
            /*loadOp*/ ShouldClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            /*storeOp*/ VK_ATTACHMENT_STORE_OP_STORE,
            /*stencilLoadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*stencilStoreOp*/ VK_ATTACHMENT_STORE_OP_DONT_CARE,
            /*initialLayout*/ ShouldClearDepth ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            /*finalLayout*/ VK_IMAGE_LAYOUT_UNDEFINED
            });
        auto& PassAttachDesc = PassAttachDescs.back();
        switch (DepthOutputUsage) {
        case RenderPassOutputUsage::Discard:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // not sure why Vulkan spec says this can't be VK_IMAGE_LAYOUT_UNDEFINED - we aren't storing it afterall!
            break;
        case RenderPassOutputUsage::Store:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreReadOnly:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreTransferSrc:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case RenderPassOutputUsage::Clear:
        case RenderPassOutputUsage::Present:
            assert(0); // currently unsupported
        }
    }

    // We need a reference to the attachments
    std::vector<VkAttachmentReference> ColorReferences;
    ColorReferences.reserve(ColorFormats.size());
    uint32_t AttachmentIndex = 0;
    for(; AttachmentIndex < ColorFormats.size(); ++AttachmentIndex)
    {
        ColorReferences.push_back({ AttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    }
    VkAttachmentReference DepthReference = {AttachmentIndex++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    // We only have one subpass
    VkSubpassDescription SubpassDesc = {};
    SubpassDesc.flags = 0;
    SubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDesc.inputAttachmentCount = 0;
    SubpassDesc.pInputAttachments = nullptr;

    if (ColorReferences.size() > 0)
    {
        SubpassDesc.colorAttachmentCount = (uint32_t)ColorReferences.size();
        SubpassDesc.pColorAttachments = ColorReferences.data();
    }

    // When we have ResolveFormats we resolve each member of ColorFormats out to a matching ResolveFormats buffer
    std::vector<VkAttachmentReference> ResolveReferences;
    if( !ResolveFormats.empty() )
    {
        ResolveReferences.resize( ColorFormats.size(), {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED} ); // must match the number of Color buffers (even if they are not resolving)
        uint32_t ColorIdx = 0;
        for( const VkFormat ResolveFormat : ResolveFormats )
        {
            if( ResolveFormat != VK_FORMAT_UNDEFINED )
            {
                ResolveReferences[ColorIdx] = { (uint32_t) PassAttachDescs.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

                // Pass1 color buffers to resolve to at end of pass.
                VkAttachmentDescription AttachmentDescResolvePass1 = { 0, ResolveFormat/*format*/,
                    VK_SAMPLE_COUNT_1_BIT/*samples*/,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
                    VK_ATTACHMENT_STORE_OP_STORE/*storeOp*/,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/, 
                    VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
                    VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/, 
                    bPresentPass ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/};
                PassAttachDescs.push_back( AttachmentDescResolvePass1 );
            }
            ++ColorIdx;
        }
        SubpassDesc.pResolveAttachments = ResolveReferences.data();
    }
    if (bHasDepth)
    {
        SubpassDesc.pDepthStencilAttachment = &DepthReference;
    }
    SubpassDesc.preserveAttachmentCount = 0;
    SubpassDesc.pPreserveAttachments = nullptr;

    // Subpass dependencies
    std::array<VkSubpassDependency, 2> PassDependencies = {};

    if (ColorFormats.empty())
    {
        // Depth only pass
        PassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[0].dstSubpass = 0;
        PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        PassDependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        PassDependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        PassDependencies[1].srcSubpass = 0;
        PassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        PassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        PassDependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        PassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        PassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
    else
    {
        // Color (and maybe depth) pass
        PassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[0].dstSubpass = 0;
        PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        PassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        PassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        PassDependencies[1].srcSubpass = 0;
        PassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        PassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        PassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        PassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        PassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }


    // We use subpass dependencies to define the color image layout transitions rather than
// explicitly do them in the command buffer, as it is more efficient to do it this way.
#if 0
// Before we can use the back buffer from the swapchain, we must change the
// image layout from the PRESENT mode to the COLOR_ATTACHMENT mode.
    PassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    PassDependencies[0].dstSubpass = 0;
    PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[0].srcAccessMask = 0;
    PassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // After writing to the back buffer of the swapchain, we need to change the
    // image layout from the COLOR_ATTACHMENT mode to the PRESENT mode which
    // is optimal for sending to the screen for users to see the completed rendering.
    PassDependencies[1].srcSubpass = 0;
    PassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    PassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    PassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[1].dstAccessMask = 0;
    PassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
#endif

    // Now ready to actually create the render pass
    VkRenderPassCreateInfo RenderPassInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.flags = (bPresentPass && m_ExtRenderPassTransformEnabled) ? VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM : 0;
    RenderPassInfo.attachmentCount = (uint32_t) PassAttachDescs.size();
    RenderPassInfo.pAttachments = PassAttachDescs.data();
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &SubpassDesc;
    RenderPassInfo.dependencyCount = (uint32_t) PassDependencies.size();
    RenderPassInfo.pDependencies = PassDependencies.data();

    RetVal = vkCreateRenderPass(m_VulkanDevice, &RenderPassInfo, nullptr, pRenderPass);
    if (!CheckVkError("vkCreateRenderPass()", RetVal))
    {
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
bool Vulkan::CreateRenderPassVRS(tcb::span<const VkFormat> ColorFormats, VkFormat DepthFormat, VkSampleCountFlagBits Msaa,
    RenderPassInputUsage ColorInputUsage, RenderPassOutputUsage ColorOutputUsage, bool ShouldClearDepth,
    RenderPassOutputUsage DepthOutputUsage, VkRenderPass* pRenderPass/*out*/, tcb::span<const VkFormat> ResolveFormats, bool HasDensityMap)
    //-----------------------------------------------------------------------------
{
    assert(pRenderPass && *pRenderPass == VK_NULL_HANDLE);  // check not already allocated and that we have a location to place the renderpass handle
    VkResult RetVal = VK_SUCCESS;

    std::vector<VkAttachmentDescription2KHR> PassAttachDescs;
    PassAttachDescs.reserve(ColorFormats.size());

    bool bPresentPass = false;
    bool bHasDepth = DepthFormat != VK_FORMAT_UNDEFINED;

    // Color Attachment
    for (const VkFormat ColorFormat : ColorFormats)
    {
        assert(ColorFormat != VK_FORMAT_UNDEFINED);

        PassAttachDescs.push_back({
            /*stype*/ VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR,
            /*pNext*/ nullptr,
            /*flags*/ 0,
            /*format*/ ColorFormat,
            /*samples*/ Msaa,
            /*loadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*storeOp*/ VK_ATTACHMENT_STORE_OP_STORE,
            /*stencilLoadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*stencilStoreOp*/ VK_ATTACHMENT_STORE_OP_DONT_CARE,
            /*initialLayout*/ VK_IMAGE_LAYOUT_UNDEFINED,
            /*finalLayout*/ VK_IMAGE_LAYOUT_UNDEFINED
            });
        auto& PassAttachDesc = PassAttachDescs.back();

        switch (ColorInputUsage) {
        case RenderPassInputUsage::Clear:
            PassAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            PassAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case RenderPassInputUsage::Load:
            PassAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            PassAttachDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassInputUsage::DontCare:
            PassAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            PassAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        };

        switch (ColorOutputUsage) {
        case RenderPassOutputUsage::Discard:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // not sure why Vulkan spec says this can't be VK_IMAGE_LAYOUT_UNDEFINED - we aren't storing it afterall!
            break;
        case RenderPassOutputUsage::Store:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreReadOnly:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreTransferSrc:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case RenderPassOutputUsage::Clear:
            assert(0); // currently unsupported
            break;
        case RenderPassOutputUsage::Present:
            if (ResolveFormats.empty())
            {
                // Nothing to resolve - this is the output to present
                PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            else
            {
                // Dont care what happens to this pass once the resolve has happened
                PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            bPresentPass = true;
            break;
        }
    }

    // Depth Attachment
    if (bHasDepth)
    {
        PassAttachDescs.push_back({
            /*sType*/ VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR,
            /*pNext*/ nullptr,
            /*flags*/ 0,
            /*format*/ DepthFormat,
            /*samples*/ Msaa,
            /*loadOp*/ ShouldClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            /*storeOp*/ VK_ATTACHMENT_STORE_OP_STORE,
            /*stencilLoadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*stencilStoreOp*/ VK_ATTACHMENT_STORE_OP_DONT_CARE,
            /*initialLayout*/ ShouldClearDepth ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            /*finalLayout*/ VK_IMAGE_LAYOUT_UNDEFINED
            });
        auto& PassAttachDesc = PassAttachDescs.back();
        switch (DepthOutputUsage) {
        case RenderPassOutputUsage::Discard:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // not sure why Vulkan spec says this can't be VK_IMAGE_LAYOUT_UNDEFINED - we aren't storing it afterall!
            break;
        case RenderPassOutputUsage::Store:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreReadOnly:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            break;
        case RenderPassOutputUsage::StoreTransferSrc:
            PassAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            PassAttachDesc.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case RenderPassOutputUsage::Clear:
        case RenderPassOutputUsage::Present:
            assert(0); // currently unsupported
        }
    }

    PassAttachDescs.push_back({
        /*sType*/ VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR,
        /*pNext*/ nullptr,
        /*flags*/ 0,
        /*format*/ VK_FORMAT_R8_UINT,
        /*samples*/ VK_SAMPLE_COUNT_1_BIT,
        /*loadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        /*storeOp*/ VK_ATTACHMENT_STORE_OP_STORE,
        /*stencilLoadOp*/ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        /*stencilStoreOp*/ VK_ATTACHMENT_STORE_OP_DONT_CARE,
        /*initialLayout*/ VK_IMAGE_LAYOUT_GENERAL,// VK_IMAGE_LAYOUT_GENERAL ?
        /*finalLayout*/ VK_IMAGE_LAYOUT_GENERAL // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ?
        });

    // We need a reference to the attachments
    std::vector<VkAttachmentReference2KHR> ColorReferences;
    ColorReferences.reserve(ColorFormats.size());
    uint32_t AttachmentIndex = 0;
    for (; AttachmentIndex < ColorFormats.size(); ++AttachmentIndex)
    {
        ColorReferences.push_back({ VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, nullptr,  AttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    }

    VkAttachmentReference2KHR DepthReference;
    if (bHasDepth) {
        DepthReference = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, nullptr, AttachmentIndex++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    }

    VkAttachmentReference2KHR VRS_AttachRef = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, nullptr, AttachmentIndex++, VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR };

    // We only have one subpass
    VkSubpassDescription2KHR SubpassDesc = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR , nullptr };
    SubpassDesc.flags = 0;
    SubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDesc.inputAttachmentCount = 0;
    SubpassDesc.pInputAttachments = nullptr;

    if (ColorReferences.size() > 0)
    {
        SubpassDesc.colorAttachmentCount = (uint32_t)ColorReferences.size();
        SubpassDesc.pColorAttachments = ColorReferences.data();
    }

    // When we have ResolveFormats we resolve each member of ColorFormats out to a matching ResolveFormats buffer
    std::vector<VkAttachmentReference2KHR> ResolveReferences;
    if (!ResolveFormats.empty())
    {
        ResolveReferences.resize(ColorFormats.size(), { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, nullptr, VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED }); // must match the number of Color buffers (even if they are not resolving)
        uint32_t ColorIdx = 0;
        for (const VkFormat ResolveFormat : ResolveFormats)
        {
            if (ResolveFormat != VK_FORMAT_UNDEFINED)
            {
                ResolveReferences[ColorIdx] = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, nullptr, (uint32_t)PassAttachDescs.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

                // Pass1 color buffers to resolve to at end of pass.
                VkAttachmentDescription2KHR AttachmentDescResolvePass1 = {
                    VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR, /*sType*/
                    nullptr, /*pNext*/
                    0, ResolveFormat/*format*/,
                    VK_SAMPLE_COUNT_1_BIT/*samples*/,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE/*loadOp*/,
                    VK_ATTACHMENT_STORE_OP_STORE/*storeOp*/,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE/*stencilLoadOp*/,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE/*stencilStoreOp*/,
                    VK_IMAGE_LAYOUT_UNDEFINED/*initialLayout*/,
                    bPresentPass ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/*finalLayout*/ };
                PassAttachDescs.push_back(AttachmentDescResolvePass1);
            }
            ++ColorIdx;
        }
        SubpassDesc.pResolveAttachments = ResolveReferences.data();
    }
    if (bHasDepth)
    {
        SubpassDesc.pDepthStencilAttachment = &DepthReference;
    }

    assert( m_ExtFragmentShadingRate );
    assert( m_ExtFragmentShadingRate->Status == VulkanExtension::eLoaded );

    VkFragmentShadingRateAttachmentInfoKHR shading_rate_attachment = {};
    shading_rate_attachment.sType = VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
    shading_rate_attachment.pFragmentShadingRateAttachment = &VRS_AttachRef;
    shading_rate_attachment.shadingRateAttachmentTexelSize.width = m_ExtFragmentShadingRate->Properties.maxFragmentShadingRateAttachmentTexelSize.width;
    shading_rate_attachment.shadingRateAttachmentTexelSize.height = m_ExtFragmentShadingRate->Properties.maxFragmentShadingRateAttachmentTexelSize.height;
    SubpassDesc.pNext = &shading_rate_attachment;
    SubpassDesc.preserveAttachmentCount = 0;
    SubpassDesc.pPreserveAttachments = nullptr;

    // Subpass dependencies
    std::array<VkSubpassDependency2KHR, 2> PassDependencies = {};

    if (ColorFormats.empty())
    {
        // Depth only pass
        PassDependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        PassDependencies[0].pNext = nullptr;
        PassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[0].dstSubpass = 0;
        PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        PassDependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        PassDependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        PassDependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        PassDependencies[1].pNext = nullptr;
        PassDependencies[1].srcSubpass = 0;
        PassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        PassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        PassDependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        PassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        PassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }
    else
    {
        // Color (and maybe depth) pass
        PassDependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        PassDependencies[0].pNext = nullptr;
        PassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[0].dstSubpass = 0;
        PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        PassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        PassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        PassDependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        PassDependencies[1].pNext = nullptr;
        PassDependencies[1].srcSubpass = 0;
        PassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        PassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        PassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        PassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        PassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        PassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }


    // We use subpass dependencies to define the color image layout transitions rather than
// explicitly do them in the command buffer, as it is more efficient to do it this way.
#if 0
// Before we can use the back buffer from the swapchain, we must change the
// image layout from the PRESENT mode to the COLOR_ATTACHMENT mode.
    PassDependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    PassDependencies[0].pNext = nullptr;
    PassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    PassDependencies[0].dstSubpass = 0;
    PassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    PassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[0].srcAccessMask = 0;
    PassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // After writing to the back buffer of the swapchain, we need to change the
    // image layout from the COLOR_ATTACHMENT mode to the PRESENT mode which
    // is optimal for sending to the screen for users to see the completed rendering.
    PassDependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    PassDependencies[1].pNext = nullptr;
    PassDependencies[1].srcSubpass = 0;
    PassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    PassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    PassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    PassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    PassDependencies[1].dstAccessMask = 0;
    PassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
#endif

    // Now ready to actually create the render pass
    VkRenderPassCreateInfo2KHR RenderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2 , nullptr };
    RenderPassInfo.flags = (bPresentPass && m_ExtRenderPassTransformEnabled) ? VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM : 0;
    RenderPassInfo.attachmentCount = (uint32_t)PassAttachDescs.size();
    RenderPassInfo.pAttachments = PassAttachDescs.data();
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &SubpassDesc;
    RenderPassInfo.dependencyCount = (uint32_t)PassDependencies.size();
    RenderPassInfo.pDependencies = PassDependencies.data();

    RetVal = m_ExtRenderPass2->m_vkCreateRenderPass2KHR(m_VulkanDevice, &RenderPassInfo, nullptr, pRenderPass);
    if (!CheckVkError("vkCreateRenderPass2KHR()", RetVal))
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
bool Vulkan::CreatePipeline(
    VkPipelineCache                         pipelineCache,
    const VkPipelineVertexInputStateCreateInfo* visci,
    VkPipelineLayout                        pipelineLayout,
    VkRenderPass                            renderPass,
    uint32_t                                subpass,
    const VkPipelineRasterizationStateCreateInfo* providedRS,
    const VkPipelineDepthStencilStateCreateInfo*  providedDSS,
    const VkPipelineColorBlendStateCreateInfo*    providedCBS,
    const VkPipelineMultisampleStateCreateInfo*   providedMS,
    tcb::span<const VkDynamicState>         dynamicStates,
    const VkViewport*                       viewport,
    const VkRect2D*                         scissor,
    VkShaderModule                          vertShaderModule,
    VkShaderModule                          fragShaderModule,
    bool                                    bAllowDerivation,
    VkPipeline                              deriveFromPipeline,
    VkPipeline*                             pipeline)
//-----------------------------------------------------------------------------
{
    // Create a basic pipeline structure with one or two shader stages, using the supplied cache.

    // Our vertex buffer describes a triangle list.
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // State for rasterization, such as polygon fill mode is defined.
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    // Setup default blending state (disabled).  Will be ignored if providedCBS is set.
    VkPipelineColorBlendAttachmentState att_state[1] = {};
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &att_state[0];

    // Standard depth and stencil state is defined
    VkPipelineDepthStencilStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;
    ds.minDepthBounds = 0.0f;
    ds.maxDepthBounds = 1.0f;

    // Default to no msaa
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.pSampleMask = nullptr;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // We define two shader stages: our vertex and fragment shader.
    uint32_t stageCount = 1;
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";
    if (fragShaderModule != VK_NULL_HANDLE)
    {
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";
        ++stageCount;
    }

    // Set up the flags
    VkPipelineCreateFlags flags = 0;
    if (bAllowDerivation)
    {
        flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    }
    if (deriveFromPipeline != VK_NULL_HANDLE)
    {
        flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    }

    // Create some dynamic states.
#ifndef VK_DYNAMIC_STATE_RANGE_SIZE
    #define VK_DYNAMIC_STATE_RANGE_SIZE     (VK_DYNAMIC_STATE_STENCIL_REFERENCE - VK_DYNAMIC_STATE_VIEWPORT + 1)
#endif // VK_DYNAMIC_STATE_RANGE_SIZE

    //
    // Populate dynamic states and viewport/scissor at the same time (if no viewport or scissor is defined we can assume it wants a dynamic state)
    //
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    
    // Do we needs to add dynamic states for the viewport/scissor?
    bool setDefaultViewport = viewport == nullptr;
    bool setDefaultScissor = scissor == nullptr;

    std::array<VkDynamicState, VK_DYNAMIC_STATE_RANGE_SIZE> dynamicStateEnables;
    if (setDefaultViewport)
        dynamicStateEnables[dynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    if (setDefaultScissor)
        dynamicStateEnables[dynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
    if (m_ExtFragmentShadingRate &&
        m_ExtFragmentShadingRate->Status == VulkanExtension::eLoaded &&
        m_ExtFragmentShadingRate->RequestedFeatures.attachmentFragmentShadingRate == VK_TRUE)
        dynamicStateEnables[dynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
    dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

#if 0
    if( !dynamicStates.empty() )
    {
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
        dynamicStateCreateInfo.dynamicStateCount = (uint32_t) dynamicStates.size();
        // see if we already have viewport and/or scissor in the dynamic states passed to this function
        for( const auto& dynamicState : dynamicStates )
        {
            if( dynamicState == VK_DYNAMIC_STATE_VIEWPORT )
                setDefaultViewport = false;
            else if( dynamicState == VK_DYNAMIC_STATE_SCISSOR )
                setDefaultScissor = false;
        }
    }
#endif

    VkViewport defaultViewport;
    if( setDefaultViewport )
    {
        defaultViewport = {};
        defaultViewport.height = (float) m_SurfaceHeight;
        defaultViewport.width = (float) m_SurfaceWidth;
        defaultViewport.minDepth = (float) 0.0f;
        defaultViewport.maxDepth = (float) 1.0f;
    }

    VkRect2D defaultScissor;
    if( setDefaultScissor )
    {
        defaultScissor = {};
        defaultScissor.extent.width = m_SurfaceWidth;
        defaultScissor.extent.height = m_SurfaceHeight;
        defaultScissor.offset.x = 0;
        defaultScissor.offset.y = 0;
    }

    VkPipelineViewportStateCreateInfo ViewportInfo {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    ViewportInfo.viewportCount = (setDefaultViewport||viewport) ? 1 : 0;
    ViewportInfo.pViewports = setDefaultViewport ? &defaultViewport : viewport;
    ViewportInfo.scissorCount = (setDefaultScissor||scissor) ? 1 : 0;
    ViewportInfo.pScissors = setDefaultScissor ? &defaultScissor : scissor;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCreateInfo.flags = flags;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.pVertexInputState = visci;
    pipelineCreateInfo.pInputAssemblyState = &ia;
    pipelineCreateInfo.pRasterizationState = (providedRS != nullptr) ? providedRS : &rs;
    pipelineCreateInfo.pColorBlendState = (providedCBS != nullptr) ? providedCBS : &cb;
    pipelineCreateInfo.pMultisampleState = (providedMS != nullptr) ? providedMS : &ms;
    pipelineCreateInfo.pViewportState = &ViewportInfo;
    pipelineCreateInfo.pDepthStencilState = (providedDSS != nullptr) ? providedDSS : &ds;
    pipelineCreateInfo.pStages = &shaderStages[0];
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;// dynamicStateCreateInfo.dynamicStateCount > 0 ? : nullptr;
    pipelineCreateInfo.stageCount = stageCount;
    pipelineCreateInfo.basePipelineHandle = (deriveFromPipeline != VK_NULL_HANDLE) ? deriveFromPipeline : VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1; // indicates this field isn't used
    pipelineCreateInfo.subpass = subpass;

    VkResult RetVal = VK_SUCCESS;
    RetVal = vkCreateGraphicsPipelines(m_VulkanDevice, pipelineCache,  1, &pipelineCreateInfo, nullptr, pipeline);
    if (!CheckVkError("vkCreateGraphicsPipelines()", RetVal))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::CreateComputePipeline(
    VkPipelineCache  pipelineCache,
    VkPipelineLayout pipelineLayout,
    VkShaderModule   computeModule,
    VkPipeline*      pipeline)
//-----------------------------------------------------------------------------
{
    VkComputePipelineCreateInfo info = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.flags = 0;
    info.layout = pipelineLayout;
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = computeModule;
    info.stage.pName = "main";

    VkResult RetVal = vkCreateComputePipelines(m_VulkanDevice, pipelineCache, 1, &info, nullptr, pipeline);
    if (!CheckVkError("vkCreateComputePipelines()", RetVal))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::SetSwapchainHrdMetadata(const VkHdrMetadataEXT& RenderingHdrMetaData)
//-----------------------------------------------------------------------------
{
    // Set the HDR transfer function of the mastering device.
    assert(RenderingHdrMetaData.sType == VK_STRUCTURE_TYPE_HDR_METADATA_EXT);

    if (m_ExtHdrMetadata == nullptr || m_ExtHdrMetadata->Status != VulkanExtension::eLoaded || m_ExtHdrMetadata->m_vkSetHdrMetadataEXT == nullptr)
        return false;

    m_ExtHdrMetadata->m_vkSetHdrMetadataEXT(m_VulkanDevice, 1, &m_VulkanSwapchain, &RenderingHdrMetaData);
    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::ChangeSurfaceFormat(VkSurfaceFormatKHR newSurfaceFormat)
//-----------------------------------------------------------------------------
{
    if (newSurfaceFormat.format == m_SurfaceFormat && newSurfaceFormat.colorSpace == m_SurfaceColorSpace)
    {
        return true;    // didn't change anything but is successful
    }

    m_SurfaceFormat = newSurfaceFormat.format;
    m_SurfaceColorSpace = newSurfaceFormat.colorSpace;

    return RecreateSwapChain();
}

//-----------------------------------------------------------------------------
bool Vulkan::RecreateSwapChain()
//-----------------------------------------------------------------------------
{
    // Wait for device to be done rendering
    VkResult RetVal = vkDeviceWaitIdle(m_VulkanDevice);
    if (!CheckVkError("vkDeviceWaitIdle()", RetVal))
    {
        return false;
    }

    //
    // Destroy everything we need to recreate in order to change the underlying swapchain
    //
    DestroyFrameBuffers();
    DestroySwapChain();

    //
    // Build the swapchain back up again
    //
    if (!InitSwapChain())
        return false;

    if (!InitSwapchainRenderPass())
        return false;

    if (!InitFrameBuffers())
        return false;

    return true;
}

//-----------------------------------------------------------------------------
Vulkan::BufferIndexAndFence Vulkan::SetNextBackBuffer()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    VkFence Fence = m_pSwapchainBuffers[m_SwapchainCurrentIndx].fence;
    VkSemaphore BackBufferSemaphore = m_pSwapchainBuffers[m_SwapchainCurrentIndx].semaphore;

    // Ensure the command buffer we will use to render the 'next image' is done rendering.
    RetVal = vkWaitForFences(m_VulkanDevice, 1, &Fence, VK_TRUE, UINT64_MAX);
    CheckVkError( "vkWaitForFences()", RetVal );

    // Reset Fence, ready to be set by the GPU when the command buffer has been submitted and completed.
    vkResetFences(m_VulkanDevice, 1, &Fence);

    // Get the next image to render to, then queue a wait until the image is ready
    uint32_t SwapchainPresentIndx = 0;
    RetVal = vkAcquireNextImageKHR(m_VulkanDevice, m_VulkanSwapchain, UINT64_MAX, BackBufferSemaphore, VK_NULL_HANDLE, &SwapchainPresentIndx);
    if (RetVal == VK_ERROR_OUT_OF_DATE_KHR)
    {
        LOGI("VK_ERROR_OUT_OF_DATE_KHR not handled in sample");
    }
    else if (RetVal == VK_SUBOPTIMAL_KHR)
    {
        LOGI("VK_SUBOPTIMAL_KHR not handled in sample");
    }
    else if (!CheckVkError("vkAcquireNextImageKHR()", RetVal))
    {
    }

    // Grab the swapchain index and then increment.
    // This index is decoupled from the present index returned by vkAcquireNextImageKHR (which may not run in sequence depending on present modes etc).
    const uint32_t CurrentIndex = m_SwapchainCurrentIndx++;
    if( m_SwapchainCurrentIndx == m_SwapchainImageCount )
        m_SwapchainCurrentIndx = 0;
    return {CurrentIndex, SwapchainPresentIndx, Fence, BackBufferSemaphore};
}

//-----------------------------------------------------------------------------
bool Vulkan::QueueSubmit(const tcb::span<const VkCommandBuffer> CommandBuffers, const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, bool UseComputeCmdPool, VkFence CompletedFence)
//-----------------------------------------------------------------------------
{
    assert( !CommandBuffers.empty() );

    // ... submit to the queue
    VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    SubmitInfo.commandBufferCount = (uint32_t) CommandBuffers.size();
    SubmitInfo.pCommandBuffers = CommandBuffers.data();

    if (!WaitSemaphores.empty())
    {
        SubmitInfo.waitSemaphoreCount = (uint32_t)WaitSemaphores.size();
        SubmitInfo.pWaitSemaphores = WaitSemaphores.data();
        assert(WaitDstStageMasks.size() == WaitSemaphores.size());   // expects wait semaphores to have a corresponding mask
        SubmitInfo.pWaitDstStageMask = WaitDstStageMasks.data();
    }

    if (!SignalSemaphores.empty())
    {
        SubmitInfo.signalSemaphoreCount = (uint32_t)SignalSemaphores.size();
        SubmitInfo.pSignalSemaphores = SignalSemaphores.data();
    }

    VkQueue Queue = (UseComputeCmdPool && m_VulkanComputeQueue != VK_NULL_HANDLE) ? m_VulkanComputeQueue : m_VulkanQueue;
    VkResult RetVal = vkQueueSubmit(Queue, 1, &SubmitInfo, CompletedFence);
    if (!CheckVkError("vkQueueSubmit()", RetVal))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::PresentQueue(const tcb::span<const VkSemaphore> pWaitSemaphores, uint32_t SwapchainPresentIndx)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // Build up the present info and present the queue
    VkPresentInfoKHR PresentInfo {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    PresentInfo.waitSemaphoreCount = 0;
    PresentInfo.pWaitSemaphores = nullptr;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &m_VulkanSwapchain;
    PresentInfo.pImageIndices = &SwapchainPresentIndx;
    PresentInfo.pResults = nullptr;

    // Do we wait for a semaphore before we present?
    if (!pWaitSemaphores.empty())
    {
        PresentInfo.waitSemaphoreCount = (uint32_t)pWaitSemaphores.size();
        PresentInfo.pWaitSemaphores = pWaitSemaphores.data();
    }

    RetVal = vkQueuePresentKHR(m_VulkanQueue, &PresentInfo);
    if (RetVal == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // Swapchain is out of data.  This can happen if the window was
        // resized after it was created (which is NOT supported here)
        LOGE("Swapchain is out of date! Unable to get next image!");
        return false;
    }
    else if (RetVal == VK_SUBOPTIMAL_KHR)
    {
        // This should not be spammed.
        // TODO: maybe log it every so often?
        VkSurfaceCapabilitiesKHR optimalSurfaceCaps;
        if (QuerySurfaceCapabilities(optimalSurfaceCaps))
        {
            LOGE("Swapchain is not optimal! Should still be presented\n  Ideal swapchain %d x %d (actual %d x %d)", optimalSurfaceCaps.currentExtent.width, optimalSurfaceCaps.currentExtent.height, m_SurfaceWidth, m_SurfaceHeight);
        }
    }
    else
    {
        if (!CheckVkError("vkQueuePresentKHR()", RetVal))
        {
            return false;
        }
    }

    // Do NOT wait for the queue to be idle.  It creates sync point between the CPU and the GPU 
    // Later: May want to do this if using validation layers. Some indication it might leak memory otherwise
#if 0//defined(USES_VULKAN_DEBUG_LAYERS)
    if(gEnableValidation)
    {
        RetVal = vkQueueWaitIdle(m_VulkanQueue);
        if (!CheckVkError("vkQueueWaitIdle()", RetVal))
        {
            return false;
        }
    }
#endif // USES_VULKAN_DEBUG_LAYERS

    return true;
}

//-----------------------------------------------------------------------------
VkCommandBuffer Vulkan::StartSetupCommandBuffer()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // Make sure we are not leaking by starting without releasing!
    if (m_SetupCmdBuffer != VK_NULL_HANDLE)
    {
        LOGE("Setup CommandBuffer has NOT been freed!");
        vkFreeCommandBuffers(m_VulkanDevice, m_VulkanCmdPool, 1, &m_SetupCmdBuffer);
        m_SetupCmdBuffer = VK_NULL_HANDLE;
    }

    // Allocate the setup command buffer...
    VkCommandBufferAllocateInfo AllocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    AllocInfo.commandPool = m_VulkanCmdPool;
    AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocInfo.commandBufferCount = 1;

    RetVal = vkAllocateCommandBuffers(m_VulkanDevice, &AllocInfo, &m_SetupCmdBuffer);
    if (!CheckVkError("vkAllocateCommandBuffers()", RetVal))
    {
        return VK_NULL_HANDLE;
    }

    // ... and start it up
    VkCommandBufferBeginInfo BeginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    BeginInfo.flags = 0;
    BeginInfo.pInheritanceInfo = nullptr;

    RetVal = vkBeginCommandBuffer(m_SetupCmdBuffer, &BeginInfo);
    if (!CheckVkError("vkBeginCommandBuffer()", RetVal))
    {
        return VK_NULL_HANDLE;
    }
    return m_SetupCmdBuffer;
}

//-----------------------------------------------------------------------------
void Vulkan::FinishSetupCommandBuffer(VkCommandBuffer setupCmdBuffer)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;

    // Make sure we are not out of state!
    if (m_SetupCmdBuffer == VK_NULL_HANDLE)
    {
        LOGE("Setup CommandBuffer has NOT been set started!");
        return;
    }
    if (m_SetupCmdBuffer != setupCmdBuffer)
    {
        LOGE("Setup CommandBuffer does not match he one passed in to FinishSetupCommandBuffer!");
    }

    // Stop recording the command buffer...
    RetVal = vkEndCommandBuffer(m_SetupCmdBuffer);
    if (!CheckVkError("vkEndCommandBuffer()", RetVal))
    {
        return;
    }

    // ... submit the command buffer ...
    VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    SubmitInfo.waitSemaphoreCount = 0;
    SubmitInfo.pWaitSemaphores = nullptr;
    SubmitInfo.pWaitDstStageMask = nullptr;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &m_SetupCmdBuffer;
    SubmitInfo.signalSemaphoreCount = 0;
    SubmitInfo.pSignalSemaphores = nullptr;

    RetVal = vkQueueSubmit(m_VulkanQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    if (!CheckVkError("vkQueueSubmit()", RetVal))
    {
        vkFreeCommandBuffers(m_VulkanDevice, m_VulkanCmdPool, 1, &m_SetupCmdBuffer);
        m_SetupCmdBuffer = VK_NULL_HANDLE;
        return;
    }

    // ... wait for it to complete...
    // This CRASHES the desktop drivers if commented out!
    RetVal = vkQueueWaitIdle(m_VulkanQueue);
    if (!CheckVkError("vkQueueWaitIdle()", RetVal))
    {
        vkFreeCommandBuffers(m_VulkanDevice, m_VulkanCmdPool, 1, &m_SetupCmdBuffer);
        m_SetupCmdBuffer = VK_NULL_HANDLE;
        return;
    }

    // ... and then clean up the command buffer
    vkFreeCommandBuffers(m_VulkanDevice, m_VulkanCmdPool, 1, &m_SetupCmdBuffer);
    m_SetupCmdBuffer = VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
bool Vulkan::HasLoadedVulkanDeviceExtension( const std::string& extensionName ) const
//-----------------------------------------------------------------------------
{
    auto foundIt = m_DeviceExtensions.m_Extensions.find( extensionName );
    if (foundIt == m_DeviceExtensions.m_Extensions.end())
        return false;
    return foundIt->second->Status == VulkanExtension::eLoaded;
}

//-----------------------------------------------------------------------------
bool Vulkan::FillRenderPassTransformBeginInfoQCOM(VkRenderPassTransformBeginInfoQCOM& RPTransformBeginInfoQCOM) const
//-----------------------------------------------------------------------------
{
    if (!m_ExtRenderPassTransformEnabled)
        return false;

#ifndef VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM_LEGACY
#define VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM_LEGACY 1000282000
#endif // VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM_LEGACY

    RPTransformBeginInfoQCOM.transform = m_SwapchainPreTransform;
    RPTransformBeginInfoQCOM.sType = (VkStructureType)(m_ExtRenderPassTransformLegacy ? VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM_LEGACY : VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM);
    RPTransformBeginInfoQCOM.pNext = nullptr;
    return true;
}

//-----------------------------------------------------------------------------
bool Vulkan::FillCommandBufferInheritanceRenderPassTransformInfoQCOM(VkCommandBufferInheritanceRenderPassTransformInfoQCOM& InheritanceInfoRenderPassTransform) const
//-----------------------------------------------------------------------------
{
    if (!m_ExtRenderPassTransformEnabled)
        return false;

#ifndef VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM_LEGACY
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM_LEGACY	1000282001
#endif // VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM_LEGACY


    InheritanceInfoRenderPassTransform.sType = (VkStructureType)(m_ExtRenderPassTransformLegacy ? VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM_LEGACY : VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM);
    InheritanceInfoRenderPassTransform.pNext = nullptr;
    InheritanceInfoRenderPassTransform.renderArea.offset = { 0,0 };
    InheritanceInfoRenderPassTransform.renderArea.extent = { m_SurfaceWidth, m_SurfaceHeight };
    InheritanceInfoRenderPassTransform.transform = m_SwapchainPreTransform;
    return true;
}


//=============================================================================
bool Vulkan::SetDebugObjectName(uint64_t object, VkObjectType objectType, const char *name)
//=============================================================================
{
#if defined(USES_VULKAN_DEBUG_LAYERS)
    if (gEnableValidation && name && *name)
    {
        return ((m_ExtDebugUtils && m_ExtDebugUtils->SetDebugUtilsObjectName( m_VulkanDevice, object, objectType, name )) ||
                (objectType <= VK_OBJECT_TYPE_COMMAND_POOL && m_ExtDebugMarker && m_ExtDebugMarker->DebugMarkerSetObjectName( m_VulkanDevice, object, (VkDebugReportObjectTypeEXT) objectType, name )));
    }
#endif // USES_VULKAN_DEBUG_LAYERS
    return true;
}

//-----------------------------------------------------------------------------
bool CheckVkError(const char* pPrefix, VkResult CheckVal)
//-----------------------------------------------------------------------------
{
    if (CheckVal == VK_SUCCESS)
        return true;

    switch (CheckVal)
    {
    case VK_NOT_READY:
        LOGE("Vulkan Error (VK_NOT_READY) from %s", pPrefix);
        break;
    case VK_TIMEOUT:
        LOGE("Vulkan Error (VK_TIMEOUT) from %s", pPrefix);
        break;
    case VK_EVENT_SET:
        LOGE("Vulkan Error (VK_EVENT_SET) from %s", pPrefix);
        break;
    case VK_EVENT_RESET:
        LOGE("Vulkan Error (VK_EVENT_RESET) from %s", pPrefix);
        break;
    case VK_INCOMPLETE:
        LOGE("Vulkan Error (VK_INCOMPLETE) from %s", pPrefix);
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        LOGE("Vulkan Error (VK_ERROR_OUT_OF_HOST_MEMORY) from %s", pPrefix);
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        LOGE("Vulkan Error (VK_ERROR_OUT_OF_DEVICE_MEMORY) from %s", pPrefix);
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        LOGE("Vulkan Error (VK_ERROR_INITIALIZATION_FAILED) from %s", pPrefix);
        break;
    case VK_ERROR_DEVICE_LOST:
        LOGE("Vulkan Error (VK_ERROR_DEVICE_LOST) from %s", pPrefix);
        break;
    case VK_ERROR_MEMORY_MAP_FAILED:
        LOGE("Vulkan Error (VK_ERROR_MEMORY_MAP_FAILED) from %s", pPrefix);
        break;
    case VK_ERROR_LAYER_NOT_PRESENT:
        LOGE("Vulkan Error (VK_ERROR_LAYER_NOT_PRESENT) from %s", pPrefix);
        break;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        LOGE("Vulkan Error (VK_ERROR_EXTENSION_NOT_PRESENT) from %s", pPrefix);
        break;
    case VK_ERROR_FEATURE_NOT_PRESENT:
        LOGE("Vulkan Error (VK_ERROR_FEATURE_NOT_PRESENT) from %s", pPrefix);
        break;
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        LOGE("Vulkan Error (VK_ERROR_INCOMPATIBLE_DRIVER) from %s", pPrefix);
        break;
    case VK_ERROR_TOO_MANY_OBJECTS:
        LOGE("Vulkan Error (VK_ERROR_TOO_MANY_OBJECTS) from %s", pPrefix);
        break;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        LOGE("Vulkan Error (VK_ERROR_FORMAT_NOT_SUPPORTED) from %s", pPrefix);
        break;
    case VK_ERROR_SURFACE_LOST_KHR:
        LOGE("Vulkan Error (VK_ERROR_SURFACE_LOST_KHR) from %s", pPrefix);
        break;
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        LOGE("Vulkan Error (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR) from %s", pPrefix);
        break;
    case VK_SUBOPTIMAL_KHR:
        LOGE("Vulkan Error (VK_SUBOPTIMAL_KHR) from %s", pPrefix);
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        LOGE("Vulkan Error (VK_ERROR_OUT_OF_DATE_KHR) from %s", pPrefix);
        break;
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        LOGE("Vulkan Error (VK_ERROR_INCOMPATIBLE_DISPLAY_KHR) from %s", pPrefix);
        break;
    case VK_ERROR_VALIDATION_FAILED_EXT:
        LOGE("Vulkan Error (VK_ERROR_VALIDATION_FAILED_EXT) from %s", pPrefix);
        break;

#if defined(OS_WINDOWS)
    case VK_ERROR_UNKNOWN:
        LOGE("Vulkan Error (VK_ERROR_UNKNOWN) from %s", pPrefix);
        break;
#endif // defined(OS_WINDOWS)

    default:
        LOGE("Vulkan Error (%d) from %s", CheckVal, pPrefix);
        break;
    }

    return false;
}

//-----------------------------------------------------------------------------
const char* Vulkan::VulkanFormatString(VkFormat WhichFormat)
//-----------------------------------------------------------------------------
{
    switch (WhichFormat)
    {
#define CASE_ENUM_RETURN_STRING(X) case X: return #X
        CASE_ENUM_RETURN_STRING(VK_FORMAT_UNDEFINED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R4G4_UNORM_PACK8);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R4G4B4A4_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B4G4R4A4_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R5G6B5_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B5G6R5_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R5G5B5A1_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B5G5R5A1_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A1R5G5B5_UNORM_PACK16);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8_SRGB);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8_SRGB);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8_SRGB);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8_SRGB);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R8G8B8A8_SRGB);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B8G8R8A8_SRGB);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_UNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_SNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_USCALED_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_UINT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_SINT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A8B8G8R8_SRGB_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2R10G10B10_SNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2R10G10B10_USCALED_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2R10G10B10_UINT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2R10G10B10_SINT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2B10G10R10_SNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2B10G10R10_USCALED_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2B10G10R10_UINT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_A2B10G10R10_SINT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_SNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_USCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_SSCALED);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R16G16B16A16_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32B32_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32B32_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32B32_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32B32A32_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32B32A32_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R32G32B32A32_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64B64_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64B64_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64B64_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64B64A64_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64B64A64_SINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_R64G64B64A64_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_D16_UNORM);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_X8_D24_UNORM_PACK32);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_D32_SFLOAT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_S8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_D16_UNORM_S8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_D24_UNORM_S8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_D32_SFLOAT_S8_UINT);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC2_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC2_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC3_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC3_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC4_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC4_SNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC5_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC5_SNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC6H_UFLOAT_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC6H_SFLOAT_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC7_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_BC7_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_EAC_R11_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_EAC_R11_SNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
        CASE_ENUM_RETURN_STRING(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
#undef CASE_ENUM_RETURN_STRING
    default:
        return "Unknown!";
    }
}

//-----------------------------------------------------------------------------
const char* Vulkan::VulkanColorSpaceString(VkColorSpaceKHR ColorSpace)
//-----------------------------------------------------------------------------
{
    switch (ColorSpace)
    {
#define CASE_ENUM_RETURN_STRING(X) case X: return #X;
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_BT709_LINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_BT709_NONLINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_BT2020_LINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_HDR10_ST2084_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_DOLBYVISION_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_HDR10_HLG_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_PASS_THROUGH_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT);
        CASE_ENUM_RETURN_STRING(VK_COLOR_SPACE_DISPLAY_NATIVE_AMD);
#undef CASE_ENUM_RETURN_STRING
    default:
        return "Unknown!";
    }
}

//-----------------------------------------------------------------------------
void Vulkan::QueryPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, PhysicalDeviceFeatures& featuresOut)
//-----------------------------------------------------------------------------
{
    assert(featuresOut.Base.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    featuresOut.Base.pNext = nullptr;

    m_GetPhysicalDeviceFeaturesExtensions.PushExtensions(&featuresOut.Base);

    if (featuresOut.Base.pNext != nullptr)
    {
        if (fpGetPhysicalDeviceFeatures2 != nullptr)
        {
            fpGetPhysicalDeviceFeatures2(physicalDevice, &featuresOut.Base);
        }
        else
        {
            LOGE("Vulkan version does not provide vkGetPhysicalDeviceFeatures2 which is required to get the requested feature information"); // if this is an issue we can implement alternate call to VK_KHR_get_physical_device_properties2
            vkGetPhysicalDeviceFeatures(physicalDevice, &featuresOut.Base.features);
        }
    }
    else
        vkGetPhysicalDeviceFeatures(physicalDevice, &featuresOut.Base.features);

    m_GetPhysicalDeviceFeaturesExtensions.PopExtensions(&featuresOut.Base);
}

//-----------------------------------------------------------------------------
void Vulkan::QueryPhysicalDeviceProperties( VkPhysicalDevice physicalDevice, const PhysicalDeviceFeatures& features, PhysicalDeviceProperties& propertiesOut )
//-----------------------------------------------------------------------------
{
    // **********************************
    // Properties
    // **********************************
    assert( propertiesOut.Base.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 );
    assert( propertiesOut.Base.pNext == nullptr );

    m_GetPhysicalDevicePropertiesExtensions.PushExtensions( &propertiesOut.Base );

    if (fpGetPhysicalDeviceProperties2)
    {
        fpGetPhysicalDeviceProperties2( physicalDevice, &propertiesOut.Base );
    }
    else
    {
        assert( propertiesOut.Base.pNext == nullptr );  // we need fpGetPhysicalDeviceProperties2 if we want to populate a pNext chain of properties.
        vkGetPhysicalDeviceProperties( physicalDevice, &propertiesOut.Base.properties );
    }

    m_GetPhysicalDevicePropertiesExtensions.PopExtensions( &propertiesOut.Base );
}


//-----------------------------------------------------------------------------
size_t Vulkan::GetBestVulkanPhysicalDeviceId(const tcb::span<const PhysicalDeviceProperties> deviceProperties)
//-----------------------------------------------------------------------------
{
    // Prefer the first discrete GPU.
    for (size_t deviceIdx = 0; deviceIdx < deviceProperties.size(); ++deviceIdx)
    {
        if (deviceProperties[deviceIdx].Base.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return deviceIdx;
    }
    // Otherwise use the first integrated GPU.
    for (size_t deviceIdx = 0; deviceIdx < deviceProperties.size(); ++deviceIdx)
    {
        if (deviceProperties[deviceIdx].Base.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            return deviceIdx;
    }
    // fallback to the first GPU.
    return 0;
}

//-----------------------------------------------------------------------------
void Vulkan::DumpDeviceInfo( const tcb::span<PhysicalDeviceFeatures> DeviceFeatures, const tcb::span<PhysicalDeviceProperties> DeviceProperties )
//-----------------------------------------------------------------------------
{
    for (uint32_t uiIndx = 0; uiIndx < (uint32_t) DeviceFeatures.size(); uiIndx++)
    {
        LOGI("******************************");
        LOGI("Vulkan Device %u", uiIndx);
        LOGI("******************************");

        // Can now query all sorts of information about the device
        // **********************************
        // Features
        // **********************************
        const PhysicalDeviceFeatures& Features = DeviceFeatures[uiIndx];

        LOGI("Features: ");

        LOGI("    robustBufferAccess: %s", Features.Base.features.robustBufferAccess ? "True" : "False");
        LOGI("    fullDrawIndexUint32: %s", Features.Base.features.fullDrawIndexUint32 ? "True" : "False");
        LOGI("    imageCubeArray: %s", Features.Base.features.imageCubeArray ? "True" : "False");
        LOGI("    independentBlend: %s", Features.Base.features.independentBlend ? "True" : "False");
        LOGI("    geometryShader: %s", Features.Base.features.geometryShader ? "True" : "False");
        LOGI("    tessellationShader: %s", Features.Base.features.tessellationShader ? "True" : "False");
        LOGI("    sampleRateShading: %s", Features.Base.features.sampleRateShading ? "True" : "False");
        LOGI("    dualSrcBlend: %s", Features.Base.features.dualSrcBlend ? "True" : "False");
        LOGI("    logicOp: %s", Features.Base.features.logicOp ? "True" : "False");
        LOGI("    multiDrawIndirect: %s", Features.Base.features.multiDrawIndirect ? "True" : "False");
        LOGI("    drawIndirectFirstInstance: %s", Features.Base.features.drawIndirectFirstInstance ? "True" : "False");
        LOGI("    depthClamp: %s", Features.Base.features.depthClamp ? "True" : "False");
        LOGI("    depthBiasClamp: %s", Features.Base.features.depthBiasClamp ? "True" : "False");
        LOGI("    fillModeNonSolid: %s", Features.Base.features.fillModeNonSolid ? "True" : "False");
        LOGI("    depthBounds: %s", Features.Base.features.depthBounds ? "True" : "False");
        LOGI("    wideLines: %s", Features.Base.features.wideLines ? "True" : "False");
        LOGI("    largePoints: %s", Features.Base.features.largePoints ? "True" : "False");
        LOGI("    alphaToOne: %s", Features.Base.features.alphaToOne ? "True" : "False");
        LOGI("    multiViewport: %s", Features.Base.features.multiViewport ? "True" : "False");
        LOGI("    samplerAnisotropy: %s", Features.Base.features.samplerAnisotropy ? "True" : "False");
        LOGI("    textureCompressionETC2: %s", Features.Base.features.textureCompressionETC2 ? "True" : "False");
        LOGI("    textureCompressionASTC_LDR: %s", Features.Base.features.textureCompressionASTC_LDR ? "True" : "False");
        LOGI("    textureCompressionBC: %s", Features.Base.features.textureCompressionBC ? "True" : "False");
        LOGI("    occlusionQueryPrecise: %s", Features.Base.features.occlusionQueryPrecise ? "True" : "False");
        LOGI("    pipelineStatisticsQuery: %s", Features.Base.features.pipelineStatisticsQuery ? "True" : "False");
        LOGI("    vertexPipelineStoresAndAtomics: %s", Features.Base.features.vertexPipelineStoresAndAtomics ? "True" : "False");
        LOGI("    fragmentStoresAndAtomics: %s", Features.Base.features.fragmentStoresAndAtomics ? "True" : "False");
        LOGI("    shaderTessellationAndGeometryPointSize: %s", Features.Base.features.shaderTessellationAndGeometryPointSize ? "True" : "False");
        LOGI("    shaderImageGatherExtended: %s", Features.Base.features.shaderImageGatherExtended ? "True" : "False");
        LOGI("    shaderStorageImageExtendedFormats: %s", Features.Base.features.shaderStorageImageExtendedFormats ? "True" : "False");
        LOGI("    shaderStorageImageMultisample: %s", Features.Base.features.shaderStorageImageMultisample ? "True" : "False");
        LOGI("    shaderStorageImageReadWithoutFormat: %s", Features.Base.features.shaderStorageImageReadWithoutFormat ? "True" : "False");
        LOGI("    shaderStorageImageWriteWithoutFormat: %s", Features.Base.features.shaderStorageImageWriteWithoutFormat ? "True" : "False");
        LOGI("    shaderUniformBufferArrayDynamicIndexing: %s", Features.Base.features.shaderUniformBufferArrayDynamicIndexing ? "True" : "False");
        LOGI("    shaderSampledImageArrayDynamicIndexing: %s", Features.Base.features.shaderSampledImageArrayDynamicIndexing ? "True" : "False");
        LOGI("    shaderStorageBufferArrayDynamicIndexing: %s", Features.Base.features.shaderStorageBufferArrayDynamicIndexing ? "True" : "False");
        LOGI("    shaderStorageImageArrayDynamicIndexing: %s", Features.Base.features.shaderStorageImageArrayDynamicIndexing ? "True" : "False");
        LOGI("    shaderClipDistance: %s", Features.Base.features.shaderClipDistance ? "True" : "False");
        LOGI("    shaderCullDistance: %s", Features.Base.features.shaderCullDistance ? "True" : "False");
        LOGI("    shaderFloat64: %s", Features.Base.features.shaderFloat64 ? "True" : "False");
        LOGI("    shaderInt64: %s", Features.Base.features.shaderInt64 ? "True" : "False");
        LOGI("    shaderInt16: %s", Features.Base.features.shaderInt16 ? "True" : "False");
        LOGI("    shaderResourceResidency: %s", Features.Base.features.shaderResourceResidency ? "True" : "False");
        LOGI("    shaderResourceMinLod: %s", Features.Base.features.shaderResourceMinLod ? "True" : "False");
        LOGI("    sparseBinding: %s", Features.Base.features.sparseBinding ? "True" : "False");
        LOGI("    sparseResidencyBuffer: %s", Features.Base.features.sparseResidencyBuffer ? "True" : "False");
        LOGI("    sparseResidencyImage2D: %s", Features.Base.features.sparseResidencyImage2D ? "True" : "False");
        LOGI("    sparseResidencyImage3D: %s", Features.Base.features.sparseResidencyImage3D ? "True" : "False");
        LOGI("    sparseResidency2Samples: %s", Features.Base.features.sparseResidency2Samples ? "True" : "False");
        LOGI("    sparseResidency4Samples: %s", Features.Base.features.sparseResidency4Samples ? "True" : "False");
        LOGI("    sparseResidency8Samples: %s", Features.Base.features.sparseResidency8Samples ? "True" : "False");
        LOGI("    sparseResidency16Samples: %s", Features.Base.features.sparseResidency16Samples ? "True" : "False");
        LOGI("    sparseResidencyAliased: %s", Features.Base.features.sparseResidencyAliased ? "True" : "False");
        LOGI("    variableMultisampleRate: %s", Features.Base.features.variableMultisampleRate ? "True" : "False");
        LOGI("    inheritedQueries: %s", Features.Base.features.inheritedQueries ? "True" : "False");

        // Display features from all the registered extensions
        {
            VulkanDeviceFeaturePrint registeredExtensionPrint;
            m_VulkanDeviceFeaturePrintExtensions.PushExtensions(&registeredExtensionPrint);
        }

        if (DeviceProperties.size() <= uiIndx)
            continue;

        const PhysicalDeviceProperties& Properties = DeviceProperties[uiIndx];

        // **********************************
        // Properties
        // **********************************

        LOGI("Properties: ");
        uint32_t MajorVersion = VK_VERSION_MAJOR( Properties.Base.properties.apiVersion);
        uint32_t MinorVersion = VK_VERSION_MINOR( Properties.Base.properties.apiVersion);
        uint32_t PatchVersion = VK_VERSION_PATCH( Properties.Base.properties.apiVersion);
        LOGI("    apiVersion: %d.%d.%d", MajorVersion, MinorVersion, PatchVersion);

        uint32_t DriverMajorVersion = VK_VERSION_MAJOR( Properties.Base.properties.driverVersion);
        uint32_t DriverMinorVersion = VK_VERSION_MINOR( Properties.Base.properties.driverVersion);
        uint32_t DriverPatchVersion = VK_VERSION_PATCH( Properties.Base.properties.driverVersion);
        LOGI("    driverVersion: %d.%d.%d", DriverMajorVersion, DriverMinorVersion, DriverPatchVersion);

        LOGI("    vendorID: %d", Properties.Base.properties.vendorID);
        LOGI("    deviceID: %d", Properties.Base.properties.deviceID);

        switch (Properties.Base.properties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            LOGI("    deviceType: VK_PHYSICAL_DEVICE_TYPE_OTHER");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            LOGI("    deviceType: VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            LOGI("    deviceType: VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            LOGI("    deviceType: VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            LOGI("    deviceType: VK_PHYSICAL_DEVICE_TYPE_CPU");
            break;

        default:
            LOGI("    deviceType: Unknown! (%d)", Properties.Base.properties.deviceType);
            break;
        }

        LOGI("    deviceName: %s", Properties.Base.properties.deviceName);

        LOGI("    limits:");
        LOGI("        maxImageDimension1D: %d", Properties.Base.properties.limits.maxImageDimension1D);
        LOGI("        maxImageDimension2D: %d", Properties.Base.properties.limits.maxImageDimension2D);
        LOGI("        maxImageDimension3D: %d", Properties.Base.properties.limits.maxImageDimension3D);
        LOGI("        maxImageDimensionCube: %d", Properties.Base.properties.limits.maxImageDimensionCube);
        LOGI("        maxImageArrayLayers: %d", Properties.Base.properties.limits.maxImageArrayLayers);
        LOGI("        maxTexelBufferElements: %d", Properties.Base.properties.limits.maxTexelBufferElements);
        LOGI("        maxUniformBufferRange: %d", Properties.Base.properties.limits.maxUniformBufferRange);
        LOGI("        maxStorageBufferRange: %d", Properties.Base.properties.limits.maxStorageBufferRange);
        LOGI("        maxPushConstantsSize: %d", Properties.Base.properties.limits.maxPushConstantsSize);
        LOGI("        maxMemoryAllocationCount: %d", Properties.Base.properties.limits.maxMemoryAllocationCount);
        LOGI("        maxSamplerAllocationCount: %d", Properties.Base.properties.limits.maxSamplerAllocationCount);
        LOGI("        bufferImageGranularity: %zu", Properties.Base.properties.limits.bufferImageGranularity);
        LOGI("        sparseAddressSpaceSize: %zu", Properties.Base.properties.limits.sparseAddressSpaceSize);
        LOGI("        maxBoundDescriptorSets: %d", Properties.Base.properties.limits.maxBoundDescriptorSets);
        LOGI("        maxPerStageDescriptorSamplers: %d", Properties.Base.properties.limits.maxPerStageDescriptorSamplers);
        LOGI("        maxPerStageDescriptorUniformBuffers: %d", Properties.Base.properties.limits.maxPerStageDescriptorUniformBuffers);
        LOGI("        maxPerStageDescriptorStorageBuffers: %d", Properties.Base.properties.limits.maxPerStageDescriptorStorageBuffers);
        LOGI("        maxPerStageDescriptorSampledImages: %d", Properties.Base.properties.limits.maxPerStageDescriptorSampledImages);
        LOGI("        maxPerStageDescriptorStorageImages: %d", Properties.Base.properties.limits.maxPerStageDescriptorStorageImages);
        LOGI("        maxPerStageDescriptorInputAttachments: %d", Properties.Base.properties.limits.maxPerStageDescriptorInputAttachments);
        LOGI("        maxPerStageResources: %d", Properties.Base.properties.limits.maxPerStageResources);
        LOGI("        maxDescriptorSetSamplers: %d", Properties.Base.properties.limits.maxDescriptorSetSamplers);
        LOGI("        maxDescriptorSetUniformBuffers: %d", Properties.Base.properties.limits.maxDescriptorSetUniformBuffers);
        LOGI("        maxDescriptorSetUniformBuffersDynamic: %d", Properties.Base.properties.limits.maxDescriptorSetUniformBuffersDynamic);
        LOGI("        maxDescriptorSetStorageBuffers: %d", Properties.Base.properties.limits.maxDescriptorSetStorageBuffers);
        LOGI("        maxDescriptorSetStorageBuffersDynamic: %d", Properties.Base.properties.limits.maxDescriptorSetStorageBuffersDynamic);
        LOGI("        maxDescriptorSetSampledImages: %d", Properties.Base.properties.limits.maxDescriptorSetSampledImages);
        LOGI("        maxDescriptorSetStorageImages: %d", Properties.Base.properties.limits.maxDescriptorSetStorageImages);
        LOGI("        maxDescriptorSetInputAttachments: %d", Properties.Base.properties.limits.maxDescriptorSetInputAttachments);
        LOGI("        maxVertexInputAttributes: %d", Properties.Base.properties.limits.maxVertexInputAttributes);
        LOGI("        maxVertexInputBindings: %d", Properties.Base.properties.limits.maxVertexInputBindings);
        LOGI("        maxVertexInputAttributeOffset: %d", Properties.Base.properties.limits.maxVertexInputAttributeOffset);
        LOGI("        maxVertexInputBindingStride: %d", Properties.Base.properties.limits.maxVertexInputBindingStride);
        LOGI("        maxVertexOutputComponents: %d", Properties.Base.properties.limits.maxVertexOutputComponents);
        LOGI("        maxTessellationGenerationLevel: %d", Properties.Base.properties.limits.maxTessellationGenerationLevel);
        LOGI("        maxTessellationPatchSize: %d", Properties.Base.properties.limits.maxTessellationPatchSize);
        LOGI("        maxTessellationControlPerVertexInputComponents: %d", Properties.Base.properties.limits.maxTessellationControlPerVertexInputComponents);
        LOGI("        maxTessellationControlPerVertexOutputComponents: %d", Properties.Base.properties.limits.maxTessellationControlPerVertexOutputComponents);
        LOGI("        maxTessellationControlPerPatchOutputComponents: %d", Properties.Base.properties.limits.maxTessellationControlPerPatchOutputComponents);
        LOGI("        maxTessellationControlTotalOutputComponents: %d", Properties.Base.properties.limits.maxTessellationControlTotalOutputComponents);
        LOGI("        maxTessellationEvaluationInputComponents: %d", Properties.Base.properties.limits.maxTessellationEvaluationInputComponents);
        LOGI("        maxTessellationEvaluationOutputComponents: %d", Properties.Base.properties.limits.maxTessellationEvaluationOutputComponents);
        LOGI("        maxGeometryShaderInvocations: %d", Properties.Base.properties.limits.maxGeometryShaderInvocations);
        LOGI("        maxGeometryInputComponents: %d", Properties.Base.properties.limits.maxGeometryInputComponents);
        LOGI("        maxGeometryOutputComponents: %d", Properties.Base.properties.limits.maxGeometryOutputComponents);
        LOGI("        maxGeometryOutputVertices: %d", Properties.Base.properties.limits.maxGeometryOutputVertices);
        LOGI("        maxGeometryTotalOutputComponents: %d", Properties.Base.properties.limits.maxGeometryTotalOutputComponents);
        LOGI("        maxFragmentInputComponents: %d", Properties.Base.properties.limits.maxFragmentInputComponents);
        LOGI("        maxFragmentOutputAttachments: %d", Properties.Base.properties.limits.maxFragmentOutputAttachments);
        LOGI("        maxFragmentDualSrcAttachments: %d", Properties.Base.properties.limits.maxFragmentDualSrcAttachments);
        LOGI("        maxFragmentCombinedOutputResources: %d", Properties.Base.properties.limits.maxFragmentCombinedOutputResources);
        LOGI("        maxComputeSharedMemorySize: %d", Properties.Base.properties.limits.maxComputeSharedMemorySize);
        LOGI("        maxComputeWorkGroupCount: [%d, %d, %d]", Properties.Base.properties.limits.maxComputeWorkGroupCount[0], Properties.Base.properties.limits.maxComputeWorkGroupCount[1], Properties.Base.properties.limits.maxComputeWorkGroupCount[2]);
        LOGI("        maxComputeWorkGroupInvocations: %d", Properties.Base.properties.limits.maxComputeWorkGroupInvocations);
        LOGI("        maxComputeWorkGroupSize: [%d, %d, %d]", Properties.Base.properties.limits.maxComputeWorkGroupSize[0], Properties.Base.properties.limits.maxComputeWorkGroupSize[1], Properties.Base.properties.limits.maxComputeWorkGroupSize[2]);
        LOGI("        subPixelPrecisionBits: %d", Properties.Base.properties.limits.subPixelPrecisionBits);
        LOGI("        subTexelPrecisionBits: %d", Properties.Base.properties.limits.subTexelPrecisionBits);
        LOGI("        mipmapPrecisionBits: %d", Properties.Base.properties.limits.mipmapPrecisionBits);
        LOGI("        maxDrawIndexedIndexValue: %d", Properties.Base.properties.limits.maxDrawIndexedIndexValue);
        LOGI("        maxDrawIndirectCount: %d", Properties.Base.properties.limits.maxDrawIndirectCount);
        LOGI("        maxSamplerLodBias: %f", Properties.Base.properties.limits.maxSamplerLodBias);
        LOGI("        maxSamplerAnisotropy: %f", Properties.Base.properties.limits.maxSamplerAnisotropy);
        LOGI("        maxViewports: %d", Properties.Base.properties.limits.maxViewports);
        LOGI("        maxViewportDimensions: [%d, %d]", Properties.Base.properties.limits.maxViewportDimensions[0], Properties.Base.properties.limits.maxViewportDimensions[1]);
        LOGI("        viewportBoundsRange: [%f, %f]", Properties.Base.properties.limits.viewportBoundsRange[0], Properties.Base.properties.limits.viewportBoundsRange[1]);
        LOGI("        viewportSubPixelBits: %d", Properties.Base.properties.limits.viewportSubPixelBits);
        LOGI("        minMemoryMapAlignment: %zu", Properties.Base.properties.limits.minMemoryMapAlignment);
        LOGI("        minTexelBufferOffsetAlignment: %zu", Properties.Base.properties.limits.minTexelBufferOffsetAlignment);
        LOGI("        minUniformBufferOffsetAlignment: %zu", Properties.Base.properties.limits.minUniformBufferOffsetAlignment);
        LOGI("        minStorageBufferOffsetAlignment: %zu", Properties.Base.properties.limits.minStorageBufferOffsetAlignment);
        LOGI("        minTexelOffset: %d", Properties.Base.properties.limits.minTexelOffset);
        LOGI("        maxTexelOffset: %d", Properties.Base.properties.limits.maxTexelOffset);
        LOGI("        minTexelGatherOffset: %d", Properties.Base.properties.limits.minTexelGatherOffset);
        LOGI("        maxTexelGatherOffset: %d", Properties.Base.properties.limits.maxTexelGatherOffset);
        LOGI("        minInterpolationOffset: %f", Properties.Base.properties.limits.minInterpolationOffset);
        LOGI("        maxInterpolationOffset: %f", Properties.Base.properties.limits.maxInterpolationOffset);
        LOGI("        subPixelInterpolationOffsetBits: %d", Properties.Base.properties.limits.subPixelInterpolationOffsetBits);
        LOGI("        maxFramebufferWidth: %d", Properties.Base.properties.limits.maxFramebufferWidth);
        LOGI("        maxFramebufferHeight: %d", Properties.Base.properties.limits.maxFramebufferHeight);
        LOGI("        maxFramebufferLayers: %d", Properties.Base.properties.limits.maxFramebufferLayers);
        LOGI("        framebufferColorSampleCounts: %d", Properties.Base.properties.limits.framebufferColorSampleCounts);
        LOGI("        framebufferDepthSampleCounts: %d", Properties.Base.properties.limits.framebufferDepthSampleCounts);
        LOGI("        framebufferStencilSampleCounts: %d", Properties.Base.properties.limits.framebufferStencilSampleCounts);
        LOGI("        framebufferNoAttachmentsSampleCounts: %d", Properties.Base.properties.limits.framebufferNoAttachmentsSampleCounts);
        LOGI("        maxColorAttachments: %d", Properties.Base.properties.limits.maxColorAttachments);
        LOGI("        sampledImageColorSampleCounts: %d", Properties.Base.properties.limits.sampledImageColorSampleCounts);
        LOGI("        sampledImageIntegerSampleCounts: %d", Properties.Base.properties.limits.sampledImageIntegerSampleCounts);
        LOGI("        sampledImageDepthSampleCounts: %d", Properties.Base.properties.limits.sampledImageDepthSampleCounts);
        LOGI("        sampledImageStencilSampleCounts: %d", Properties.Base.properties.limits.sampledImageStencilSampleCounts);
        LOGI("        storageImageSampleCounts: %d", Properties.Base.properties.limits.storageImageSampleCounts);
        LOGI("        maxSampleMaskWords: %d", Properties.Base.properties.limits.maxSampleMaskWords);
        LOGI("        timestampComputeAndGraphics: %s", Properties.Base.properties.limits.timestampComputeAndGraphics ? "True" : "False");
        LOGI("        timestampPeriod: %f", Properties.Base.properties.limits.timestampPeriod);
        LOGI("        maxClipDistances: %d", Properties.Base.properties.limits.maxClipDistances);
        LOGI("        maxCullDistances: %d", Properties.Base.properties.limits.maxCullDistances);
        LOGI("        maxCombinedClipAndCullDistances: %d", Properties.Base.properties.limits.maxCombinedClipAndCullDistances);
        LOGI("        discreteQueuePriorities: %d", Properties.Base.properties.limits.discreteQueuePriorities);
        LOGI("        pointSizeRange: [%f, %f]", Properties.Base.properties.limits.pointSizeRange[0], Properties.Base.properties.limits.pointSizeRange[1]);
        LOGI("        lineWidthRange: [%f, %f]", Properties.Base.properties.limits.lineWidthRange[0], Properties.Base.properties.limits.lineWidthRange[1]);
        LOGI("        pointSizeGranularity: %f", Properties.Base.properties.limits.pointSizeGranularity);
        LOGI("        lineWidthGranularity: %f", Properties.Base.properties.limits.lineWidthGranularity);
        LOGI("        strictLines: %s", Properties.Base.properties.limits.strictLines ? "True" : "False");
        LOGI("        standardSampleLocations: %s", Properties.Base.properties.limits.standardSampleLocations ? "True" : "False");
        LOGI("        optimalBufferCopyOffsetAlignment: %zu", Properties.Base.properties.limits.optimalBufferCopyOffsetAlignment);
        LOGI("        optimalBufferCopyRowPitchAlignment: %zu", Properties.Base.properties.limits.optimalBufferCopyRowPitchAlignment);
        LOGI("        nonCoherentAtomSize: %zu", Properties.Base.properties.limits.nonCoherentAtomSize);

        // Display features from all the registered extensions
        {
            m_VulkanDevicePropertiesPrintExtensions.PushExtensions( nullptr );
        }
    }

}

// ****************************************************************************
// Code from HLM team - Begin
// ****************************************************************************

#if OS_WINDOWS

bool memory_type_from_properties(VkPhysicalDeviceMemoryProperties *memory_properties,
	uint32_t typeBits, VkFlags requirements_mask,
	uint32_t *typeIndex) {
	// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < 32; i++) {
		if ((typeBits & 1) == 1) {
			// Type is available, does it match user properties?
			if ((memory_properties->memoryTypes[i].propertyFlags &
				requirements_mask) == requirements_mask) {
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	// No memory types matched, return failure
	return false;
}

struct WritePPMCleanupData {
	VkImage image2;
	VkImage image3;
	VkDeviceMemory mem2;
	VkDeviceMemory mem3;
	bool mem2mapped;
	bool mem3mapped;
};

// ****************************************************************************
// Code from HLM team - End
// ****************************************************************************

#endif // OS_WINDOWS

