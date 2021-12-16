// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#ifdef OS_WINDOWS
#define NOMINMAX
#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#elif OS_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#endif // OS_WINDOWS | OS_WINDOWS

// This definition allows prototypes of Vulkan API functions,
// rather than dynamically loading entrypoints to the API manually.
#define VK_PROTOTYPES

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#ifdef OS_ANDROID
#include "VK_QCOM_render_pass_transform.h"
#endif // OS_ANDROID

#include <vector>
#include <functional>
#include <optional>
#include "tcb/span.hpp"
#include "memory/memoryManager.hpp"

// This should actually be defined in the makefile!
#define USES_VULKAN_DEBUG_LAYERS

// This is an arbitrary number of layers
#define MAX_VALIDATION_LAYERS   50

#define NUM_VULKAN_BUFFERS      4   // Kept track of with mSwapchainCurrentIdx

// Comment this in if you need AHB support in your app (should be part of application configuration in the future).  May interfere with profiling!
//#define ANDROID_HARDWARE_BUFFER_SUPPORT

// Forward declarations
struct ANativeWindow;

bool CheckVkError(const char* pPrefix, VkResult CheckVal);

//=============================================================================
// Enumerations
//=============================================================================
enum class RenderPassInputUsage {
    Clear,
    Load,
    DontCare
};

enum class RenderPassOutputUsage {
    Discard,
    Store,
    StoreReadOnly,
    Present,        // Pass writing to the backbuffer
    Clear           //needs exension
};

//=============================================================================
// Structures
//=============================================================================

/// Single Swapchain image
typedef struct _SwapchainBuffers
{
    VkImage         image;
    VkImageView     view;
    VkFence         fence;
    VkCommandBuffer cmdBuffer;
} SwapchainBuffers;

/// DepthBuffer memory, image and view
typedef struct _DepthInfo
{
    VkFormat                          format;
    VkImageView                       view;
    MemoryVmaAllocatedBuffer<VkImage> image;
} DepthInfo;

//=============================================================================
// Vulkan
//=============================================================================
class Vulkan
{
    Vulkan(const Vulkan&) = delete;
    Vulkan& operator=(const Vulkan&) = delete;
public:
    Vulkan();
    virtual ~Vulkan();

    struct AppConfiguration
    {
        /// (optional) override of the priority used to initialize the async queue (assuming VK_EXT_global_priority is available and loaded)
        std::optional<VkQueueGlobalPriorityEXT> AsyncQueuePriority;
        /// (optional) override of the framebuffer depth format.  Setting to VK_FORMAT_UNDEFINED will disable the creation of a depth buffer during InitSwapChain
        std::optional<VkFormat> SwapchainDepthFormat;
    };

    typedef std::function<int(tcb::span<const VkSurfaceFormatKHR>)> tSelectSurfaceFormatFn;
    typedef std::function<void(AppConfiguration&)> tConfigurationFn;
    bool Init(uintptr_t hWnd, uintptr_t hInst, int iDesiredMSAA, const tSelectSurfaceFormatFn& SelectSurfaceFormatFn = nullptr, const tConfigurationFn& CustomConfigurationFn = nullptr);

    void Terminate();

    bool ChangeSurfaceFormat(VkSurfaceFormatKHR newSurfaceFormat);

    /// Get the next image to render to, then queue a wait until the image is ready
	/// @return index of next backbuffer.  May not be in sequential order!
    uint32_t SetNextBackBuffer();


    /// Run vkSubmitQueue for the given command buffer(s)
    bool QueueSubmit( const tcb::span<const VkCommandBuffer> CommandBuffers, const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, bool UseComputeCmdPool );
    /// Run vkSubmitQueue for the given command buffer(s) (simplified helper)
    bool QueueSubmit( VkCommandBuffer CommandBuffer, VkSemaphore WaitSemaphore, VkPipelineStageFlags WaitDstStageMask, VkSemaphore SignalSemaphore, bool UseComputeCmdPool )
    {
        auto WaitSemaphores = (WaitSemaphore!=VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &WaitSemaphore, 1 ) : tcb::span<VkSemaphore>();
        auto SignalSemaphores = (SignalSemaphore!=VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &SignalSemaphore, 1 ) : tcb::span<VkSemaphore>();
        return QueueSubmit( { &CommandBuffer,1 }, WaitSemaphores, { &WaitDstStageMask, 1 }, SignalSemaphores, UseComputeCmdPool );
    }

