//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "extension.hpp"

class Vulkan;

struct VulkanDeviceFeaturePrint
{
    VkStructureType sType = VK_STRUCTURE_TYPE_MAX_ENUM;
    void* pNext = nullptr;
};
struct VulkanDevicePropertiesPrint
{
    VkStructureType sType = VK_STRUCTURE_TYPE_MAX_ENUM;
    void* pNext = nullptr;
};
struct VulkanInstanceFunctionPointerLookup
{
    const VkInstance vkInstance;
};
struct VulkanDeviceFunctionPointerLookup
{
    const VkDevice vkDevice;
    const PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr;
};


/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceFeatures2
/// Default behaviour assumes that the application will do a query before the vkCreateDevice and the features populated in that call are then required/desired by the vkCreateDevice.
/// Overriding 'PopulateRequestedFeatures' allows for changes to this behaviour (called when the feature 'chain' for vkCreateDevice is being constructed).
/// 
/// @tparam FeaturesStruct 
template<typename T_FEATURESSTRUCTURE, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class VulkanDeviceFeatureExtensionHelper : public VulkanExtension
{
public:
    VulkanDeviceFeatureExtensionHelper( std::string extensionName, VulkanExtension::eStatus status ) : VulkanExtension( extensionName, status ), m_DeviceCreateInfoHook( this ), m_GetPhysicalDeviceFeaturesHook( this ), m_VulkanDeviceFeaturePrintHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        vulkan.AddExtensionHooks( &m_DeviceCreateInfoHook, &m_GetPhysicalDeviceFeaturesHook, &m_VulkanDeviceFeaturePrintHook );
    }
    virtual void PrintFeatures() const = 0;

    T_FEATURESSTRUCTURE AvailableFeatures { T_FEATURESSTRUCTURE_TYPE };
    T_FEATURESSTRUCTURE RequestedFeatures { T_FEATURESSTRUCTURE_TYPE };
protected:
    virtual void PopulateRequestedFeatures() { RequestedFeatures = AvailableFeatures; }
private:
    class DeviceCreateInfoHook final : public ExtensionHook<VkDeviceCreateInfo>
    {
        friend class VulkanDeviceFeatureExtensionHelper;
        DeviceCreateInfoHook( VulkanDeviceFeatureExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
        VkBaseOutStructure* Obtain( tBase* ) override {
            Parent->PopulateRequestedFeatures();
            Parent->RequestedFeatures.pNext = nullptr;
            return (VkBaseOutStructure*) &Parent->RequestedFeatures;
        };
        VulkanDeviceFeatureExtensionHelper* Parent;
    } m_DeviceCreateInfoHook;

    class GetPhysicalDeviceFeaturesHook final : public ExtensionHook<VkPhysicalDeviceFeatures2>
    {
        friend class VulkanDeviceFeatureExtensionHelper;
        GetPhysicalDeviceFeaturesHook( VulkanDeviceFeatureExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
        VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->AvailableFeatures; };
        VulkanDeviceFeatureExtensionHelper* Parent;
    } m_GetPhysicalDeviceFeaturesHook;

    class VulkanDeviceFeaturePrintHook final : public ExtensionHook<VulkanDeviceFeaturePrint>
    {
        friend class VulkanDeviceFeatureExtensionHelper;
        VulkanDeviceFeaturePrintHook( VulkanDeviceFeatureExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
        VkBaseOutStructure* Obtain( tBase* ) override { Parent->PrintFeatures();  return nullptr; };
        VulkanDeviceFeatureExtensionHelper* Parent;
    } m_VulkanDeviceFeaturePrintHook;
};

/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceFeatures2
/// AND have properties queried through VkPhysicalDeviceProperties2.
/// Default behaviour assumes that the application will do a query before the vkCreateDevice and the features populated in that call are then required/desired by the vkCreateDevice.
/// Overriding 'PopulateRequestedFeatures' allows for changes to this behaviour (called when the feature 'chain' for vkCreateDevice is being constructed).
template<typename T_FEATURESSTRUCTURE, VkStructureType T_FEATURESSTRUCTURE_TYPE, typename T_PROPERTIESSTRUCTURE, VkStructureType T_PROPERTIESSTRUSTURE_TYPE>
class VulkanDeviceFeaturePropertiesExtensionHelper : public VulkanDeviceFeatureExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE>
{
    using tBase = VulkanDeviceFeatureExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE>;
public:
    VulkanDeviceFeaturePropertiesExtensionHelper( std::string extensionName, VulkanExtension::eStatus status )
        : tBase( extensionName, status )
        , m_GetPhysicalDevicePropertiesHook( this )
        , m_VulkanDevicePropertiesPrintHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        tBase::Register( vulkan );
        vulkan.AddExtensionHooks( &m_GetPhysicalDevicePropertiesHook, &m_VulkanDevicePropertiesPrintHook );
    }

    virtual void PrintProperties() const = 0;

    class GetPhysicalDevicePropertiesHook final : public ExtensionHook<VkPhysicalDeviceProperties2>
    {
        friend class VulkanDeviceFeaturePropertiesExtensionHelper;
        GetPhysicalDevicePropertiesHook( VulkanDeviceFeaturePropertiesExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return T_PROPERTIESSTRUSTURE_TYPE; };
        VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->Properties; };
        VulkanDeviceFeaturePropertiesExtensionHelper* Parent;
    } m_GetPhysicalDevicePropertiesHook;

    class VulkanDevicePropertiesPrintHook final : public ExtensionHook<VulkanDevicePropertiesPrint>
    {
        friend class VulkanDeviceFeaturePropertiesExtensionHelper;
        VulkanDevicePropertiesPrintHook( VulkanDeviceFeaturePropertiesExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return T_PROPERTIESSTRUSTURE_TYPE; };
        VkBaseOutStructure* Obtain( tBase* ) override { Parent->PrintProperties();  return nullptr; };
        VulkanDeviceFeaturePropertiesExtensionHelper* Parent;
    } m_VulkanDevicePropertiesPrintHook;

    T_PROPERTIESSTRUCTURE Properties { T_PROPERTIESSTRUSTURE_TYPE };
};


