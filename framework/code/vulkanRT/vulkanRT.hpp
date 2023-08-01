//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "vulkan/vulkan.hpp"
#include <array>

// forward declarations
class Vulkan;
template<typename T_GFXAPI> struct Uniform;
namespace ExtensionHelperRT
{
    struct Ext_VK_KHR_buffer_device_address;
    struct Ext_VK_KHR_ray_tracing_pipeline;
};

#define VULKAN_RT_USE_STUB (0)
#if !VULKAN_RT_USE_STUB

class VulkanRT
{
    VulkanRT(const VulkanRT&) = delete;
    VulkanRT& operator=(const VulkanRT&) = delete;
public:
    VulkanRT(Vulkan&);
    ~VulkanRT();

    /// @brief Initialize this class (prepare for use)
    /// @return true on success
    bool Init();

    /// Return the Vulkan wrapper class that this VulkanRT was created with
    const Vulkan& GetVulkan() const { return m_vulkan; }
    Vulkan& GetVulkan() { return m_vulkan; }

    /// Add the Vulkan extensions required for ray tracing/query to the application config structure.
    /// Must be called before Init.
    /// @param rayQueryOnly if true we are only requesting the extensions needed for ray queries (false for ray tracing and queries)
    void RegisterRequiredVulkanLayerExtensions( Vulkan::AppConfiguration& appConfig, bool rayQueryOnly);

    /// Return if we were able to initialize the ray pipeline extension (always false until affter Init called and always false if RegisterRequiredVulkanLayerExtensions was called with rayQueryOnly==true)
    bool HasRayPipelines() const;

    /// Call down in to the Vulkan vkGetAccelerationStructureBuildSizesKHR function
    void vkGetAccelerationStructureBuildSizesKHR(VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) const;

    /// Call down in to the Vulkan vkCmdBuildAccelerationStructuresKHR function
    void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppOffsetInfos) const;

    /// Call down in to the Vulkan vkCmdCopyAccelerationStructureKHR function
    void vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) const;

    /// Call down in to the Vulkan vkGetAccelerationStructureDeviceAddressKHR function
    VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkAccelerationStructureKHR) const;

    /// Call down in to the Vulkan vkCreateAccelerationStructureKHR function
    VkAccelerationStructureKHR vkCreateAccelerationStructureKHR(const VkAccelerationStructureCreateInfoKHR&) const;

    /// Call down in to the Vulkan vkDestroyAccelerationStructureKHR function
    void vkDestroyAccelerationStructureKHR(VkAccelerationStructureKHR) const;

    /// @brief  Call down into the Vulkan vkCmdTraceRaysKHR functrion
    void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) const;

    /// Create a pipeline to run the Ray (tracing) Pipeline shaders.
    /// Also optionally builds the corresponding shader binding table buffer
    bool CreateRTPipeline(VkPipelineCache  pipelineCache, VkPipelineLayout pipelineLayout, VkShaderModule rayGenerationModule, VkShaderModule rayClosestHitModule, VkShaderModule rayAnyHitModule, VkShaderModule rayMissModule, const VkSpecializationInfo* specializationInfo, uint32_t maxRayRecursionDepth, VkPipeline& pipeline/*out*/, Uniform<Vulkan>& shaderBindingTableBuffer/*out*/, std::array< VkStridedDeviceAddressRegionKHR, 4>& shaderBindingTableAddresses/*out*/);

    /// Destroy the VkPipeline created with @CreateRTPipeline
    void DestroyRTPipeline(VkPipeline pipeline);

private:
    Vulkan& m_vulkan;

    const ExtensionHelperRT::Ext_VK_KHR_buffer_device_address* m_pExtKhrBufferDeviceAddress = nullptr;  ///< owned by Vulkan
    const ExtensionHelperRT::Ext_VK_KHR_ray_tracing_pipeline* m_pExtKhrRayTracingPipeline = nullptr;  ///< owned by Vulkan

    PFN_vkGetAccelerationStructureBuildSizesKHR m_fpGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR m_fpCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR m_fpDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR m_fpCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR m_fpCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR m_fpGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR m_fpCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR m_fpGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR m_fpCmdTraceRaysKHR = nullptr;
};


#endif // VULKAN_RT_USE_STUB


class VulkanRTStub
{
    VulkanRTStub( const VulkanRTStub& ) = delete;
    VulkanRTStub& operator=( const VulkanRTStub& ) = delete;
public:
    VulkanRTStub( Vulkan& );
    ~VulkanRTStub();

    /// @brief Initialize this class (prepare for use)
    /// @return true on success
    bool Init();

    /// Return the Vulkan wrapper class that this VulkanRTStub was created with
    const Vulkan& GetVulkan() const { return m_vulkan; }
    Vulkan& GetVulkan() { return m_vulkan; }

    /// Add the Vulkan extensions required for ray tracing/query to the application config structure.
    /// Since this is the 'stub' version of VulkanRT we dont add any Ray tracing/query/acceleration-structure specify extensions!
    /// Must be called before Init.
    /// @param rayQueryOnly if true we are only requesting the extensions needed for ray queries (false for ray tracing and queries)
    void RegisterRequiredVulkanLayerExtensions( Vulkan::AppConfiguration& appConfig, bool rayQueryOnly );

    // Since this is the stub lets always return true so we can make stubbed ray tracing pipelines
    bool HasRayPipelines() const { return true; }

    /// Call down in to the Vulkan vkGetAccelerationStructureBuildSizesKHR function
    void vkGetAccelerationStructureBuildSizesKHR( VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo ) const;

    /// Call down in to the Vulkan vkCmdBuildAccelerationStructuresKHR function
    void vkCmdBuildAccelerationStructuresKHR( VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppOffsetInfos ) const;

    /// Call down in to the Vulkan vkCmdCopyAccelerationStructureKHR function
    void vkCmdCopyAccelerationStructureKHR( VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo ) const;

    /// Call down in to the Vulkan vkGetAccelerationStructureDeviceAddressKHR function
    VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR( VkAccelerationStructureKHR ) const;

    /// Call down in to the Vulkan vkCreateAccelerationStructureKHR function
    VkAccelerationStructureKHR vkCreateAccelerationStructureKHR( const VkAccelerationStructureCreateInfoKHR& ) const;

    /// Call down in to the Vulkan vkDestroyAccelerationStructureKHR function
    void vkDestroyAccelerationStructureKHR( VkAccelerationStructureKHR ) const;

    /// @brief  Call down into the Vulkan vkCmdTraceRaysKHR functrion
    void vkCmdTraceRaysKHR( VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth ) const;

    /// Create a pipeline to run the Ray (tracing) Pipeline shaders.
    /// Also optionally builds the corresponding shader binding table buffer
    bool CreateRTPipeline( VkPipelineCache  pipelineCache, VkPipelineLayout pipelineLayout, VkShaderModule rayGenerationModule, VkShaderModule rayClosestHitModule, VkShaderModule rayAnyHitModule, VkShaderModule rayMissModule, uint32_t maxRayRecursionDepth, VkPipeline& pipeline/*out*/, Uniform<Vulkan>& shaderBindingTableBuffer/*out*/, std::array< VkStridedDeviceAddressRegionKHR, 4>& shaderBindingTableAddresses/*out*/ );

    /// Destroy the VkPipeline created with @CreateRTPipeline
    void DestroyRTPipeline( VkPipeline pipeline );

private:
    Vulkan& m_vulkan;
    mutable ptrdiff_t m_nextAccelerationStructureId = 1;
};


#if VULKAN_RT_USE_STUB

class VulkanRT : public VulkanRTStub {};

#endif // VULKAN_RT_USE_STUB
