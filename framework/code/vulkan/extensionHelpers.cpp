//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "vulkan.hpp"
#include "extensionHelpers.hpp"
#include "system/os_common.h"
#include <cassert>

void VulkanFunctionPointerExtensionHelper::Register(Vulkan& vulkan)
{
    vulkan.AddExtensionHooks( &m_InstanceFunctionPointerHook );
    vulkan.AddExtensionHooks( &m_DeviceFunctionPointerHook );
}


namespace ExtensionHelper {


#if VK_KHR_shader_float16_int8
    void Ext_VK_KHR_shader_float16_int8::PrintFeatures() const
    {
        LOGI( "FeaturesShaderFloat16Int8: " );
        LOGI( "    shaderFloat16: %s", this->AvailableFeatures.shaderFloat16 ? "True" : "False" );
        LOGI( "    shaderInt8: %s", this->AvailableFeatures.shaderInt8 ? "True" : "False" );
    }
#endif // VK_KHR_shader_float16_int8

#if VK_EXT_index_type_uint8
    void Ext_VK_EXT_index_type_uint8::PrintFeatures() const
    {
        LOGI( "VK_EXT_index_type_uint8 (VkPhysicalDeviceIndexTypeUint8FeaturesEXT): " );
        LOGI( "    indexTypeUint8: %s", this->AvailableFeatures.indexTypeUint8 ? "True" : "False" );
    }
#endif // VK_EXT_index_type_uint8

#if VK_KHR_shader_subgroup_extended_types
    void Ext_VK_KHR_shader_subgroup_extended_types::PrintFeatures() const
    {
        LOGI( "FeaturesShaderSubgroupExtendedTypes: " );
        LOGI( "    shaderSubgroupExtendedTypes: %s", this->AvailableFeatures.shaderSubgroupExtendedTypes ? "True" : "False" );
    }
#endif // VK_KHR_shader_subgroup_extended_types

#if VK_EXT_descriptor_indexing
    void Ext_VK_EXT_descriptor_indexing::PrintFeatures() const
    {
        LOGI( "VK_EXT_descriptor_indexing (VkPhysicalDeviceDescriptorIndexingFeatures): " );
        LOGI( "    shaderInputAttachmentArrayDynamicIndexing: %s", this->AvailableFeatures.shaderInputAttachmentArrayDynamicIndexing ? "True" : "False" );
        LOGI( "    shaderUniformTexelBufferArrayDynamicIndexing: %s", this->AvailableFeatures.shaderUniformTexelBufferArrayDynamicIndexing ? "True" : "False" );
        LOGI( "    shaderStorageTexelBufferArrayDynamicIndexing: %s", this->AvailableFeatures.shaderStorageTexelBufferArrayDynamicIndexing ? "True" : "False" );
        LOGI( "    shaderUniformBufferArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderUniformBufferArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    shaderSampledImageArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderSampledImageArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    shaderStorageBufferArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderStorageBufferArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    shaderStorageImageArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderStorageImageArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    shaderInputAttachmentArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderInputAttachmentArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    shaderUniformTexelBufferArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderUniformTexelBufferArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    shaderStorageTexelBufferArrayNonUniformIndexing: %s", this->AvailableFeatures.shaderStorageTexelBufferArrayNonUniformIndexing ? "True" : "False" );
        LOGI( "    descriptorBindingUniformBufferUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingUniformBufferUpdateAfterBind ? "True" : "False" );
        LOGI( "    descriptorBindingSampledImageUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingSampledImageUpdateAfterBind ? "True" : "False" );
        LOGI( "    descriptorBindingStorageImageUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingStorageImageUpdateAfterBind ? "True" : "False" );
        LOGI( "    descriptorBindingStorageBufferUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingStorageBufferUpdateAfterBind ? "True" : "False" );
        LOGI( "    descriptorBindingUniformTexelBufferUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind ? "True" : "False" );
        LOGI( "    descriptorBindingStorageTexelBufferUpdateAfterBind: %s", this->AvailableFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind ? "True" : "False" );
        LOGI( "    descriptorBindingUpdateUnusedWhilePending: %s", this->AvailableFeatures.descriptorBindingUpdateUnusedWhilePending ? "True" : "False" );
        LOGI( "    descriptorBindingPartiallyBound: %s", this->AvailableFeatures.descriptorBindingPartiallyBound ? "True" : "False" );
        LOGI( "    descriptorBindingVariableDescriptorCount: %s", this->AvailableFeatures.descriptorBindingVariableDescriptorCount ? "True" : "False" );
        LOGI( "    runtimeDescriptorArray: %s", this->AvailableFeatures.runtimeDescriptorArray ? "True" : "False" );
    }

    void Ext_VK_EXT_descriptor_indexing::PrintProperties() const
    {
        LOGI( "VK_EXT_descriptor_indexing (VkPhysicalDeviceDescriptorIndexingProperties): " );
        LOGI( "    maxUpdateAfterBindDescriptorsInAllPools: %u", Properties.maxUpdateAfterBindDescriptorsInAllPools );
        LOGI( "    shaderUniformBufferArrayNonUniformIndexingNative: %s", Properties.shaderUniformBufferArrayNonUniformIndexingNative ? "True" : "False" );
        LOGI( "    shaderSampledImageArrayNonUniformIndexingNative: %s", Properties.shaderSampledImageArrayNonUniformIndexingNative ? "True" : "False" );
        LOGI( "    shaderStorageBufferArrayNonUniformIndexingNative: %s", Properties.shaderStorageBufferArrayNonUniformIndexingNative ? "True" : "False" );
        LOGI( "    shaderStorageImageArrayNonUniformIndexingNative: %s", Properties.shaderStorageImageArrayNonUniformIndexingNative ? "True" : "False" );
        LOGI( "    shaderInputAttachmentArrayNonUniformIndexingNative: %s", Properties.shaderInputAttachmentArrayNonUniformIndexingNative ? "True" : "False" );
        LOGI( "    robustBufferAccessUpdateAfterBind: %s", Properties.robustBufferAccessUpdateAfterBind ? "True" : "False" );
        LOGI( "    quadDivergentImplicitLod: %s", Properties.quadDivergentImplicitLod ? "True" : "False" );
        LOGI( "    maxPerStageDescriptorUpdateAfterBindSamplers: %u", Properties.maxPerStageDescriptorUpdateAfterBindSamplers );
        LOGI( "    maxPerStageDescriptorUpdateAfterBindUniformBuffers: %u", Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers );
        LOGI( "    maxPerStageDescriptorUpdateAfterBindStorageBuffers: %u", Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers );
        LOGI( "    maxPerStageDescriptorUpdateAfterBindSampledImages: %u", Properties.maxPerStageDescriptorUpdateAfterBindSampledImages );
        LOGI( "    maxPerStageDescriptorUpdateAfterBindStorageImages: %u", Properties.maxPerStageDescriptorUpdateAfterBindStorageImages );
        LOGI( "    maxPerStageDescriptorUpdateAfterBindInputAttachments: %u", Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments );
        LOGI( "    maxPerStageUpdateAfterBindResources: %u", Properties.maxPerStageUpdateAfterBindResources );
        LOGI( "    maxDescriptorSetUpdateAfterBindSamplers: %u", Properties.maxDescriptorSetUpdateAfterBindSamplers );
        LOGI( "    maxDescriptorSetUpdateAfterBindUniformBuffers: %u", Properties.maxDescriptorSetUpdateAfterBindUniformBuffers );
        LOGI( "    maxDescriptorSetUpdateAfterBindUniformBuffersDynamic: %u", Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic );
        LOGI( "    maxDescriptorSetUpdateAfterBindStorageBuffers: %u", Properties.maxDescriptorSetUpdateAfterBindStorageBuffers );
        LOGI( "    maxDescriptorSetUpdateAfterBindStorageBuffersDynamic: %u", Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic );
        LOGI( "    maxDescriptorSetUpdateAfterBindSampledImages: %u", Properties.maxDescriptorSetUpdateAfterBindSampledImages );
        LOGI( "    maxDescriptorSetUpdateAfterBindStorageImages: %u", Properties.maxDescriptorSetUpdateAfterBindStorageImages );
        LOGI( "    maxDescriptorSetUpdateAfterBindInputAttachments: %u", Properties.maxDescriptorSetUpdateAfterBindInputAttachments );
    }
#endif // VK_EXT_descriptor_indexing

#if VK_KHR_8bit_storage
    void Ext_VK_KHR_8bit_storage::PrintFeatures() const
    {
        LOGI( "VK_KHR_8bit_storage (VkPhysicalDevice8BitStorageFeaturesKHR): " );
        LOGI( "    storageBuffer8BitAccess: %s", this->AvailableFeatures.storageBuffer8BitAccess ? "True" : "False" );
        LOGI( "    uniformAndStorageBuffer8BitAccess: %s", this->AvailableFeatures.uniformAndStorageBuffer8BitAccess ? "True" : "False" );
        LOGI( "    storagePushConstant8: %s", this->AvailableFeatures.storagePushConstant8 ? "True" : "False" );
    }
#endif // VK_KHR_8bit_storage

#if VK_KHR_portability_subset && VK_ENABLE_BETA_EXTENSIONS
    void Ext_VK_KHR_portability_subset::PrintFeatures() const
    {
        LOGI( "VK_KHR_portability_subset (VkPhysicalDevicePortabilitySubsetFeaturesKHR): " );
        LOGI( "    constantAlphaColorBlendFactors: %s", this->AvailableFeatures.constantAlphaColorBlendFactors ? "True" : "False" );
        LOGI( "    events: %s", this->AvailableFeatures.events ? "True" : "False" );
        LOGI( "    imageViewFormatReinterpretation: %s", this->AvailableFeatures.imageViewFormatReinterpretation ? "True" : "False" );
        LOGI( "    imageViewFormatSwizzle: %s", this->AvailableFeatures.imageViewFormatSwizzle ? "True" : "False" );
        LOGI( "    imageView2DOn3DImage: %s", this->AvailableFeatures.imageView2DOn3DImage ? "True" : "False" );
        LOGI( "    multisampleArrayImage: %s", this->AvailableFeatures.multisampleArrayImage ? "True" : "False" );
        LOGI( "    mutableComparisonSamplers: %s", this->AvailableFeatures.mutableComparisonSamplers ? "True" : "False" );
        LOGI( "    pointPolygons: %s", this->AvailableFeatures.pointPolygons ? "True" : "False" );
        LOGI( "    samplerMipLodBias: %s", this->AvailableFeatures.samplerMipLodBias ? "True" : "False" );
        LOGI( "    separateStencilMaskRef: %s", this->AvailableFeatures.separateStencilMaskRef ? "True" : "False" );
        LOGI( "    shaderSampleRateInterpolationFunctions: %s", this->AvailableFeatures.shaderSampleRateInterpolationFunctions ? "True" : "False" );
        LOGI( "    tessellationIsolines: %s", this->AvailableFeatures.tessellationIsolines ? "True" : "False" );
        LOGI( "    tessellationPointMode: %s", this->AvailableFeatures.tessellationPointMode ? "True" : "False" );
        LOGI( "    triangleFans: %s", this->AvailableFeatures.triangleFans ? "True" : "False" );
        LOGI( "    vertexAttributeAccessBeyondStride: %s", this->AvailableFeatures.vertexAttributeAccessBeyondStride ? "True" : "False" );
    }
    void Ext_VK_KHR_portability_subset::PrintProperties() const
    {
        LOGI( "VK_KHR_portability_subset (VkPhysicalDevicePortabilitySubsetPropertiesKHR): " );
        LOGI( "    minVertexInputBindingStrideAlignment: %u", this->Properties.minVertexInputBindingStrideAlignment );
    }
#endif // VK_KHR_portability_subset

#if VK_KHR_fragment_shading_rate
    void Ext_VK_KHR_fragment_shading_rate::PrintFeatures() const
    {
        LOGI( "VK_KHR_fragment_shading_rate (VkPhysicalDeviceFragmentShadingRateFeaturesKHR): " );
        LOGI( "    pipelineFragmentShadingRate: %s", this->AvailableFeatures.pipelineFragmentShadingRate ? "True" : "False" );
        LOGI( "    primitiveFragmentShadingRate: %s", this->AvailableFeatures.primitiveFragmentShadingRate ? "True" : "False" );
        LOGI( "    attachmentFragmentShadingRate: %s", this->AvailableFeatures.attachmentFragmentShadingRate ? "True" : "False" );
    }
    void Ext_VK_KHR_fragment_shading_rate::PrintProperties() const
    {
        LOGI( "VK_KHR_fragment_shading_rate (VkPhysicalDeviceFragmentShadingRatePropertiesKHR): " );
        LOGI( "    minFragmentDensityTexelSize: %ux%u", this->Properties.minFragmentShadingRateAttachmentTexelSize.width, this->Properties.minFragmentShadingRateAttachmentTexelSize.height );
        LOGI( "    maxFragmentDensityTexelSize: %ux%u", this->Properties.maxFragmentShadingRateAttachmentTexelSize.width, this->Properties.maxFragmentShadingRateAttachmentTexelSize.height );
        LOGI( "    maxFragmentShadingRateAttachmentTexelSizeAspectRatio: %u", this->Properties.maxFragmentShadingRateAttachmentTexelSizeAspectRatio );
        LOGI( "    primitiveFragmentShadingRateWithMultipleViewports: %s", this->Properties.primitiveFragmentShadingRateWithMultipleViewports ? "True" : "False" );
        LOGI( "    layeredShadingRateAttachments: %s", this->Properties.layeredShadingRateAttachments ? "True" : "False" );
        LOGI( "    fragmentShadingRateNonTrivialCombinerOps: %s", this->Properties.fragmentShadingRateNonTrivialCombinerOps ? "True" : "False" );
        LOGI( "    maxFragmentSize: %ux%u", this->Properties.maxFragmentSize.width, this->Properties.maxFragmentSize.height );
        LOGI( "    maxFragmentSizeAspectRatio: %u", this->Properties.maxFragmentSizeAspectRatio );
        LOGI( "    maxFragmentShadingRateCoverageSamples: %u", this->Properties.maxFragmentShadingRateCoverageSamples );
        LOGI( "    maxFragmentShadingRateRasterizationSamples: 0x%02x", this->Properties.maxFragmentShadingRateRasterizationSamples );
        LOGI( "    fragmentShadingRateWithShaderDepthStencilWrites: %s", this->Properties.fragmentShadingRateWithShaderDepthStencilWrites ? "True" : "False" );
        LOGI( "    fragmentShadingRateWithSampleMask: %s", this->Properties.fragmentShadingRateWithSampleMask ? "True" : "False" );
        LOGI( "    fragmentShadingRateWithShaderSampleMask: %s", this->Properties.fragmentShadingRateWithShaderSampleMask ? "True" : "False" );
        LOGI( "    fragmentShadingRateWithConservativeRasterization: %s", this->Properties.fragmentShadingRateWithConservativeRasterization ? "True" : "False" );
        LOGI( "    fragmentShadingRateWithFragmentShaderInterlock: %s", this->Properties.fragmentShadingRateWithFragmentShaderInterlock ? "True" : "False" );
        LOGI( "    fragmentShadingRateWithCustomSampleLocations: %s", this->Properties.fragmentShadingRateWithCustomSampleLocations ? "True" : "False" );
        LOGI( "    fragmentShadingRateStrictMultiplyCombiner: %s", this->Properties.fragmentShadingRateStrictMultiplyCombiner ? "True" : "False" );
    }
#endif // VK_KHR_fragment_shading_rate

#if VK_EXT_debug_utils
    bool Ext_VK_EXT_debug_utils::SetDebugUtilsObjectName( VkDevice vkDevice, uint64_t object, VkObjectType objectType, const char* name ) const
    {
        if (m_vkSetDebugUtilsObjectNameEXT == nullptr)
            return false;
        VkDebugUtilsObjectNameInfoEXT nameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = object;
        nameInfo.pObjectName = name;
        return m_vkSetDebugUtilsObjectNameEXT( vkDevice, &nameInfo ) == VK_SUCCESS;
    }
#endif // VK_EXT_debug_utils

#if VK_EXT_debug_marker
    bool Ext_VK_EXT_debug_marker::DebugMarkerSetObjectName( VkDevice vkDevice, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name ) const
    {
        if (m_vkDebugMarkerSetObjectNameEXT == nullptr)
            return false;
        VkDebugMarkerObjectNameInfoEXT markerInfo { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
        markerInfo.objectType = objectType;
        markerInfo.object = object;
        markerInfo.pObjectName = name;
        return m_vkDebugMarkerSetObjectNameEXT( vkDevice, &markerInfo ) == VK_SUCCESS;
    }
#endif // VK_EXT_debug_marker


    void Vulkan_SubgroupPropertiesHook::PrintProperties() const
    {
        LOGI( "VkPhysicalDeviceSubgroupProperties: " );
        LOGI( "    subgroupSize: %d", Properties.subgroupSize );

        std::string SupportedStages;
        static const char* SubgroupStages[] = { "Vertex", "Tessellation Control", "Tessellation Evaluation", "Geometry", "Fragment", "Compute" };
        for (int i = 0; i < sizeof( SubgroupStages ) / sizeof( *SubgroupStages ); ++i)
        {
            if ((Properties.supportedStages & (1 << i)) != 0)
            {
                SupportedStages.append( SubgroupStages[i] );
                SupportedStages.append( ", " );
            }
        }
        LOGI( "        supportedStages: %s", SupportedStages.c_str() );

        std::string SupportedOperations;
        static const char* SubgroupOperations[] = { "Basic", "Vote", "Arithmetic", "Ballot", "Shuffle", "Shuffle Relative", "Clustered", "Quad" };
        for (int i = 0; i < sizeof( SubgroupOperations ) / sizeof( *SubgroupOperations ); ++i)
        {
            if ((Properties.supportedOperations & (1 << i)) != 0)
            {
                SupportedOperations.append( SubgroupOperations[i] );
                SupportedOperations.append( ", " );
            }
        }
        LOGI( "        supportedOperations: %s", SupportedOperations.c_str() );

        LOGI( "        quadOperationsInAllStages: %s", Properties.quadOperationsInAllStages ? "True" : "False" );
    }


    void Vulkan_StorageFeaturesHook::PrintFeatures() const
    {
        LOGI( "FeaturesStorage16Bit: " );
        LOGI( "    storageBuffer16BitAccess: %s", this->AvailableFeatures.storageBuffer16BitAccess ? "True" : "False" );
        LOGI( "    uniformAndStorageBuffer16BitAccess: %s", this->AvailableFeatures.uniformAndStorageBuffer16BitAccess ? "True" : "False" );
        LOGI( "    storagePushConstant16: %s", this->AvailableFeatures.storagePushConstant16 ? "True" : "False" );
        LOGI( "    storageInputOutput16: %s", this->AvailableFeatures.storageInputOutput16 ? "True" : "False" );
    }


}; //namespace