class VulkanFunctionPointerExtensionHelper : public VulkanExtension
{
public:
    VulkanFunctionPointerExtensionHelper( std::string extensionName, VulkanExtension::eStatus status ) : VulkanExtension( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
    {}

    void Register( Vulkan& vulkan ) override;

protected:
    virtual void LookupFunctionPointers( VkInstance ) = 0;
    virtual void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) = 0;
private:
    struct InstanceFunctionPointerHook final : public ExtensionHook<VulkanInstanceFunctionPointerLookup>
    {
        friend class VulkanFunctionPointerExtensionHelper;
        InstanceFunctionPointerHook( VulkanFunctionPointerExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return (VkStructureType) 0; };
        VkBaseOutStructure* Obtain( VulkanInstanceFunctionPointerLookup* pBase ) override {
            Parent->LookupFunctionPointers( pBase->vkInstance );
            return nullptr;
        }
        VulkanFunctionPointerExtensionHelper* Parent;
    } m_InstanceFunctionPointerHook;
    struct DeviceFunctionPointerHook final : public ExtensionHook<VulkanDeviceFunctionPointerLookup>
    {
        friend class VulkanFunctionPointerExtensionHelper;
        DeviceFunctionPointerHook( VulkanFunctionPointerExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return (VkStructureType) 0; };
        VkBaseOutStructure* Obtain( VulkanDeviceFunctionPointerLookup* pBase ) override {
            if (Parent->Status == VulkanExtension::eLoaded)
                Parent->LookupFunctionPointers( pBase->vkDevice, pBase->fpGetDeviceProcAddr );
            return nullptr;
        }
        VulkanFunctionPointerExtensionHelper* Parent;
    } m_DeviceFunctionPointerHook;
};


/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceFeatures2
/// AND have properties queried through VkPhysicalDeviceProperties2.
/// AND want function pointer lookup
template<typename T_FEATURESSTRUCTURE, VkStructureType T_FEATURESSTRUCTURE_TYPE, typename T_PROPERTIESSTRUCTURE, VkStructureType T_PROPERTIESSTRUSTURE_TYPE>
class VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper : public VulkanDeviceFeaturePropertiesExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE, T_PROPERTIESSTRUCTURE, T_PROPERTIESSTRUSTURE_TYPE>
{
    using tBase = VulkanDeviceFeaturePropertiesExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE, T_PROPERTIESSTRUCTURE, T_PROPERTIESSTRUSTURE_TYPE>;
public:
    VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper( std::string extensionName, VulkanExtension::eStatus status ) : tBase( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        tBase::Register( vulkan );
        vulkan.AddExtensionHooks( &m_InstanceFunctionPointerHook );
        vulkan.AddExtensionHooks( &m_DeviceFunctionPointerHook );
    }

protected:
    virtual void LookupFunctionPointers( VkInstance ) = 0;
    virtual void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) = 0;