    /// Run the vkQueuePresentKHR
    bool PresentQueue(const tcb::span<const VkSemaphore> pWaitSemaphores);
    /// Run the vkQueuePresentKHR (simplified helper)
    bool PresentQueue( VkSemaphore WaitSemaphore )
    {
        auto WaitSemaphores = (WaitSemaphore!=VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &WaitSemaphore, 1 ) : tcb::span<VkSemaphore>();
        return PresentQueue( WaitSemaphores );
    }

    VkCommandBuffer StartSetupCommandBuffer();
    void FinishSetupCommandBuffer(VkCommandBuffer setupCmdBuffer);

    VkCompositeAlphaFlagBitsKHR     GetBestVulkanCompositeAlpha();
    /// @brief return the supported depth format with the highest precision depth/stencil supported with optimal tiling
    /// @param NeedStencil set if we have to have a format with stencil bits (defaulted to false).
    VkFormat                        GetBestVulkanDepthFormat(bool NeedStencil = false);
    /// @return true if the given format is a depth stencil format (must have stencil), otherwise return false
    static bool FormatHasStencil( VkFormat );
    /// @return true if the given format is a depth buffer format (may or may not have stencil), otherwise return false
    static bool FormatHasDepth( VkFormat );
    /// @return true if the given format is a block compressed format, otherwise return false
    static bool FormatIsCompressed( VkFormat );
    /// Check if the Vulkan device supports the given texture format.
    /// Typically used to check block compression formats when loading images.
    /// @return true if format is supported
    bool IsTextureFormatSupported( VkFormat ) const;

    void SetImageLayout(VkImage image,
        VkCommandBuffer cmdBuffer,
        VkImageAspectFlags aspect,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlags dstMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        uint32_t mipLevel = 0,
        uint32_t mipLevelCount = 1,
        uint32_t baseLayer = 0,
        uint32_t layerCount = 1);

    /// @brief Create a VkRenderPass (single subpass) with the given parameters. 
    /// @param ColorFormats Format and usage of all expected color buffers.
    /// @param DepthFormat Format and usage of the depth buffer.
    /// @param Msaa antialiasing for all the color/depth buffers
    /// @param ShouldClearDepth true if the renderpass should clear the depth buffer before use
    /// @param pRenderPass output
    /// @return 
    bool CreateRenderPass(
        tcb::span<const VkFormat> ColorFormats,
        VkFormat DepthFormat,
        VkSampleCountFlagBits Msaa,
        RenderPassInputUsage ColorInputUsage,
        RenderPassOutputUsage ColorOutputUsage,
        bool ShouldClearDepth,
        RenderPassOutputUsage DepthOutputUsage,
        VkRenderPass* pRenderPass/*out*/);

    /// @brief Create a render pass pipeline.
    /// @param pipelineCache (optional) vulkan pipeline cache
    /// @param visci (required) vertex input state
    /// @param pipelineLayout (required) Vulkan pipeline layout
    /// @param renderPass (required) render pass to make this pipeline for
    /// @param subpass (required) subpass number (0 if first subpass or not using subpasses)
    /// @param providedRS (optional) rasterization state
    /// @param providedDSS (optional) depth stencil state
    /// @return true on success
    bool CreatePipeline(
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
        VkPipeline*                             pipeline);
    bool CreateComputePipeline(
        VkPipelineCache  pipelineCache,
        VkPipelineLayout pipelineLayout,
        VkShaderModule   computeModule,
        VkPipeline* pipeline);

    bool SetSwapchainHrdMetadata(const VkHdrMetadataEXT& RenderingHdrMetaData);

