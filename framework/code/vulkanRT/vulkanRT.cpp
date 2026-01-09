//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "vulkanRT.hpp"
#include "extensionLibRT.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include "vulkan/extensionLib.hpp"
#include "memory/vulkan/uniform.hpp"
#include "vulkan/vulkan_support.hpp"

#if !VULKAN_RT_USE_STUB

VulkanRT::VulkanRT(Vulkan& vulkan) : m_vulkan(vulkan)
{
}

VulkanRT::~VulkanRT()
{}

bool VulkanRT::Init()
{
    // We MUST have bufferDeviceAddress for Ray Tracing (vkGetBufferDeviceAddress)
    if (m_pExtKhrBufferDeviceAddress->Status!=VulkanExtensionStatus::eLoaded || m_pExtKhrBufferDeviceAddress->RequestedFeatures.bufferDeviceAddress == VK_FALSE)
    {
        LOGE("Vulkan physical device must support bufferDeviceAddress (VK_KHR_buffer_device_address extension)");
        return false;
    }

    // Populate the function pointers for VK_KHR_acceleration_structure
    PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(m_vulkan.GetVulkanInstance(), "vkGetDeviceProcAddr");
    m_fpGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkGetAccelerationStructureBuildSizesKHR");
    if (!m_fpGetAccelerationStructureBuildSizesKHR)
        return false;
    m_fpCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkCreateAccelerationStructureKHR");
    if (!m_fpCreateAccelerationStructureKHR)
        return false;
    m_fpDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkDestroyAccelerationStructureKHR");
    if (!m_fpDestroyAccelerationStructureKHR)
        return false;
    m_fpCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkCmdBuildAccelerationStructuresKHR");
    if (!m_fpCmdBuildAccelerationStructuresKHR)
        return false;
    m_fpCmdCopyAccelerationStructureKHR = (PFN_vkCmdCopyAccelerationStructureKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkCmdCopyAccelerationStructureKHR");
    if (!m_fpCmdCopyAccelerationStructureKHR)
        return false;
    m_fpGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkGetAccelerationStructureDeviceAddressKHR");
    if (!m_fpGetAccelerationStructureDeviceAddressKHR)
        return false;
    // Will only be valid if Ray Pipeline extension enabled
    m_fpCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkCreateRayTracingPipelinesKHR");
    // Will only be valid if Ray Pipeline extension enabled
    m_fpGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkGetRayTracingShaderGroupHandlesKHR");
    // Will only be valid if Ray Pipeline extension enabled
    m_fpCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)fpGetDeviceProcAddr(m_vulkan.m_VulkanDevice, "vkCmdTraceRaysKHR");

    return true;
}

void VulkanRT::RegisterRequiredVulkanLayerExtensions( Vulkan::AppConfiguration& appConfig, bool rayQueryOnly)
{
    assert( m_pExtKhrBufferDeviceAddress == nullptr );
    assert( m_pExtKhrRayTracingPipeline == nullptr );
    m_pExtKhrBufferDeviceAddress = appConfig.RequiredExtension<ExtensionLib::Ext_VK_KHR_buffer_device_address>();
    appConfig.RequiredExtension<ExtensionLibRT::Ext_VK_KHR_acceleration_structure>();
    appConfig.RequiredExtension<ExtensionLib::Ext_VK_EXT_descriptor_indexing>();
    appConfig.RequiredExtension<ExtensionLib::Ext_VK_KHR_get_physical_device_properties2>();
    appConfig.RequiredExtension( "VK_KHR_deferred_host_operations" );
    appConfig.RequiredExtension( "VK_KHR_shader_float_controls" );
    appConfig.RequiredExtension( "VK_KHR_spirv_1_4" );
    appConfig.RequiredExtension<ExtensionLibRT::Ext_VK_KHR_ray_query>();
    if (!rayQueryOnly)
    {
#if ANDROID
        m_pExtKhrRayTracingPipeline = appConfig.OptionalExtension<ExtensionLibRT::Ext_VK_KHR_ray_tracing_pipeline>();
#else  // ANDROID
        m_pExtKhrRayTracingPipeline = appConfig.RequiredExtension<ExtensionLibRT::Ext_VK_KHR_ray_tracing_pipeline>();
#endif // ANDROID
    }
}

