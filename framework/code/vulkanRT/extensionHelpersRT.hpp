//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "vulkan/extensionHelpers.hpp"
#include "system/os_common.h"

class Vulkan;

///
/// Library of Vulkan extension helpers for Ray Tracing
/// 


namespace ExtensionHelperRT
{

#if VK_KHR_acceleration_structure

    struct Ext_VK_KHR_acceleration_structure : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        VkPhysicalDeviceAccelerationStructurePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR>
    {
        static constexpr auto Name = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
        Ext_VK_KHR_acceleration_structure( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override
        {
            LOGI("FeaturesAccelerationStructure: ");
            LOGI("    accelerationStructure: %s", this->AvailableFeatures.accelerationStructure ? "True" : "False");
            LOGI("    accelerationStructureCaptureReplay: %s", this->AvailableFeatures.accelerationStructureCaptureReplay ? "True" : "False");
            LOGI("    accelerationStructureIndirectBuild: %s", this->AvailableFeatures.accelerationStructureIndirectBuild ? "True" : "False");
            LOGI("    accelerationStructureHostCommands: %s", this->AvailableFeatures.accelerationStructureHostCommands ? "True" : "False");
            LOGI("    descriptorBindingAccelerationStructureUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingAccelerationStructureUpdateAfterBind ? "True" : "False");
        }
        void PopulateRequestedFeatures() override
        {
            // Enable just the 'accelerationStructure' feature
            RequestedFeatures.accelerationStructure = AvailableFeatures.accelerationStructure;
        }

        void PrintProperties() const override
        {
            LOGI("  accelerationStructure:");
            LOGI("        maxGeometryCount: %zd", Properties.maxGeometryCount);
            LOGI("        maxInstanceCount: %zd", Properties.maxInstanceCount);
            LOGI("        maxPrimitiveCount: %zd", Properties.maxPrimitiveCount);
            LOGI("        maxPerStageDescriptorAccelerationStructures: %u", Properties.maxPerStageDescriptorAccelerationStructures);
            LOGI("        maxPerStageDescriptorUpdateAfterBindAccelerationStructures: %u", Properties.maxPerStageDescriptorUpdateAfterBindAccelerationStructures);
            LOGI("        maxDescriptorSetAccelerationStructures: %u", Properties.maxDescriptorSetAccelerationStructures);
            LOGI("        maxDescriptorSetUpdateAfterBindAccelerationStructures: %u", Properties.maxDescriptorSetUpdateAfterBindAccelerationStructures);
            LOGI("        minAccelerationStructureScratchOffsetAlignment: %u", Properties.minAccelerationStructureScratchOffsetAlignment);
        }
    };

#endif // VK_KHR_acceleration_structure


#if VK_KHR_buffer_device_address

    struct Ext_VK_KHR_buffer_device_address : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceBufferDeviceAddressFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
        Ext_VK_KHR_buffer_device_address( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override
        {
            LOGI("FeaturesBufferDeviceAddress: ");
            LOGI("    bufferDeviceAddress: %s", this->AvailableFeatures.bufferDeviceAddress ? "True" : "False");
            LOGI("    bufferDeviceAddressCaptureReplay: %s", this->AvailableFeatures.bufferDeviceAddressCaptureReplay ? "True" : "False");
            LOGI("    bufferDeviceAddressMultiDevice: %s", this->AvailableFeatures.bufferDeviceAddressMultiDevice ? "True" : "False");
        }
        void PopulateRequestedFeatures() override
        {
            // Enable just the 'bufferDeviceAddress' feature
            RequestedFeatures.bufferDeviceAddress = AvailableFeatures.bufferDeviceAddress;
        }
    };

#endif // VK_KHR_buffer_device_address



#if VK_KHR_ray_tracing_pipeline

    struct Ext_VK_KHR_ray_tracing_pipeline : public VulkanDeviceFeaturePropertiesExtensionHelper<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, VkPhysicalDeviceRayTracingPipelinePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR>
    {
        static constexpr auto Name = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;
        Ext_VK_KHR_ray_tracing_pipeline( VulkanExtensionStatus status = VulkanExtensionStatus::eOptional )
            : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override
        {
            LOGI("FeaturesRayTracing: ");
            LOGI("    rayTracingPipeline: %s", this->AvailableFeatures.rayTracingPipeline ? "True" : "False");
            LOGI("    rayTracingPipelineShaderGroupHandleCaptureReplay: %s", this->AvailableFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay ? "True" : "False");
            LOGI("    rayTracingPipelineShaderGroupHandleCaptureReplayMixed: %s", this->AvailableFeatures.rayTracingPipelineShaderGroupHandleCaptureReplayMixed ? "True" : "False");
            LOGI("    rayTracingPipelineTraceRaysIndirect: %s", this->AvailableFeatures.rayTracingPipelineTraceRaysIndirect ? "True" : "False");
            LOGI("    rayTracingPrimitiveCulling: %s", this->AvailableFeatures.rayTraversalPrimitiveCulling ? "True" : "False");
        }
        void PopulateRequestedFeatures() override
        {
            // Enable just the 'rayTracingPipeline' feature
            RequestedFeatures.rayTracingPipeline = AvailableFeatures.rayTracingPipeline;
        }

        void PrintProperties() const override
        {
            LOGI("  rayTracingPipeline:");
            LOGI("        shaderGroupHandleSize: %u", Properties.shaderGroupHandleSize);
            LOGI("        maxRayRecursionDepth: %u", Properties.maxRayRecursionDepth);
            LOGI("        maxShaderGroupStride: %u", Properties.maxShaderGroupStride);
            LOGI("        shaderGroupBaseAlignment: %u", Properties.shaderGroupBaseAlignment);
            LOGI("        shaderGroupHandleCaptureReplaySize: %u", Properties.shaderGroupHandleCaptureReplaySize);
            LOGI("        maxRayDispatchInvocationCount: %u", Properties.maxRayDispatchInvocationCount);
            LOGI("        shaderGroupHandleAlignment: %u", Properties.shaderGroupHandleAlignment);
            LOGI("        maxRayHitAttributeSize: %u", Properties.maxRayHitAttributeSize);
        }

    };

#endif // VK_KHR_ray_tracing_pipeline


#if VK_KHR_ray_query

    struct Ext_VK_KHR_ray_query : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceRayQueryFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_RAY_QUERY_EXTENSION_NAME;
        Ext_VK_KHR_ray_query( VulkanExtensionStatus status = VulkanExtensionStatus::eOptional )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override
        {
            LOGI("FeaturesRayQuery: ");
            LOGI("    rayQuery: %s", this->AvailableFeatures.rayQuery ? "True" : "False");
        }
        void PopulateRequestedFeatures() override
        {
            // Enable just the 'rayQuery' feature
            RequestedFeatures.rayQuery = AvailableFeatures.rayQuery;
        }

    };

#endif // VK_KHR_ray_query


}; // namespace