    // Accessors
    MemoryManager& GetMemoryManager() { return m_MemoryManager; }
    const MemoryManager& GetMemoryManager() const { return m_MemoryManager; }
    VkInstance GetVulkanInstance() const { return m_VulkanInstance; }
    uint32_t GetVulkanQueueIndx() const { return m_VulkanGraphicsQueueIndx; }
    bool GetExtGlobalPriorityAvailable() const { return m_ExtGlobalPriorityAvailable; }
    bool GetExtRenderPassShaderResolveAvailable() const { return m_ExtRenderPassShaderResolveAvailable; }
    bool GetExtRenderPassTransformAvailable() const { return m_ExtRenderPassTransformAvailable; }
    const std::vector<VkSurfaceFormatKHR>& GetVulkanSurfaceFormats() const { return m_SurfaceFormats; }

    bool FillRenderPassTransformBeginInfoQCOM(VkRenderPassTransformBeginInfoQCOM& ) const;
    bool FillCommandBufferInheritanceRenderPassTransformInfoQCOM(VkCommandBufferInheritanceRenderPassTransformInfoQCOM& ) const;

    bool SetDebugObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name );
    bool SetDebugObjectName( VkCommandBuffer cmdBuffer, const char* name )
    {
        return SetDebugObjectName( (uint64_t)cmdBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name );
    }
    bool SetDebugObjectName( VkImage image, const char* name )
    {
        return SetDebugObjectName( (uint64_t)image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name );
    }

    // Static helpers
    static const char* VulkanFormatString(VkFormat WhichFormat);
    static const char* VulkanColorSpaceString(VkColorSpaceKHR ColorSpace);

private:
    // Vulkan takes a huge amount of code to initialize :)
    // Break up into multiple smaller functions.
    bool CreateInstance();
    void InitInstanceExtensions();
    bool GetPhysicalDevices();
    void InitDeviceExtensions();
    bool InitQueue();
    bool InitInstanceFunctions();

#ifdef USES_VULKAN_DEBUG_LAYERS
    bool InitDebugCallback();
    bool ReleaseDebugCallback();
#endif // USES_VULKAN_DEBUG_LAYERS

    bool InitSurface();
    bool InitCompute();
    bool InitDevice();
    bool InitSyncElements();
    bool InitCommandPools();
    bool InitMemoryManager();
    bool InitSwapChain();
    bool InitCommandBuffers();
    bool InitRenderPass();
    bool InitFrameBuffers();

    void DestroySwapChain();
    void DestroyCommandBuffers();
    void DestroyRenderPass();
    void DestroyFrameBuffers();

public:
    uint32_t                m_RenderWidth;
    uint32_t                m_RenderHeight;
    uint32_t                m_SurfaceWidth;
    uint32_t                m_SurfaceHeight;

    // Public Vulkan objects
    VkDevice                m_VulkanDevice;
    VkQueue                 m_VulkanQueue;
    VkCommandPool           m_VulkanCmdPool;
    VkQueue                 m_VulkanAsyncComputeQueue;
    VkCommandPool           m_VulkanAsyncComputeCmdPool;

    VkPhysicalDevice        m_VulkanGpu;

    VkSemaphore             m_BackBufferSemaphore;
    VkSemaphore             m_RenderCompleteSemaphore;
    VkFence                 m_Fence;

    // Swapchain is really the back buffers.  Anything else is a render target
    uint32_t                m_SwapchainImageCount;
    VkSwapchainKHR          m_VulkanSwapchain;

    std::vector<SwapchainBuffers> m_pSwapchainBuffers;
    DepthInfo               m_SwapchainDepth;       // ... but they all use the same depth

    VkRenderPass            m_SwapchainRenderPass;
    std::vector<VkFramebuffer> m_pSwapchainFrameBuffers;

    VkFormat                m_SurfaceFormat;        // Current surface format
    VkColorSpaceKHR         m_SurfaceColorSpace;    // Current surface colorspace
    bool                    m_UseRenderPassTransform;     // If set attempt to disable the surfaceflinger doing backbuffer rotation, assumes the application will render in the device's default orientation

private:
#if defined(OS_WINDOWS)
    HINSTANCE               m_hInstance;
    HWND                    m_hWnd;