bool VulkanRT::HasRayPipelines() const
{
    return m_pExtKhrRayTracingPipeline != nullptr && m_pExtKhrRayTracingPipeline->Status == VulkanExtensionStatus::eLoaded;
}

void VulkanRT::vkGetAccelerationStructureBuildSizesKHR(VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) const
{
    m_fpGetAccelerationStructureBuildSizesKHR(m_vulkan.m_VulkanDevice, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

VkAccelerationStructureKHR VulkanRT::vkCreateAccelerationStructureKHR(const VkAccelerationStructureCreateInfoKHR& info) const
{
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
    m_fpCreateAccelerationStructureKHR(m_vulkan.m_VulkanDevice, &info, nullptr, &accelerationStructure);
    return accelerationStructure;
}

void VulkanRT::vkDestroyAccelerationStructureKHR(VkAccelerationStructureKHR as) const
{
    m_fpDestroyAccelerationStructureKHR(m_vulkan.m_VulkanDevice, as, nullptr);
}

void VulkanRT::vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppOffsetInfos) const
{
    // Issue a single command to build the acceleration structure
    m_fpCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppOffsetInfos);
}

void VulkanRT::vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) const
{
    // Issue a single command to build the acceleration structure
    m_fpCmdCopyAccelerationStructureKHR(commandBuffer, pInfo);
}

VkDeviceAddress VulkanRT::vkGetAccelerationStructureDeviceAddressKHR(VkAccelerationStructureKHR as) const
{
    VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = as;
    return m_fpGetAccelerationStructureDeviceAddressKHR(m_vulkan.m_VulkanDevice, &info);
}

