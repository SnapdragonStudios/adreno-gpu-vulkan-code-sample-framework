//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
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

//#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#ifdef OS_ANDROID
#endif // OS_ANDROID

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "extension.hpp"
#include "tcb/span.hpp"
#include "memory/memoryManager.hpp"

// This should actually be defined in the makefile!
#define USES_VULKAN_DEBUG_LAYERS
// Enable the Vulkan validation layer to also flag 'best practices' (if the debug/validation layers are in use)
//#define VULKAN_VALIDATION_ENABLE_BEST_PRACTICES
// Enable the Vulkan validation layer to also flag 'syncronization' issues (if the debug/validation layers are in use)
//#define VULKAN_VALIDATION_ENABLE_SYNCHRONIZATION

#define NUM_VULKAN_BUFFERS      5   // Kept track of with mSwapchainCurrentIdx

// Comment this in if you need AHB support in your app (should be part of application configuration in the future).  May interfere with profiling!
//#define ANDROID_HARDWARE_BUFFER_SUPPORT

// Forward declarations
struct ANativeWindow;
class VulkanExtension;
struct VulkanDeviceFeaturePrint;
struct VulkanDevicePropertiesPrint;
struct VulkanInstanceFunctionPointerLookup;
struct VulkanDeviceFunctionPointerLookup;
namespace ExtensionHelper {
    struct Ext_VK_KHR_draw_indirect_count;
    struct Ext_VK_EXT_debug_utils;
    struct Ext_VK_EXT_debug_marker;
    struct Ext_VK_EXT_hdr_metadata;
    struct Ext_VK_KHR_fragment_shading_rate;
    struct Ext_VK_KHR_create_renderpass2;
    struct Vulkan_SubgroupPropertiesHook;
    struct Vulkan_StorageFeaturesHook;
};

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
    StoreTransferSrc,
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
    VkSemaphore     semaphore;
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
        /// (optional) presentation mode that this app would like.  If the requested mode is not available (or not requested) a 'fastest' default will be used.
        std::optional<VkPresentModeKHR> PresentMode;
        /// (optional) number of desired Vulkan timer queries (0 = Queries not even initialized)
        std::optional<uint32_t> NumTimerQueries;

        /// @brief Register the vulkan extension (templated) as required by this app
        /// @tparam T template class for the extension
        /// @return pointer to the registered extension (guaranteed to not go out of scope or move until Vulkan deleted)
        template<typename T>
        const T* RequiredExtension() { return AddExtension( std::make_unique<T>( VulkanExtension::eRequired ) ); }
        void RequiredExtension( const std::string& extensionName ) { AddExtension( std::make_unique<VulkanExtension>( extensionName, VulkanExtension::eRequired ) ); }

        /// @brief Register the vulkan extension (templated) as desired by this app (but optional)
        /// @tparam T template class for the extension
        /// @return pointer to the registered extension (guaranteed to not go out of scope or move until Vulkan deleted)
        template<typename T>
        const T* OptionalExtension() { return AddExtension( std::make_unique<T>( VulkanExtension::eOptional ) ); }
        const VulkanExtension* OptionalExtension( const std::string& extensionName ) { return AddExtension( std::make_unique<VulkanExtension>( extensionName, VulkanExtension::eOptional ) ); }
 
        template<typename T>
        const T* AddExtension( std::unique_ptr<T> extension ) {
            auto it = AdditionalVulkanDeviceExtensions.try_emplace( extension->Name, std::move( extension ) );
            if (!it.second)
            {
                assert( 0 );    // cannot add another extension with the same name but (potentially) a different implementation class
                return nullptr;
            }
            return static_cast<const T*>(&(*it.first->second.get()));
        }

        // specialized for VulkanExtension base class.
        template<>
        const VulkanExtension* AddExtension( std::unique_ptr<VulkanExtension> extension ) {
            auto it = AdditionalVulkanDeviceExtensions.try_emplace( extension->Name, std::move( extension ) );
            if (!it.second)
            {
                // extension already registered!  But ok because we are only adding the base type.
            }
            return &(*it.first->second.get());
        }
    protected:
        friend class Vulkan;
        /// (optional) list of 'additional' device extensions that this app requires or would like (optional).
        std::map<std::string, std::unique_ptr<VulkanExtension>> AdditionalVulkanDeviceExtensions;
    };

    typedef std::function<int(tcb::span<const VkSurfaceFormatKHR>)> tSelectSurfaceFormatFn;
    typedef std::function<void(AppConfiguration&)> tConfigurationFn;
    bool Init(uintptr_t hWnd, uintptr_t hInst, const tSelectSurfaceFormatFn& SelectSurfaceFormatFn = nullptr, const tConfigurationFn& CustomConfigurationFn = nullptr);

    void Terminate();

    /// Attempt to re initialize (already initialized) vulkan instance
    /// @return true on success.
    bool ReInit(uintptr_t windowHandle);

    /// Set surface format to given format and re-create all the dependant items.
    /// Does nothing if newSurfaceFormat is same as existing otherwise calls RecreateSwapChain.
    /// @return true on success.
    bool ChangeSurfaceFormat(VkSurfaceFormatKHR newSurfaceFormat);
    /// Rebuild the swapchain (and all dependant objects).
    /// Tears down and re-initializes swap chain, command buffers, render passes and frame buffers (owned by the Vulkan class)
    /// Waits for GPU to be idle before running.
    /// Will reset the current frame index.
    /// @return true on success.
    bool RecreateSwapChain();

    /// Current buffer index (that can be filled) and the fence that should be signalled when the GPU completes this buffer and the semaphore to wait on before starting rendering.
    struct BufferIndexAndFence
    {
        /// Current frame index (internal - always in order)
        const uint32_t      idx;
        /// Current backbuffer index (from vkAcquireNextImageKHR - may be 'out of order')
        const uint32_t      swapchainPresentIdx;
        /// Fence set when GPU completes rendering this buffer
        const VkFence       fence;
        /// Backbuffer semaphore (start of rendering pipeline needs to wait for this)
        const VkSemaphore   semaphore;
    };

    /// Get the next image to render to and the fence to signal at the end, then queue a wait until the image is ready
	/// @return index of next backbuffer and the fence to signal upon comdbuffer completion.  May not be in sequential order!
    BufferIndexAndFence SetNextBackBuffer();


    /// Run vkSubmitQueue for the given command buffer(s)
    bool QueueSubmit( const tcb::span<const VkCommandBuffer> CommandBuffers, const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, const tcb::span<const VkSemaphore> SignalSemaphores, bool UseComputeCmdPool, VkFence CompletedFence );
    /// Run vkSubmitQueue for the given command buffer(s) (simplified helper)
    bool QueueSubmit( VkCommandBuffer CommandBuffer, VkSemaphore WaitSemaphore, VkPipelineStageFlags WaitDstStageMask, VkSemaphore SignalSemaphore, bool UseComputeCmdPool, VkFence CompletedFence )
    {
        auto WaitSemaphores = (WaitSemaphore!=VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &WaitSemaphore, 1 ) : tcb::span<VkSemaphore>();
        auto SignalSemaphores = (SignalSemaphore!=VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &SignalSemaphore, 1 ) : tcb::span<VkSemaphore>();
        return QueueSubmit( { &CommandBuffer,1 }, WaitSemaphores, { &WaitDstStageMask, 1 }, SignalSemaphores, UseComputeCmdPool, CompletedFence );
    }

    /// Run the vkQueuePresentKHR
    bool PresentQueue(const tcb::span<const VkSemaphore> pWaitSemaphores, uint32_t SwapchainPresentIndx);
    /// Run the vkQueuePresentKHR (simplified helper)
    bool PresentQueue( VkSemaphore WaitSemaphore, uint32_t SwapchainPresentIndx )
    {
        auto WaitSemaphores = (WaitSemaphore!=VK_NULL_HANDLE) ? tcb::span<VkSemaphore>( &WaitSemaphore, 1 ) : tcb::span<VkSemaphore>();
        return PresentQueue( WaitSemaphores, SwapchainPresentIndx );
    }

    typedef std::function<void( uint32_t width, uint32_t height, VkFormat format, uint32_t span, const void* data)> tDumpSwapChainOutputFn;

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
    /// @return true if the given format is sRGB, return false if linear
    static bool FormatIsSrgb( VkFormat );

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
    /// When we have ResolveFormats we resolve each member of ColorFormats out to a matching ResolveFormats buffer (unless ResolveFormat is VK_FORMAT_UNDEFINED, render pass expects ColorPass + number of defined Resolve buffers - resolve buffers must be VK_SAMPLE_COUNT_1_BIT)
    /// @param ColorFormats Format and usage of all expected color buffers.
    /// @param DepthFormat Format and usage of the depth buffer.
    /// @param Msaa antialiasing for all the color/depth buffers
    /// @param ShouldClearDepth true if the renderpass should clear the depth buffer before use
    /// @param pRenderPass output
    /// @param ResolveFormats - the render pass will resolve to each of these, each entry corresponds to entry in ColorFormats - if VK_FORMAT_UNDEFINED then that color format slot is not resolved
    /// @return 
    bool CreateRenderPass(
        tcb::span<const VkFormat> ColorFormats,
        VkFormat DepthFormat,
        VkSampleCountFlagBits Msaa,
        RenderPassInputUsage ColorInputUsage,
        RenderPassOutputUsage ColorOutputUsage,
        bool ShouldClearDepth,
        RenderPassOutputUsage DepthOutputUsage,
        VkRenderPass* pRenderPass/*out*/,
        tcb::span<const VkFormat> ResolveFormats = {} );


    bool CreateRenderPassVRS(
        tcb::span<const VkFormat> ColorFormats,
        VkFormat DepthFormat,
        VkSampleCountFlagBits Msaa,
        RenderPassInputUsage ColorInputUsage,
        RenderPassOutputUsage ColorOutputUsage,
        bool ShouldClearDepth,
        RenderPassOutputUsage DepthOutputUsage,
        VkRenderPass* pRenderPass/*out*/,
        tcb::span<const VkFormat> ResolveFormats = {},
        bool hasDensityMap = false);

    /// @brief Create a VkRenderPass (two subpasses) with MSAA resolves (including shader resolves if supported).
    /// First subpass writes to the buffers described by 'InternalColorFormats'.  Those buffers are cleared before use and discarded at the end of the (entire) pass.
    /// Second subpass takes the buffers written by the first subpass as inputs and writes to buffers described by OutputColorFormats.  Those buffers are NOT cleared before use (assumed 2nd pass writes to all pixels).
    /// @param InternalColorFormats 
    /// @param OutputColorFormats 
    /// @param InternalDepthFormat format of depth buffer (is cleared on first subpass load and discarded after all subpasses)
    /// @param InternalPassMsaa antialisaing mode for internal color buffers and depth (all assumed to be the same) for each pass.  If PassMsaa[] != OutputMsaa AND UseShaderResolve the relevant render pass will attempt to use VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM.
    /// @param UseShaderResolve enable VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM for resolve pass, IF the internal and output msaa modes are different AND Vulkan has the VK_QCOM_render_pass_shader_resolve extension available.
    /// @param pRenderPass Output VkRenderPass
    /// @return true if successful
    bool Create2SubpassRenderPass( const tcb::span<const VkFormat> InternalColorFormats,
        const tcb::span<const VkFormat> OutputColorFormats,
        VkFormat InternalDepthFormat,
        const tcb::span<const VkSampleCountFlagBits> InternalPassMsaa,
        VkSampleCountFlagBits OutputMsaa,
        VkRenderPass* pRenderPass/*out*/ );
    bool Create2SubpassRenderPass( const tcb::span<const VkFormat> InternalColorFormats,
        const tcb::span<const VkFormat> OutputColorFormats,
        VkFormat InternalDepthFormat,
        VkSampleCountFlagBits InternalPassMsaa, /* same for both passes*/
        VkSampleCountFlagBits OutputMsaa,
        VkRenderPass* pRenderPass/*out*/ );

    /// @brief Create a VkRenderPass (two subpasses) with MSAA shader resolves (uses and requires shader resolve extension).
    /// First subpass writes to the buffers described by 'InternalColorFormats'.  Those buffers are cleared before use and discarded at the end of the (entire) pass.
    /// Second subpass takes the buffers written by the first subpass as inputs and writes to buffers described by OutputColorFormats.  Those buffers are NOT cleared before use (assumed 2nd pass writes to all pixels).
    /// @param InternalColorFormats 
    /// @param OutputColorFormats 
    /// @param InternalDepthFormat format of depth buffer (is cleared on first subpass load and discarded after all subpasses)
    /// @param InternalPassMsaa antialisaing mode for internal color buffers and depth (all assumed to be the same).
    /// @param OutputMsaa antialiasing mode for output of second 'shader resolve' pass.  Must be different (assumed lower) to @InternalPassMsaa
    /// @param pRenderPass Output VkRenderPass
    /// @return true if successful, false if shader resolve pass is not available, if inputs would not cause a shader resolve to be needed or if there is a Vulkan error creating the VkRenderPass
    bool CreateSubpassShaderResolveRenderPass(const tcb::span<const VkFormat> InternalColorFormats,
        const tcb::span<const VkFormat> OutputColorFormats,
        VkFormat InternalDepthFormat,
        VkSampleCountFlagBits InternalMsaa,
        VkSampleCountFlagBits OutputMsaa,
        VkRenderPass* pRenderPass/*out*/ );

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

    /// @brief Return the Vulkan pipeline cache (for the default device)
    /// Pipeline cache may be VK_NULL_HANDLE (no cache)
    VkPipelineCache GetPipelineCache() const { return m_PipelineCache; }

    bool SetSwapchainHrdMetadata(const VkHdrMetadataEXT& RenderingHdrMetaData);

    // Accessors
    MemoryManager& GetMemoryManager() { return m_MemoryManager; }
    const MemoryManager& GetMemoryManager() const { return m_MemoryManager; }
    VkInstance GetVulkanInstance() const { return m_VulkanInstance; }
    uint32_t GetVulkanQueueIndx() const { return m_VulkanGraphicsQueueIndx; }
    const auto& GetGpuProperties() const { return m_VulkanGpuProperties; }

    bool GetExtGlobalPriorityAvailable() const { return m_ExtGlobalPriorityAvailable; }
    bool GetExtRenderPassShaderResolveAvailable() const { return m_ExtRenderPassShaderResolveAvailable; }
    bool GetExtRenderPassTransformAvailable() const { return m_ExtRenderPassTransformAvailable; }
    const std::vector<VkSurfaceFormatKHR>& GetVulkanSurfaceFormats() const { return m_SurfaceFormats; }
    VkSurfaceTransformFlagBitsKHR GetPreTransform() const { return m_SwapchainPreTransform; }

    bool HasLoadedVulkanDeviceExtension(const std::string& extensionName) const; ///< Returns true if the named extension is loaded.

    bool FillRenderPassTransformBeginInfoQCOM(VkRenderPassTransformBeginInfoQCOM& ) const;
    bool FillCommandBufferInheritanceRenderPassTransformInfoQCOM(VkCommandBufferInheritanceRenderPassTransformInfoQCOM& ) const;

    bool SetDebugObjectName( uint64_t object, VkObjectType objectType, const char* name );
    bool SetDebugObjectName( VkCommandBuffer cmdBuffer, const char* name )
    {
        return SetDebugObjectName( (uint64_t)cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, name );
    }
    bool SetDebugObjectName( VkImage image, const char* name )
    {
        return SetDebugObjectName( (uint64_t)image, VK_OBJECT_TYPE_IMAGE, name );
    }
    bool SetDebugObjectName( VkRenderPass renderPass, const char* name )
    {
        return SetDebugObjectName( (uint64_t)renderPass, VK_OBJECT_TYPE_RENDER_PASS, name );
    }
    bool SetDebugObjectName( VkPipeline pipeline, const char* name )
    {
        return SetDebugObjectName( (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, name );
    }
    bool SetDebugObjectName( VkFramebuffer framebuffer, const char* name )
    {
        return SetDebugObjectName( (uint64_t) framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER, name );
    }
    bool SetDebugObjectName( VkDescriptorSet descriptorSet, const char* name )
    {
        return SetDebugObjectName( (uint64_t) descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, name );
    }
    bool SetDebugObjectName( VkShaderModule shaderModule, const char* name )
    {
        return SetDebugObjectName( (uint64_t) shaderModule, VK_OBJECT_TYPE_SHADER_MODULE, name );
    }

    // Static helpers
    static const char* VulkanFormatString(VkFormat WhichFormat);
    static const char* VulkanColorSpaceString(VkColorSpaceKHR ColorSpace);

    float GetTimeStampPeriod() 
    {
        return m_VulkanGpuProperties.Base.properties.limits.timestampPeriod;
    }

private:
    // Vulkan takes a huge amount of code to initialize :)
    // Break up into multiple smaller functions.
    bool RegisterKnownExtensions();
    bool CreateInstance();
    void InitInstanceExtensions();
    bool GetPhysicalDevices();
    bool InitDeviceExtensions();
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
    bool InitQueryPools();
    bool InitMemoryManager();
    bool InitPipelineCache();
    bool QuerySurfaceCapabilities();
    bool QuerySurfaceCapabilities(VkSurfaceCapabilitiesKHR& out);
    bool InitSwapChain();
    bool InitSwapchainRenderPass();
    bool InitFrameBuffers();

    void DestroySwapChain();
    void DestroySwapchainRenderPass();
    void DestroyFrameBuffers();

    struct PhysicalDeviceFeatures;
    struct PhysicalDeviceProperties;
    void QueryPhysicalDeviceFeatures(VkPhysicalDevice, PhysicalDeviceFeatures& featuresOut);
    void QueryPhysicalDeviceProperties(VkPhysicalDevice, const PhysicalDeviceFeatures& features, PhysicalDeviceProperties& propertiesOut);

    /// Given an array of device properties return the index of the 'best' one (ie the one we would most liklely want as our primary rendering device).
    static size_t GetBestVulkanPhysicalDeviceId(const tcb::span<const PhysicalDeviceProperties> deviceProperties);

    void DumpDeviceInfo(const tcb::span<PhysicalDeviceFeatures> DeviceFeatures, const tcb::span<PhysicalDeviceProperties> DeviceProperties);

public:
    uint32_t                m_SurfaceWidth;     ///< Swapchain width
    uint32_t                m_SurfaceHeight;    ///< Swapchain height

    // Public Vulkan objects
    VkDevice                m_VulkanDevice;
    VkQueue                 m_VulkanQueue;
    VkCommandPool           m_VulkanCmdPool;
    VkQueue                 m_VulkanComputeQueue;
    VkCommandPool           m_VulkanComputeCmdPool;
    VkQueryPool             m_VulkanQueryPool;

    VkPhysicalDevice        m_VulkanGpu;        ///< Current Vulkan GPU device being used (only one GPU is currently supported)

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
    bool                    m_UseRenderPassTransform;     // If set attempt to disable the surfaceflinger doing backbuffer rotation and to use the Qualcomm render pass transform extension, assumes the application will render in the device's default orientation
    bool                    m_UsePreTransform;      // If set attempt to disable the surfaceflinger doing backbuffer rotation without enabling the Qualcomm render pass transform extension.  Assumes the application will render to the rotated backbuffer.

    struct RegisteredExtensions
    {
        /// Template to add a VulkanExtension definition, parameters are the extension name and (optionally) status/version
        template<typename ... T>
        void AddExtension(std::string name, T ...args) {
            auto pExtension = std::make_unique<VulkanExtension>(name, std::forward<T>(args)...);
            m_Extensions.insert({ name, std::move(pExtension) });
        }
        /// Template to add a more complex VulkanExtension definition (expects a class derived from VulkanExtension)
        template<typename T, typename ... TT>
        const T* AddExtension(TT ...args) {
            auto pExtension = std::make_unique<T>( std::forward<TT>(args)... );
            auto insertIt = m_Extensions.insert({ pExtension->Name, std::move(pExtension) });
            return static_cast<T*>(insertIt.first->second.get());
        }

        const VulkanExtension* GetExtension( const std::string& extensionName ) const {
            auto it = m_Extensions.find( extensionName );
            if ( it == m_Extensions.end() )
                return nullptr;
            return it->second.get();
        }
        VulkanExtension* GetExtension( const std::string& extensionName ) {
            auto it = m_Extensions.find( extensionName );
            if ( it == m_Extensions.end() )
                return nullptr;
            return it->second.get();
        }

        template<typename T>
        const T* GetExtension() const {
            auto foundIt = m_Extensions.find( T::Name );
            if (foundIt == m_Extensions.end())
                return nullptr;
            return foundIt->second->Status == VulkanExtension::eLoaded ? (const T*) foundIt->second.get() : nullptr;
        }

        // Call once, once all the known/wanted extensions have been added.
        void RegisterAll(Vulkan& vulkan)
        {
            for (auto& extension : m_Extensions)
            {
                extension.second->Register( vulkan );
            }
        }

        // Container of all the extensions we know about (app may add to this from its config).  Our GPU device driver may not support all these known extensions.
        std::map< std::string, std::unique_ptr<VulkanExtension> > m_Extensions;
    };

    // Containers for the registered (and potentially loaded) extensions.
    RegisteredExtensions m_DeviceExtensions;                    ///< Device extensions
    RegisteredExtensions m_Vulkan11ProvidedExtensions;          ///< 'extensions' included in Vulkan 1.1 (implicitly loaded).

    // Generic extension query (will fail to compile if type T does not define static Name).
    template<typename T>
    const T* GetExtension() const {
        return m_DeviceExtensions.GetExtension(T::Name);
    }
    // Template specializations for stored extension pointers (compile time lookup).
    template<>
    const ExtensionHelper::Ext_VK_KHR_draw_indirect_count* GetExtension() const { return m_ExtKhrDrawIndirectCount; };
    template<>
    const ExtensionHelper::Ext_VK_EXT_debug_utils*  GetExtension() const { return m_ExtDebugUtils; };
    template<>
    const ExtensionHelper::Ext_VK_EXT_debug_marker* GetExtension() const { return m_ExtDebugMarker; };
    template<>
    const ExtensionHelper::Ext_VK_EXT_hdr_metadata* GetExtension() const { return m_ExtHdrMetadata; };
    template<>
    const ExtensionHelper::Vulkan_SubgroupPropertiesHook* GetExtension() const { return m_SubgroupProperties; };

    template<typename T, typename ...TT>
    void AddExtensionHooks( ExtensionHook<T>* t, TT... tt ) {
        AddExtensionHook( t );
        if constexpr (sizeof...(TT) > 1)
            AddExtensionHooks( tt... );
        else if constexpr (sizeof...(TT) == 1)
            AddExtensionHook( tt... );
    }

    // The application may be using different queues (graphics/compute/transfer) and so
    // need access to information about the queues.  The application make also be doing timer
    // queries and the timer resolution changes based on the queue.
    // Therefore, these have been moved from "private" to "public"
    int                                     m_VulkanGraphicsQueueIndx;
    bool                                    m_VulkanGraphicsQueueSupportsCompute;
    int                                     m_VulkanComputeQueueIndx;
    int                                     m_VulkanTransferQueueIndx;
    std::vector<VkQueueFamilyProperties>    m_pVulkanQueueProps;

private:
#if defined(OS_WINDOWS)
    HINSTANCE               m_hInstance;
    HWND                    m_hWnd;
#elif defined(OS_ANDROID)
    ANativeWindow*           m_pAndroidWindow;
#endif // defined(OS_WINDOWS)

    /// App driven configuration overrides (setup by app before Vulkan is initialized in order to potentially override default Vulkan configuration settings)
    AppConfiguration        m_ConfigOverride;

    /// Current frame index (internal - always in order)
    uint32_t                m_SwapchainCurrentIndx;

    // Debug/Validation Layers
    std::vector<VkLayerProperties>      m_InstanceLayerProps;
    std::vector<VkExtensionProperties>  m_InstanceExtensionProps;

    /// Layers we want to use (sorted alphabetically)
    std::vector<const char*>            m_InstanceLayerNames;
    /// Extensions we want to use (sorted alphabetically)
    std::vector<const char*>            m_InstanceExtensionNames;

    // Vulkan Objects
    VkInstance                          m_VulkanInstance;
    uint32_t                            m_VulkanGpuCount;
    uint32_t                            m_VulkanGpuIdx;

    /// Features used in call to vkCreateDevice (ie the features available AND USED for the current device).
    struct PhysicalDeviceFeatures
    {
        VkPhysicalDeviceFeatures2       Base = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    } m_VulkanGpuFeatures;

    /// Properties used in call to vkCreateDevice
    struct PhysicalDeviceProperties
    {
        VkPhysicalDeviceProperties2     Base { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    } m_VulkanGpuProperties;

    VkPhysicalDeviceMemoryProperties2   m_PhysicalDeviceMemoryProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };

    std::vector<VkLayerProperties>      m_pDeviceLayerProps;
    std::vector<const char*>            m_pDeviceLayerNames;            ///< Requested/Loaded device layers

    // Containers for handles in to VulkanExtensions that extend Vulkan structures (via pNext chain).
    // Allows for external applications to add suppoort for Vulkan extensions that are not supported by the framework by default.
    ExtensionChain<VkDeviceCreateInfo>                  m_DeviceCreateInfoExtensions;             ///< extensions that chain in to VkDeviceCreateInfo
    ExtensionChain<VkPhysicalDeviceFeatures2>           m_GetPhysicalDeviceFeaturesExtensions;    ///< extensions that chain in to VkPhysicalDeviceFeatures2
    ExtensionChain<VkPhysicalDeviceProperties2>         m_GetPhysicalDevicePropertiesExtensions;  ///< extensions that chain in to VkPhysicalDeviceProperties2
    ExtensionChain<VulkanDeviceFeaturePrint>            m_VulkanDeviceFeaturePrintExtensions;     ///< extensions that hook to VulkanDeviceFeaturePrint
    ExtensionChain<VulkanDevicePropertiesPrint>         m_VulkanDevicePropertiesPrintExtensions;  ///< extensions that hook to VulkanDevicePropertiesPrint
    ExtensionChain<VulkanInstanceFunctionPointerLookup> m_VulkanInstanceFunctionPointerLookupExtensions;  ///< extensions that hook to VulkanInstanceFunctionPointerLookup
    ExtensionChain<VulkanDeviceFunctionPointerLookup>   m_VulkanDeviceFunctionPointerLookupExtensions;  ///< extensions that hook to VulkanDeviceFunctionPointerLookup

    void AddExtensionHook( ExtensionHook< VkDeviceCreateInfo >* p ) { m_DeviceCreateInfoExtensions.AddExtensionHook( p ); }
    void AddExtensionHook( ExtensionHook< VkPhysicalDeviceFeatures2 >* p ) { m_GetPhysicalDeviceFeaturesExtensions.AddExtensionHook( p ); }
    void AddExtensionHook( ExtensionHook< VkPhysicalDeviceProperties2 >* p ) { m_GetPhysicalDevicePropertiesExtensions.AddExtensionHook( p ); }
    void AddExtensionHook( ExtensionHook< VulkanDeviceFeaturePrint >* p ) { m_VulkanDeviceFeaturePrintExtensions.AddExtensionHook( p ); }
    void AddExtensionHook( ExtensionHook< VulkanDevicePropertiesPrint >* p ) { m_VulkanDevicePropertiesPrintExtensions.AddExtensionHook( p ); }
    void AddExtensionHook( ExtensionHook< VulkanInstanceFunctionPointerLookup >* p ) { m_VulkanInstanceFunctionPointerLookupExtensions.AddExtensionHook( p ); }
    void AddExtensionHook( ExtensionHook< VulkanDeviceFunctionPointerLookup >* p ) { m_VulkanDeviceFunctionPointerLookupExtensions.AddExtensionHook( p ); }

    bool                                m_LayerKhronosValidationAvailable;
    bool                                m_ExtGlobalPriorityAvailable;
    bool                                m_ExtSwapchainColorspaceAvailable;
    bool                                m_ExtSurfaceCapabilities2Available;
    bool                                m_ExtRenderPassTransformAvailable;
    bool                                m_ExtRenderPassTransformEnabled;    ///< Set when the device is using the renderpasstransform extension (ie display pre-rotation is happening and m_ExtRenderPassTransformAvailable)
    bool                                m_ExtRenderPassTransformLegacy;     ///< Use the 'legacy' interface to VK_QCOM_render_pass_transform (older drivers)
    bool                                m_ExtRenderPassShaderResolveAvailable;
    bool                                m_ExtDebugUtilsAvailable;
    bool                                m_ExtPortability;                   ///< Vulkan Portability extension present (and so must be enabled).  Limited subset of Vulkan functionality (for backwards compatibility with other graphics api)
    uint32_t                            m_ExtValidationFeaturesVersion;     ///< Version of the VK_EXT_validation_features extension (0 validation is disabled and/or extension not loaded)

    // Extensions loaded by this class
    const ExtensionHelper::Ext_VK_KHR_draw_indirect_count* m_ExtKhrDrawIndirectCount = nullptr;
    const ExtensionHelper::Ext_VK_EXT_debug_utils*         m_ExtDebugUtils = nullptr;
    const ExtensionHelper::Ext_VK_EXT_debug_marker*        m_ExtDebugMarker = nullptr;
    const ExtensionHelper::Ext_VK_EXT_hdr_metadata*        m_ExtHdrMetadata = nullptr;
    const ExtensionHelper::Ext_VK_KHR_fragment_shading_rate* m_ExtFragmentShadingRate = nullptr;
    const ExtensionHelper::Ext_VK_KHR_create_renderpass2*  m_ExtRenderPass2 = nullptr;
    const ExtensionHelper::Vulkan_SubgroupPropertiesHook*  m_SubgroupProperties = nullptr;
    const ExtensionHelper::Vulkan_StorageFeaturesHook*     m_StorageFeatures = nullptr;

#if defined (OS_ANDROID)
    bool                                m_ExtExternMemoryCapsAvailable;
    bool                                m_ExtAndroidExternalMemoryAvailable;
#endif // defined (OS_ANDROID)

    std::vector<VkSurfaceFormatKHR>     m_SurfaceFormats;       ///< Available formats
    VkSurfaceKHR                        m_VulkanSurface;        ///< Current surface format format
    VkSurfaceCapabilitiesKHR            m_VulkanSurfaceCaps;    ///< Capabilities of current surface format
    VkSurfaceTransformFlagBitsKHR       m_SwapchainPreTransform;///< Current swapchain pre-transform

    MemoryManager                       m_MemoryManager;

    VkCommandBuffer                     m_SetupCmdBuffer;

    VkPipelineCache                     m_PipelineCache;
};