private:
    class InstanceFunctionPointerHook final : public ExtensionHook<VulkanInstanceFunctionPointerLookup>
    {
        friend class VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper;
        InstanceFunctionPointerHook( VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return (VkStructureType) 0; };
        VkBaseOutStructure* Obtain( VulkanInstanceFunctionPointerLookup* pBase ) override {
            Parent->LookupFunctionPointers( pBase->vkInstance );
            return nullptr;
        }
        VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper* Parent;
    } m_InstanceFunctionPointerHook;
    class DeviceFunctionPointerHook final : public ExtensionHook<VulkanDeviceFunctionPointerLookup>
    {
        friend class VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper;
        DeviceFunctionPointerHook( VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper* _Parent ) : Parent( _Parent ) {}
        VkStructureType StructureType() const override { return (VkStructureType) 0; };
        VkBaseOutStructure* Obtain( VulkanDeviceFunctionPointerLookup* pBase ) override {
            if (Parent->Status == VulkanExtension::eLoaded)
                Parent->LookupFunctionPointers( pBase->vkDevice, pBase->fpGetDeviceProcAddr );
            return nullptr;
        }
        VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper* Parent;
    } m_DeviceFunctionPointerHook;
};


///
/// Library of Vulkan extension helpers
/// 

namespace ExtensionHelper
{

#if VK_KHR_shader_float16_int8

    struct Ext_VK_KHR_shader_float16_int8 : public VulkanDeviceFeatureExtensionHelper<VkPhysicalDeviceShaderFloat16Int8FeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME;
        Ext_VK_KHR_shader_float16_int8( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeatureExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };
    
#endif // VK_KHR_shader_float16_int8

#if VK_EXT_index_type_uint8

    struct Ext_VK_EXT_index_type_uint8 : public VulkanDeviceFeatureExtensionHelper<VkPhysicalDeviceIndexTypeUint8FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT>
    {
        static constexpr auto Name = VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME;
        Ext_VK_EXT_index_type_uint8( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeatureExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };

#endif // VK_EXT_index_type_uint8

#if VK_KHR_shader_subgroup_extended_types

    struct Ext_VK_KHR_shader_subgroup_extended_types : public VulkanDeviceFeatureExtensionHelper<VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME;
        Ext_VK_KHR_shader_subgroup_extended_types( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeatureExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };

#endif // VK_KHR_shader_subgroup_extended_types

#if VK_EXT_descriptor_indexing