void VulkanRT::vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) const
{
    m_fpCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

bool VulkanRT::CreateRTPipeline(VkPipelineCache  pipelineCache,
    VkPipelineLayout pipelineLayout,
    VkShaderModule   rayGenerationModule,
    VkShaderModule   rayClosestHitModule,
    VkShaderModule   rayAnyHitModule,
    VkShaderModule   rayMissModule,
    const VkSpecializationInfo* specializationInfo,
    uint32_t         maxRayRecursionDepth,
    VkPipeline&      pipeline,
    Uniform<Vulkan>& shaderBindingTableBuffer,
    std::array< VkStridedDeviceAddressRegionKHR,4>& shaderBindingTableAddresses)
{
    assert(pipeline==VK_NULL_HANDLE);

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups;
    groups.fill({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR });
    uint32_t groupCount = 0;

    std::array<VkPipelineShaderStageCreateInfo, 4> stages{ };
    std::for_each(stages.begin(), stages.end(), [&specializationInfo](auto& s) {
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s.pName = "main";
        s.pSpecializationInfo = specializationInfo;
    });
    uint32_t stageCount = 0;

    uint32_t rayGenerationCount = 0;
    uint32_t rayHitCount = 0;
    uint32_t rayMissCount = 0;
    uint32_t rayIntersectionCount = 0;
    uint32_t rayCallableCount = 0;

    if (rayGenerationModule)
    {
        assert(groupCount == 0);    // raygen shader comes first
        assert(stageCount == 0);
        groups[groupCount].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groups[groupCount].generalShader = stageCount;

        stages[stageCount].module = rayGenerationModule;
        stages[stageCount].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        ++stageCount;
        ++rayGenerationCount;
        ++groupCount;
    }
    if (rayMissModule)
    {
        groups[groupCount].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groups[groupCount].generalShader = stageCount;

        stages[stageCount].module = rayMissModule;
        stages[stageCount].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        ++stageCount;
        ++rayMissCount;
        ++groupCount;
    }
    if (rayClosestHitModule || rayAnyHitModule)
    {
        groups[groupCount].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;

        if (rayClosestHitModule)
        {
            stages[stageCount].module = rayClosestHitModule;
            stages[stageCount].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            groups[groupCount].closestHitShader = stageCount;
            ++stageCount;
            ++rayHitCount;
        }
        if (rayAnyHitModule)
        {
            stages[stageCount].module = rayAnyHitModule;
            stages[stageCount].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            groups[groupCount].anyHitShader = stageCount;
            ++stageCount;
            ++rayHitCount;
        }
        ++groupCount;
    }

    VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    if (stageCount > 0)
    {
        rayPipelineInfo.stageCount = stageCount;
        rayPipelineInfo.pStages = stages.data();
    }
    if (groupCount > 0)
    {
        rayPipelineInfo.groupCount = groupCount;
        rayPipelineInfo.pGroups = groups.data();
    }
    rayPipelineInfo.layout = pipelineLayout;
    rayPipelineInfo.maxPipelineRayRecursionDepth = maxRayRecursionDepth;

    assert(m_fpCreateRayTracingPipelinesKHR != nullptr);
    if (VK_SUCCESS != m_fpCreateRayTracingPipelinesKHR(m_vulkan.m_VulkanDevice, VK_NULL_HANDLE, pipelineCache, 1, &rayPipelineInfo, nullptr, &pipeline))
    {
        return false;
    }

    const auto& rayTracingPipelineProperties = m_pExtKhrRayTracingPipeline->Properties;

    const size_t shaderGroupHandleAlignment = rayTracingPipelineProperties.shaderGroupHandleAlignment;
    const size_t shaderGroupHandleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
    const size_t shaderGroupHandleAlignedBytes = (shaderGroupHandleSize + (shaderGroupHandleAlignment - 1)) & ~(shaderGroupHandleAlignment - 1);
    const size_t shaderGroupBaseAlignment = rayTracingPipelineProperties.shaderGroupBaseAlignment;

    {
        // Initialize the shader binding table strides and sizes (without the device address) to help us calulate the GPU ShaderBase Binding Table buffer size.

        // Ray generation shader binding
        assert(rayGenerationCount == 1);    // must have one and only one raygen

        const std::array<uint32_t, 4> shaderBindingTableShaderCounts = { { 1/*rayGenerationCount*/, rayMissCount, rayHitCount, rayCallableCount } };
        static_assert(shaderBindingTableShaderCounts.size() == std::tuple_size_v<std::remove_reference_t<decltype(shaderBindingTableAddresses)>>);

        // Populate binding table strides, sizes (aligned to group base alignment) and address offsets
        VkDeviceSize shaderBindingTableBytes = 0;
        for(auto i=0;i< shaderBindingTableAddresses.size(); ++i)
        {
            if (shaderBindingTableShaderCounts[i] > 0)
            {
                shaderBindingTableAddresses[i].size = ((shaderGroupHandleAlignedBytes * shaderBindingTableShaderCounts[i]) + shaderGroupBaseAlignment - 1) & ~(shaderGroupBaseAlignment - 1);
                shaderBindingTableAddresses[i].deviceAddress = shaderBindingTableBytes;
                shaderBindingTableBytes += shaderBindingTableAddresses[i].size;
                shaderBindingTableAddresses[i].stride = shaderGroupHandleAlignedBytes;
            }
            else
            {
                // Unused address
                shaderBindingTableAddresses[i] = { 0,0,0 };
            }
        }
        // raygen stride must equal size
        shaderBindingTableAddresses[0].stride = shaderBindingTableAddresses[0].size;

        // Get the shader group handles into a CPU buffer.
        std::vector<uint8_t> shaderHandleData(shaderGroupHandleSize* stageCount);
        assert(m_fpGetRayTracingShaderGroupHandlesKHR);
        m_fpGetRayTracingShaderGroupHandlesKHR(m_vulkan.m_VulkanDevice, pipeline, 0, stageCount, shaderHandleData.size(), shaderHandleData.data());

        // Fill in the (cpu side) shader binding table (with the correct GPU alignments and offsets).  If there was user data included in the SBT we would want to copy it here too
        std::vector<uint8_t> shaderBindingTable(shaderBindingTableBytes, 0);
        size_t srcHandleIdx = 0;

        const auto* pSrc = shaderHandleData.data();
        for (auto i = 0; i < shaderBindingTableShaderCounts.size(); ++i)
        {
            auto* pDest = &shaderBindingTable[shaderBindingTableAddresses[i].deviceAddress];
            for (uint32_t j = 0; j < shaderBindingTableShaderCounts[i]; ++j)
            {
                memcpy(pDest, pSrc, shaderGroupHandleSize);
                pDest += shaderBindingTableAddresses[i].stride; // next shader
                pSrc += shaderGroupHandleSize;
            }
        }
        assert(pSrc == shaderHandleData.data() + shaderHandleData.size());

        if (!CreateUniformBuffer(&m_vulkan, &shaderBindingTableBuffer, shaderBindingTable.size(), shaderBindingTable.data(), BufferUsageFlags::ShaderBindingTable | BufferUsageFlags::ShaderDeviceAddress))
        {
            DestroyRTPipeline(pipeline);
            pipeline = VK_NULL_HANDLE;
            return false;
        }

        // Finish up the shader binding table addresses (by including the buffer address)
        VkDeviceAddress deviceAddress = m_vulkan.GetMemoryManager().GetBufferDeviceAddress(shaderBindingTableBuffer.buf.GetVkBuffer());
        std::for_each(shaderBindingTableAddresses.begin(), shaderBindingTableAddresses.end(), [deviceAddress](auto& a) {
            if (a.size > 0)
                a.deviceAddress += deviceAddress;
        });
    }

    return true;
}

void VulkanRT::DestroyRTPipeline(VkPipeline pipeline)
{
    if (pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_vulkan.m_VulkanDevice, pipeline, nullptr);
}

#endif // VULKAN_RT_USE_STUB


VulkanRTStub::VulkanRTStub( Vulkan& vulkan) : m_vulkan(vulkan)
{}

VulkanRTStub::~VulkanRTStub()
{}

bool VulkanRTStub::Init()
{
    return true;
}

void VulkanRTStub::RegisterRequiredVulkanLayerExtensions( Vulkan::AppConfiguration& appConfig, bool rayQueryOnly )
{
    // Register a subset of what actual Ray Tracing or Ray Query needs.
    appConfig.RequiredExtension<ExtensionLib::Ext_VK_KHR_buffer_device_address>();
    appConfig.RequiredExtension<ExtensionLib::Ext_VK_EXT_descriptor_indexing>();
    appConfig.RequiredExtension( "VK_KHR_shader_float_controls" );
    appConfig.RequiredExtension( "VK_KHR_spirv_1_4" );
}

void VulkanRTStub::vkGetAccelerationStructureBuildSizesKHR( VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo ) const
{
    assert( pSizeInfo );
    pSizeInfo->accelerationStructureSize = 64;
    pSizeInfo->buildScratchSize = 64;
    pSizeInfo->updateScratchSize = 64;
}

void VulkanRTStub::vkCmdBuildAccelerationStructuresKHR( VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppOffsetInfos ) const
{}

void VulkanRTStub::vkCmdCopyAccelerationStructureKHR( VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo ) const
{}

VkDeviceAddress VulkanRTStub::vkGetAccelerationStructureDeviceAddressKHR( VkAccelerationStructureKHR accelerationStructure) const
{
    return (VkDeviceAddress) accelerationStructure /*this is SUPER agregious, but we are stubbing so this AS should never go near the driver, and it is something other than VK_NULL_HANDLE*/;
}

VkAccelerationStructureKHR VulkanRTStub::vkCreateAccelerationStructureKHR( const VkAccelerationStructureCreateInfoKHR& ) const
{
    return (VkAccelerationStructureKHR) m_nextAccelerationStructureId++/*stub, so this 'AS' should never go near the driver, and it is something other than VK_NULL_HANDLE.  Must also be unique!*/;
}

void VulkanRTStub::vkDestroyAccelerationStructureKHR( VkAccelerationStructureKHR ) const
{}

void VulkanRTStub::vkCmdTraceRaysKHR( VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth ) const
{}

bool VulkanRTStub::CreateRTPipeline( VkPipelineCache  pipelineCache, VkPipelineLayout pipelineLayout, VkShaderModule rayGenerationModule, VkShaderModule rayClosestHitModule, VkShaderModule rayAnyHitModule, VkShaderModule rayMissModule, uint32_t maxRayRecursionDepth, VkPipeline& pipeline/*out*/, Uniform<Vulkan>& shaderBindingTableBuffer/*out*/, std::array< VkStridedDeviceAddressRegionKHR, 4>& shaderBindingTableAddresses/*out*/ )
{
    pipeline = VK_NULL_HANDLE;
    ReleaseUniformBuffer( &m_vulkan, &shaderBindingTableBuffer );
    for (auto& addr : shaderBindingTableAddresses)
        addr = { 0/*deviceAddress*/, 0/*stride*/, 0/*size*/ };
    return true;
}

void VulkanRTStub::DestroyRTPipeline( VkPipeline pipeline )
{
    assert( pipeline == VK_NULL_HANDLE );
}