#elif defined(OS_ANDROID)
    ANativeWindow*           m_pAndroidWindow;
#endif // defined(OS_WINDOWS)

    /// App driven configuration overrides (setup by app before Vulkan is initialized in order to potentially override default Vulkan configuration settings)
    AppConfiguration        m_ConfigOverride;

    /// Current backbuffer index (from vkAcquireNextImageKHR - may be 'out of order')
    uint32_t                m_SwapchainPresentIndx;
    /// Current frame index (internal - always in order)
    uint32_t                m_SwapchainCurrentIndx;

    // Debug/Validation Layers
    uint32_t                m_NumInstanceLayerProps;
    VkLayerProperties*      m_pInstanceLayerProps;

    uint32_t                m_NumInstanceExtensionProps;
    VkExtensionProperties*  m_pInstanceExtensionProps;

    uint32_t                m_NumInstanceLayers;
    char*                   m_pInstanceLayerNames[MAX_VALIDATION_LAYERS];

    uint32_t                m_NumInstanceExtensions;
    char*                   m_pInstanceExtensionNames[MAX_VALIDATION_LAYERS];

    // Vulkan Objects
    VkInstance                          m_VulkanInstance;
    uint32_t                            m_VulkanGpuCount;
    VkPhysicalDeviceFeatures2           m_VulkanGpuFeatures;
    VkPhysicalDeviceProperties          m_VulkanGpuProps;
    VkPhysicalDeviceMemoryProperties    m_PhysicalDeviceMemoryProperties;

    uint32_t                            m_NumDeviceLayerProps = 0;
    VkLayerProperties*                  m_pDeviceLayerProps = NULL;

    uint32_t                            m_NumDeviceExtensionProps = 0;
    VkExtensionProperties*              m_pDeviceExtensionProps = NULL;

    uint32_t                            m_NumDeviceLayers = 0;
    char*                               m_pDeviceLayerNames[MAX_VALIDATION_LAYERS];

    uint32_t                            m_NumDeviceExtensions = 0;
    const char*                         m_pDeviceExtensionNames[MAX_VALIDATION_LAYERS];

    bool                                m_LayerKhronosValidationAvailable;
    bool                                m_ExtDebugMarkerAvailable;
    bool                                m_ExtGlobalPriorityAvailable;
    bool                                m_ExtSwapchainColorspaceAvailable;
    bool                                m_ExtHdrMetadataAvailable;
    bool                                m_ExtSampleLocationsAvailable;
    bool                                m_ExtSurfaceCapabilities2Available;
    bool                                m_ExtRenderPassTransformAvailable;
    bool                                m_ExtRenderPassTransformEnabled;    // Set when the device is using the renderpasstransform extension (ie display pre-rotation is happening and m_ExtRenderPassTransformAvailable)
    bool                                m_ExtRenderPassTransformLegacy;     // Use the 'legacy' interface to VK_QCOM_render_pass_transform (older drivers)
    bool                                m_ExtRenderPassShaderResolveAvailable;

#if defined (OS_ANDROID)
    bool                                m_ExtExternMemoryCapsAvailable;
    bool                                m_ExtAndroidExternalMemoryAvailable;
#endif // defined (OS_ANDROID)

    uint32_t                            m_VulkanQueueCount;
    int                                 m_VulkanGraphicsQueueIndx;
    bool                                m_VulkanGraphicsQueueSupportsCompute;
    int                                 m_VulkanAsyncComputeQueueIndx;
    VkQueueFamilyProperties*            m_pVulkanQueueProps;

    std::vector<VkSurfaceFormatKHR>     m_SurfaceFormats;       // Available formats
    VkSurfaceKHR                        m_VulkanSurface;        // Current surface format format
    VkSurfaceCapabilitiesKHR            m_VulkanSurfaceCaps;    // Capabilities of current surface format
    VkSurfaceTransformFlagBitsKHR       m_SwapchainPreTransform;// Current swapchain pre-transform

    VkPhysicalDeviceMemoryProperties    m_MemoryProperties;

    MemoryManager                       m_MemoryManager;

    VkCommandBuffer                     m_SetupCmdBuffer;
};