    struct Ext_VK_EXT_descriptor_indexing : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDeviceDescriptorIndexingFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
        VkPhysicalDeviceDescriptorIndexingPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT>
    {
        static constexpr auto Name = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
        Ext_VK_EXT_descriptor_indexing( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
        void PrintProperties() const override;
    };

#endif // VK_EXT_descriptor_indexing

#if VK_KHR_8bit_storage

    struct Ext_VK_KHR_8bit_storage : public VulkanDeviceFeatureExtensionHelper<
        VkPhysicalDevice8BitStorageFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
        Ext_VK_KHR_8bit_storage( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeatureExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };

#endif // VK_KHR_8bit_storage

#if VK_KHR_portability_subset && VK_ENABLE_BETA_EXTENSIONS

    struct Ext_VK_KHR_portability_subset : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDevicePortabilitySubsetFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
        VkPhysicalDevicePortabilitySubsetPropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR>
    {
        static constexpr auto Name = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
        Ext_VK_KHR_portability_subset( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
        void PrintProperties() const override;
    };
#else

    // 'dummy' implementation of Ext_VK_KHR_portability_subset for when vulkan headers do not contain 'VK_KHR_portability_subset'
    struct Ext_VK_KHR_portability_subset : public VulkanExtension
    {
        static constexpr auto Name = "VK_KHR_portability_subset";
        Ext_VK_KHR_portability_subset( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanExtension( Name, status ) {}
    };

#endif // VK_KHR_portability_subset

#if VK_KHR_fragment_shading_rate
    struct Ext_VK_KHR_fragment_shading_rate : public VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR>
    {
        static constexpr auto Name = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
        Ext_VK_KHR_fragment_shading_rate( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper( Name, status )
        {}

        void PopulateRequestedFeatures() override
        {
            RequestedFeatures.sType = AvailableFeatures.sType;
            RequestedFeatures.attachmentFragmentShadingRate = AvailableFeatures.attachmentFragmentShadingRate;
        }
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkCmdSetFragmentShadingRateKHR = (PFN_vkCmdSetFragmentShadingRateKHR) vkGetInstanceProcAddr( vkInstance, "vkCmdSetFragmentShadingRateKHR" );
        }
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {}
        void PrintFeatures() const override;
        void PrintProperties() const override;
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR AvailableFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR };
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR Properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR };
        PFN_vkCmdSetFragmentShadingRateKHR m_vkCmdSetFragmentShadingRateKHR = nullptr;
    };
#endif // VK_KHR_fragment_shading_rate

#if VK_KHR_create_renderpass2
    struct Ext_VK_KHR_create_renderpass2 : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
        Ext_VK_KHR_create_renderpass2( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkCreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCreateRenderPass2KHR" );
            m_vkCmdBeginRenderPass2KHR = (PFN_vkCmdBeginRenderPass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCmdBeginRenderPass2KHR" );
            m_vkCmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCmdNextSubpass2KHR" );
            m_vkCmdEndRenderPass2KHR = (PFN_vkCmdEndRenderPass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCmdEndRenderPass2KHR" );
        }
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {}
        PFN_vkCreateRenderPass2KHR      m_vkCreateRenderPass2KHR = nullptr;
        PFN_vkCmdBeginRenderPass2KHR    m_vkCmdBeginRenderPass2KHR = nullptr;
        PFN_vkCmdNextSubpass2KHR        m_vkCmdNextSubpass2KHR = nullptr;
        PFN_vkCmdEndRenderPass2KHR      m_vkCmdEndRenderPass2KHR = nullptr;
    };
#endif // VK_KHR_create_renderpass2

#if VK_KHR_draw_indirect_count
    struct Ext_VK_KHR_draw_indirect_count : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME;
        Ext_VK_KHR_draw_indirect_count( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkCmdDrawIndirectCountKHR = (PFN_vkCmdDrawIndirectCountKHR) vkGetInstanceProcAddr( vkInstance, "vkCmdDrawIndirectCountKHR" );
            m_vkCmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR) vkGetInstanceProcAddr( vkInstance, "vkCmdDrawIndexedIndirectCountKHR" );
        }
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override {}
        PFN_vkCmdDrawIndirectCountKHR        m_vkCmdDrawIndirectCountKHR = nullptr;
        PFN_vkCmdDrawIndexedIndirectCountKHR m_vkCmdDrawIndexedIndirectCountKHR = nullptr;
    };
#endif // VK_KHR_draw_indirect_count

#if VK_EXT_hdr_metadata

    struct Ext_VK_EXT_hdr_metadata : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_EXT_HDR_METADATA_EXTENSION_NAME;
        Ext_VK_EXT_hdr_metadata(VulkanExtension::eStatus status = VulkanExtension::eRequired) : VulkanFunctionPointerExtensionHelper(Name, status) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override {}
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override
        {
            m_vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT) fpGetDeviceProcAddr( vkDevice, "vkSetHdrMetadataEXT" );
        }
        PFN_vkSetHdrMetadataEXT             m_vkSetHdrMetadataEXT = nullptr;
    };
#endif // VK_EXT_hdr_metadata

#if VK_EXT_debug_utils
    struct Ext_VK_EXT_debug_utils : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        Ext_VK_EXT_debug_utils( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetInstanceProcAddr( vkInstance, "vkSetDebugUtilsObjectNameEXT" );
        }
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override {}
        bool SetDebugUtilsObjectName( VkDevice vkDevice, uint64_t object, VkObjectType objectType, const char* name ) const;

        PFN_vkSetDebugUtilsObjectNameEXT  m_vkSetDebugUtilsObjectNameEXT = nullptr;
    };
#endif // VK_EXT_debug_utils

#if VK_EXT_debug_marker
    struct Ext_VK_EXT_debug_marker : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
        Ext_VK_EXT_debug_marker( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override {}
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override
        {
            m_vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT) fpGetDeviceProcAddr( vkDevice, "vkDebugMarkerSetObjectNameEXT" );
        }
        bool DebugMarkerSetObjectName( VkDevice vkDevice, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name ) const;

        PFN_vkDebugMarkerSetObjectNameEXT m_vkDebugMarkerSetObjectNameEXT = nullptr;
    };
#endif // Ext_VK_EXT_debug_marker

    //
    // Vulkan 1.1 (VK_VERSION_1_1) provided features/properties.
    // Same interface as other extensions but do not need to be added to the list of extension names on vkCreateDevice.
    //


    struct Vulkan_SubgroupPropertiesHook : public VulkanExtension
    {
        Vulkan_SubgroupPropertiesHook& operator=( const Vulkan_SubgroupPropertiesHook& ) = delete;
        Vulkan_SubgroupPropertiesHook( const Vulkan_SubgroupPropertiesHook& ) = delete;
        Vulkan_SubgroupPropertiesHook( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanExtension( std::string(), status ), m_GetPhysicalDevicePropertiesHook( this ), m_VulkanDevicePropertiesPrintHook( this )
        {}
        VkPhysicalDeviceSubgroupProperties Properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };

    private:
        void Register( Vulkan& vulkan ) override
        {
            vulkan.AddExtensionHooks( &m_GetPhysicalDevicePropertiesHook, &m_VulkanDevicePropertiesPrintHook );
        }

        class GetPhysicalDevicePropertiesHook final : public ExtensionHook<VkPhysicalDeviceProperties2>
        {
        private:
            friend struct Vulkan_SubgroupPropertiesHook;
            GetPhysicalDevicePropertiesHook( Vulkan_SubgroupPropertiesHook* _Parent ) : Parent( _Parent ) {}
            VkStructureType StructureType() const override { return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES; };
            VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->Properties; };
            Vulkan_SubgroupPropertiesHook* Parent;
        } m_GetPhysicalDevicePropertiesHook;

        class VulkanDevicePropertiesPrintHook final : public ExtensionHook<VulkanDevicePropertiesPrint>
        {
        private:
            friend struct Vulkan_SubgroupPropertiesHook;
            VulkanDevicePropertiesPrintHook( Vulkan_SubgroupPropertiesHook* _Parent ) : Parent( _Parent ) {}
            VkStructureType StructureType() const override { return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES; };
            VkBaseOutStructure* Obtain( tBase* ) override { Parent->PrintProperties();  return nullptr; };
            Vulkan_SubgroupPropertiesHook* Parent;
        } m_VulkanDevicePropertiesPrintHook;

        void PrintProperties() const;
    };

    struct Vulkan_StorageFeaturesHook : public VulkanExtension
    {
        Vulkan_StorageFeaturesHook& operator=( const Vulkan_StorageFeaturesHook& ) = delete;
        Vulkan_StorageFeaturesHook( const Vulkan_StorageFeaturesHook& ) = delete;
        Vulkan_StorageFeaturesHook( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanExtension( std::string(), status ), m_DeviceCreateInfoHook(this), m_GetPhysicalDeviceFeaturesHook(this), m_VulkanDeviceFeaturePrintHook(this)
        {}
        VkPhysicalDevice16BitStorageFeatures AvailableFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR };

    private:
        class DeviceCreateInfoHook final : public ExtensionHook<VkDeviceCreateInfo>
        {
        private:
            friend struct Vulkan_StorageFeaturesHook;
            DeviceCreateInfoHook( Vulkan_StorageFeaturesHook* _Parent ) : Parent( _Parent ) {}
            VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->AvailableFeatures; }; // enable all available features
            VkStructureType StructureType() const override { return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR; };
            Vulkan_StorageFeaturesHook* Parent;
        } m_DeviceCreateInfoHook;

        class GetPhysicalDeviceFeaturesHook final : public ExtensionHook<VkPhysicalDeviceFeatures2>
        {
        private:
            friend struct Vulkan_StorageFeaturesHook;
            GetPhysicalDeviceFeaturesHook( Vulkan_StorageFeaturesHook* _Parent ) : Parent( _Parent ) {}
            VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->AvailableFeatures; };
            VkStructureType StructureType() const override { return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR; };
            Vulkan_StorageFeaturesHook* Parent;
        } m_GetPhysicalDeviceFeaturesHook;

        class VulkanDeviceFeaturePrintHook final : public ExtensionHook<VulkanDeviceFeaturePrint>
        {
        private:
            friend struct Vulkan_StorageFeaturesHook;
            VulkanDeviceFeaturePrintHook( Vulkan_StorageFeaturesHook* _Parent ) : Parent( _Parent ) {}
            VkStructureType StructureType() const override { return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR; };
            VkBaseOutStructure* Obtain( tBase* ) override { Parent->PrintFeatures();  return nullptr; };
            Vulkan_StorageFeaturesHook* Parent;
        } m_VulkanDeviceFeaturePrintHook;

        virtual void PrintFeatures() const;
    };

}; // namespace
