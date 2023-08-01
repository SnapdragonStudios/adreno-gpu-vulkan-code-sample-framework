//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

template<typename T, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class DeviceCreateInfoHook final : public ExtensionHook<VkDeviceCreateInfo>
{
    friend T;
    explicit DeviceCreateInfoHook( T* _Parent ) : ExtensionHook<VkDeviceCreateInfo>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
    VkBaseOutStructure* Obtain( tBase* ) override {
        Parent->PopulateRequestedFeatures();
        Parent->RequestedFeatures.pNext = nullptr;
        return (VkBaseOutStructure*) &Parent->RequestedFeatures;
    };
    T* Parent;
};

template<typename T, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class GetPhysicalDeviceFeaturesHook final : public ExtensionHook<VkPhysicalDeviceFeatures2>
{
    friend T;
    explicit GetPhysicalDeviceFeaturesHook( T* _Parent ) : ExtensionHook<VkPhysicalDeviceFeatures2>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
    VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->AvailableFeatures; };
    T* Parent;
};

template<typename T, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class VulkanDeviceFeaturePrintHook final : public ExtensionHook<VulkanDeviceFeaturePrint>
{
    friend T;
    explicit VulkanDeviceFeaturePrintHook( T* _Parent ) : ExtensionHook<VulkanDeviceFeaturePrint>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
    VkBaseOutStructure* Obtain( tBase* ) override { Parent->PrintFeatures();  return nullptr; };
    T* Parent;
};

template<typename T, VkStructureType T_PROPERTIESSTRUSTURE_TYPE>
class GetPhysicalDevicePropertiesHook final : public ExtensionHook<VkPhysicalDeviceProperties2>
{
    friend T;
    explicit GetPhysicalDevicePropertiesHook( T* _Parent ) : ExtensionHook<VkPhysicalDeviceProperties2>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return T_PROPERTIESSTRUSTURE_TYPE; };
    VkBaseOutStructure* Obtain( tBase* ) override { return (VkBaseOutStructure*) &Parent->Properties; };
    T* Parent;
};

template<typename T, VkStructureType T_PROPERTIESSTRUSTURE_TYPE>
class VulkanDevicePropertiesPrintHook final : public ExtensionHook<VulkanDevicePropertiesPrint>
{
    friend T;
    explicit VulkanDevicePropertiesPrintHook( T* _Parent ) : ExtensionHook<VulkanDevicePropertiesPrint>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return T_PROPERTIESSTRUSTURE_TYPE; };
    VkBaseOutStructure* Obtain( tBase* ) override { Parent->PrintProperties();  return nullptr; };
    T* Parent;
};

/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceFeatures2
/// Default behaviour assumes that the application will do a query before the vkCreateDevice and the features populated in that call are then required/desired by the vkCreateDevice.
/// Overriding 'PopulateRequestedFeatures' allows for changes to this behaviour (called when the feature 'chain' for vkCreateDevice is being constructed).
/// 
/// @tparam FeaturesStruct 
template<typename T_FEATURESSTRUCTURE, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class VulkanDeviceFeaturesExtensionHelper : public VulkanExtension
{
public:
    VulkanDeviceFeaturesExtensionHelper( std::string extensionName, VulkanExtension::eStatus status ) : VulkanExtension( extensionName, status ), m_DeviceCreateInfoHook( this ), m_GetPhysicalDeviceFeaturesHook( this ), m_VulkanDeviceFeaturePrintHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        vulkan.AddExtensionHooks( &m_DeviceCreateInfoHook, &m_GetPhysicalDeviceFeaturesHook, &m_VulkanDeviceFeaturePrintHook );
    }
    virtual void PrintFeatures() const = 0;

    T_FEATURESSTRUCTURE AvailableFeatures { T_FEATURESSTRUCTURE_TYPE };
    T_FEATURESSTRUCTURE RequestedFeatures { T_FEATURESSTRUCTURE_TYPE };
protected:
    friend class DeviceCreateInfoHook<VulkanDeviceFeaturesExtensionHelper, T_FEATURESSTRUCTURE_TYPE>;
    virtual void PopulateRequestedFeatures() { RequestedFeatures = AvailableFeatures; }
private:
    DeviceCreateInfoHook<VulkanDeviceFeaturesExtensionHelper, T_FEATURESSTRUCTURE_TYPE> m_DeviceCreateInfoHook;
    GetPhysicalDeviceFeaturesHook<VulkanDeviceFeaturesExtensionHelper, T_FEATURESSTRUCTURE_TYPE> m_GetPhysicalDeviceFeaturesHook;
    VulkanDeviceFeaturePrintHook<VulkanDeviceFeaturesExtensionHelper, T_FEATURESSTRUCTURE_TYPE> m_VulkanDeviceFeaturePrintHook;
};

/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceFeatures2
/// AND have properties queried through VkPhysicalDeviceProperties2.
/// Default behaviour assumes that the application will do a query before the vkCreateDevice and the features populated in that call are then required/desired by the vkCreateDevice.
/// Overriding 'PopulateRequestedFeatures' allows for changes to this behaviour (called when the feature 'chain' for vkCreateDevice is being constructed).
template<typename T_FEATURESSTRUCTURE, VkStructureType T_FEATURESSTRUCTURE_TYPE, typename T_PROPERTIESSTRUCTURE, VkStructureType T_PROPERTIESSTRUSTURE_TYPE>
class VulkanDeviceFeaturePropertiesExtensionHelper : public VulkanDeviceFeaturesExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE>
{
    using tBase = VulkanDeviceFeaturesExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE>;
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

    GetPhysicalDevicePropertiesHook<VulkanDeviceFeaturePropertiesExtensionHelper, T_PROPERTIESSTRUSTURE_TYPE> m_GetPhysicalDevicePropertiesHook;
    VulkanDevicePropertiesPrintHook<VulkanDeviceFeaturePropertiesExtensionHelper, T_PROPERTIESSTRUSTURE_TYPE> m_VulkanDevicePropertiesPrintHook;

    T_PROPERTIESSTRUCTURE Properties { T_PROPERTIESSTRUSTURE_TYPE };
};


template<typename T>
class InstanceFunctionPointerHook final : public ExtensionHook<VulkanInstanceFunctionPointerLookup>
{
    friend T;
    explicit InstanceFunctionPointerHook( T* _Parent ) : ExtensionHook<VulkanInstanceFunctionPointerLookup>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return (VkStructureType) 0; };
    VkBaseOutStructure* Obtain( tBase* pBase ) override {
        Parent->LookupFunctionPointers( pBase->vkInstance );
        return nullptr;
    }
    T* Parent;
};

template<typename T>
class DeviceFunctionPointerHook final : public ExtensionHook<VulkanDeviceFunctionPointerLookup>
{
    friend T;
    explicit DeviceFunctionPointerHook( T* _Parent ) : ExtensionHook<VulkanDeviceFunctionPointerLookup>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return (VkStructureType) 0; };
    VkBaseOutStructure* Obtain( tBase* pBase ) override {
        if (Parent->Status == VulkanExtension::eLoaded)
            Parent->LookupFunctionPointers( pBase->vkDevice, pBase->fpGetDeviceProcAddr );
        return nullptr;
    }
    T* Parent;
};


class VulkanFunctionPointerExtensionHelper : public VulkanExtension
{
public:
    explicit VulkanFunctionPointerExtensionHelper( std::string extensionName, VulkanExtension::eStatus status ) : VulkanExtension( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
    {}

    void Register( Vulkan& vulkan ) override;

protected:
    friend class InstanceFunctionPointerHook< VulkanFunctionPointerExtensionHelper>;
    friend class DeviceFunctionPointerHook< VulkanFunctionPointerExtensionHelper>;
    virtual void LookupFunctionPointers( VkInstance ) = 0;
    virtual void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) = 0;

private:
    InstanceFunctionPointerHook<VulkanFunctionPointerExtensionHelper> m_InstanceFunctionPointerHook;
    DeviceFunctionPointerHook<VulkanFunctionPointerExtensionHelper>   m_DeviceFunctionPointerHook;
};


/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceFeatures2
/// AND want function pointer lookup
template<typename T_FEATURESSTRUCTURE, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class VulkanFeaturesAndFunctionPointerExtensionHelper : public VulkanDeviceFeaturesExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE>
{
    using tBase = VulkanDeviceFeaturesExtensionHelper<T_FEATURESSTRUCTURE, T_FEATURESSTRUCTURE_TYPE>;
public:
    explicit VulkanFeaturesAndFunctionPointerExtensionHelper( std::string extensionName, VulkanExtension::eStatus status ) : tBase( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        tBase::Register( vulkan );
        vulkan.AddExtensionHooks( &m_InstanceFunctionPointerHook );
        vulkan.AddExtensionHooks( &m_DeviceFunctionPointerHook );
    }

protected:
    friend class InstanceFunctionPointerHook< VulkanFeaturesAndFunctionPointerExtensionHelper>;
    friend class DeviceFunctionPointerHook< VulkanFeaturesAndFunctionPointerExtensionHelper>;
    virtual void LookupFunctionPointers( VkInstance ) = 0;
    virtual void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) = 0;

private:
    InstanceFunctionPointerHook<VulkanFeaturesAndFunctionPointerExtensionHelper> m_InstanceFunctionPointerHook;
    DeviceFunctionPointerHook<VulkanFeaturesAndFunctionPointerExtensionHelper> m_DeviceFunctionPointerHook;
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
    friend class InstanceFunctionPointerHook< VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper>;
    friend class DeviceFunctionPointerHook< VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper>;
    virtual void LookupFunctionPointers( VkInstance ) = 0;
    virtual void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) = 0;

private:
    InstanceFunctionPointerHook<VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper> m_InstanceFunctionPointerHook;
    DeviceFunctionPointerHook<VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper>   m_DeviceFunctionPointerHook;
};


///
/// Library of Vulkan extension helpers
/// 

namespace ExtensionHelper
{

#if VK_KHR_shader_float16_int8

    struct Ext_VK_KHR_shader_float16_int8 : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceShaderFloat16Int8FeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME;
        explicit Ext_VK_KHR_shader_float16_int8( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };
    
#endif // VK_KHR_shader_float16_int8

#if VK_EXT_shader_image_atomic_int64

    struct Ext_VK_EXT_shader_image_atomic_int64 : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT>
    {
        static constexpr auto Name = VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME;
        explicit Ext_VK_EXT_shader_image_atomic_int64( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeaturesExtensionHelper( Name, status )
        {}
        void PrintFeatures() const override;
    };

#endif // VK_EXT_shader_image_atomic_int64

#if VK_EXT_index_type_uint8

    struct Ext_VK_EXT_index_type_uint8 : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceIndexTypeUint8FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT>
    {
        static constexpr auto Name = VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME;
        explicit Ext_VK_EXT_index_type_uint8( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };

#endif // VK_EXT_index_type_uint8

#if VK_KHR_shader_subgroup_extended_types

    struct Ext_VK_KHR_shader_subgroup_extended_types : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME;
        explicit Ext_VK_KHR_shader_subgroup_extended_types( VulkanExtension::eStatus status = VulkanExtension::eRequired )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
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
        explicit Ext_VK_EXT_descriptor_indexing( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
        void PrintProperties() const override;
    };

#endif // VK_EXT_descriptor_indexing

#if VK_KHR_8bit_storage

    struct Ext_VK_KHR_8bit_storage : public VulkanDeviceFeaturesExtensionHelper<
        VkPhysicalDevice8BitStorageFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
        explicit Ext_VK_KHR_8bit_storage( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };

#endif // VK_KHR_8bit_storage

#if VK_KHR_portability_subset

    struct Ext_VK_KHR_portability_subset : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDevicePortabilitySubsetFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
        VkPhysicalDevicePortabilitySubsetPropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR>
    {
        static constexpr auto Name = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
        explicit Ext_VK_KHR_portability_subset( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
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
        explicit Ext_VK_KHR_fragment_shading_rate( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper( Name, status )
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
        PFN_vkCmdSetFragmentShadingRateKHR m_vkCmdSetFragmentShadingRateKHR = nullptr;
    };

#endif // VK_KHR_fragment_shading_rate

#if VK_KHR_create_renderpass2

    struct Ext_VK_KHR_create_renderpass2 : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
        explicit Ext_VK_KHR_create_renderpass2( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkCreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCreateRenderPass2KHR" );
            m_vkCmdBeginRenderPass2KHR = (PFN_vkCmdBeginRenderPass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCmdBeginRenderPass2KHR" );
            m_vkCmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCmdNextSubpass2KHR" );
            m_vkCmdEndRenderPass2KHR = (PFN_vkCmdEndRenderPass2KHR) vkGetInstanceProcAddr( vkInstance, "vkCmdEndRenderPass2KHR" );
        }
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/ }
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
        explicit Ext_VK_KHR_draw_indirect_count( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkCmdDrawIndirectCountKHR = (PFN_vkCmdDrawIndirectCountKHR) vkGetInstanceProcAddr( vkInstance, "vkCmdDrawIndirectCountKHR" );
            m_vkCmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR) vkGetInstanceProcAddr( vkInstance, "vkCmdDrawIndexedIndirectCountKHR" );
        }
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/}
        PFN_vkCmdDrawIndirectCountKHR        m_vkCmdDrawIndirectCountKHR = nullptr;
        PFN_vkCmdDrawIndexedIndirectCountKHR m_vkCmdDrawIndexedIndirectCountKHR = nullptr;
    };

#endif // VK_KHR_draw_indirect_count

#if VK_EXT_hdr_metadata

    struct Ext_VK_EXT_hdr_metadata : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_EXT_HDR_METADATA_EXTENSION_NAME;
        explicit Ext_VK_EXT_hdr_metadata(VulkanExtension::eStatus status = VulkanExtension::eRequired) : VulkanFunctionPointerExtensionHelper(Name, status) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override {/*no instance functions*/}
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
        explicit Ext_VK_EXT_debug_utils( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override
        {
            m_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetInstanceProcAddr( vkInstance, "vkSetDebugUtilsObjectNameEXT" );
        }
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/ }
        bool SetDebugUtilsObjectName( VkDevice vkDevice, uint64_t object, VkObjectType objectType, const char* name ) const;

        PFN_vkSetDebugUtilsObjectNameEXT  m_vkSetDebugUtilsObjectNameEXT = nullptr;
    };

#endif // VK_EXT_debug_utils

#if VK_EXT_debug_marker

    struct Ext_VK_EXT_debug_marker : public VulkanFunctionPointerExtensionHelper
    {
        static constexpr auto Name = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
        explicit Ext_VK_EXT_debug_marker( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers( VkInstance ) override {/*no instance functions*/ }
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override
        {
            m_vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT) fpGetDeviceProcAddr( vkDevice, "vkDebugMarkerSetObjectNameEXT" );
        }
        bool DebugMarkerSetObjectName( VkDevice vkDevice, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name ) const;

        PFN_vkDebugMarkerSetObjectNameEXT m_vkDebugMarkerSetObjectNameEXT = nullptr;
    };

#endif // Ext_VK_EXT_debug_marker

#if VK_EXT_subgroup_size_control

    struct Ext_VK_EXT_subgroup_size_control : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT,
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT >
    {
        using tBase = VulkanDeviceFeaturePropertiesExtensionHelper;
        static constexpr auto Name = VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME;
        explicit Ext_VK_EXT_subgroup_size_control( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper( Name, status ), m_ShaderCreateHook(this)
        {}
        void PrintFeatures() const override;
        void PrintProperties() const override;

        VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT SubGroupSizeControl { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT };

    private:
        class ShaderCreateStageHook final : public ExtensionHook<VkPipelineShaderStageCreateInfo>
        {
            friend struct Ext_VK_EXT_subgroup_size_control;
            explicit ShaderCreateStageHook( Ext_VK_EXT_subgroup_size_control* _Parent ) : ExtensionHook<VkPipelineShaderStageCreateInfo>(), Parent( _Parent ) {}
            VkStructureType StructureType() const override { return (VkStructureType) 0; };
            VkBaseOutStructure* Obtain( tBase* pBase ) override {
                if ((pBase->stage & Parent->Properties.requiredSubgroupSizeStages)!=0
                    && Parent->RequestedFeatures.subgroupSizeControl
                    && Parent->SubGroupSizeControl.requiredSubgroupSize != 0)
                {
                    Parent->SubGroupSizeControl.pNext = nullptr;
                    pBase->flags |= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT;
                    return (VkBaseOutStructure*) &Parent->SubGroupSizeControl;
                }
                return nullptr;
            }
            void Release( VkBaseOutStructure* pBase ) override {
                assert( pBase == (VkBaseOutStructure *) &Parent->SubGroupSizeControl );
            }
            Ext_VK_EXT_subgroup_size_control* Parent;
        } m_ShaderCreateHook;

        void Register( Vulkan& vulkan ) override
        {
            tBase::Register( vulkan );
            vulkan.AddExtensionHooks( &m_ShaderCreateHook );
        }
    };

#endif // VK_EXT_subgroup_size_control

#if VK_EXT_host_query_reset

    struct Ext_VK_EXT_host_query_reset : public VulkanFeaturesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceHostQueryResetFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT >
    {
        using tBase = VulkanFeaturesAndFunctionPointerExtensionHelper;
        static constexpr auto Name = VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME;
        explicit Ext_VK_EXT_host_query_reset( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : tBase( Name, status )
        {}
        void PrintFeatures() const override;
        void LookupFunctionPointers( VkInstance vkInstance ) override {/*no instance functions*/ }
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override
        {
            if (RequestedFeatures.hostQueryReset == VK_TRUE)
                m_vkResetQueryPoolEXT = (PFN_vkResetQueryPoolEXT) fpGetDeviceProcAddr( vkDevice, "vkResetQueryPoolEXT" );
        }
        PFN_vkResetQueryPoolEXT m_vkResetQueryPoolEXT = nullptr;
    };

#endif // VK_EXT_host_query_reset

#if VK_KHR_timeline_semaphore

    struct Ext_VK_KHR_timeline_semaphore : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDeviceTimelineSemaphoreFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
        VkPhysicalDeviceTimelineSemaphorePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR >
    {
        using tBase = VulkanDeviceFeaturePropertiesExtensionHelper;
        static constexpr auto Name = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
        explicit Ext_VK_KHR_timeline_semaphore( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : tBase( Name, status )
        {}
        void PrintFeatures() const override;
        void PrintProperties() const override;
    };

#endif // VK_KHR_timeline_semaphore

#if VK_KHR_synchronization2

    struct Ext_VK_KHR_synchronization2 : public VulkanFeaturesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceSynchronization2FeaturesKHR, (VkStructureType) VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR >
    {
        using tBase = VulkanFeaturesAndFunctionPointerExtensionHelper;
        static constexpr auto Name = VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME;
        explicit Ext_VK_KHR_synchronization2(VulkanExtension::eStatus status = VulkanExtension::eRequired) : tBase(Name, status)
        {}
        void PrintFeatures() const override;
        void LookupFunctionPointers(VkInstance vkInstance) override;
        void LookupFunctionPointers(VkDevice, PFN_vkGetDeviceProcAddr) override {/*no device functions*/ }
        PFN_vkQueueSubmit2KHR           m_vkQueueSubmit2KHR = nullptr;
    };

#endif // VK_KHR_synchronization2

    //
    // Vulkan 1.1 (VK_VERSION_1_1) provided features/properties.
    // Same interface as other extensions but do not need to be added to the list of extension names on vkCreateDevice.
    //

    struct Vulkan_SubgroupPropertiesHook : public VulkanExtension
    {
        static constexpr auto Name = "SubgroupProperties";
        Vulkan_SubgroupPropertiesHook& operator=( const Vulkan_SubgroupPropertiesHook& ) = delete;
        Vulkan_SubgroupPropertiesHook( const Vulkan_SubgroupPropertiesHook& ) = delete;
        explicit Vulkan_SubgroupPropertiesHook( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanExtension( Name, status ), m_GetPhysicalDevicePropertiesHook( this ), m_VulkanDevicePropertiesPrintHook( this )
        {}
        VkPhysicalDeviceSubgroupProperties Properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };

    private:
        void Register( Vulkan& vulkan ) override
        {
            vulkan.AddExtensionHooks( &m_GetPhysicalDevicePropertiesHook, &m_VulkanDevicePropertiesPrintHook );
        }

        friend class GetPhysicalDevicePropertiesHook<Vulkan_SubgroupPropertiesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES>;
        friend class VulkanDevicePropertiesPrintHook<Vulkan_SubgroupPropertiesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES>;
        GetPhysicalDevicePropertiesHook<Vulkan_SubgroupPropertiesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES> m_GetPhysicalDevicePropertiesHook;
        VulkanDevicePropertiesPrintHook<Vulkan_SubgroupPropertiesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES> m_VulkanDevicePropertiesPrintHook;

        void PrintProperties() const;
    };

    struct Vulkan_StorageFeaturesHook : public VulkanExtension
    {
        static constexpr auto Name = "StorageFeatures";
        Vulkan_StorageFeaturesHook& operator=( const Vulkan_StorageFeaturesHook& ) = delete;
        Vulkan_StorageFeaturesHook( const Vulkan_StorageFeaturesHook& ) = delete;
        explicit Vulkan_StorageFeaturesHook( VulkanExtension::eStatus status = VulkanExtension::eRequired ) : VulkanExtension( std::string(), status ), m_DeviceCreateInfoHook(this), m_GetPhysicalDeviceFeaturesHook(this), m_VulkanDeviceFeaturePrintHook(this)
        {}
        VkPhysicalDevice16BitStorageFeatures AvailableFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR };

    private:
        friend class DeviceCreateInfoHook< Vulkan_StorageFeaturesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR>;
        friend class GetPhysicalDeviceFeaturesHook< Vulkan_StorageFeaturesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR>;
        friend class VulkanDeviceFeaturePrintHook< Vulkan_StorageFeaturesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR>;
        DeviceCreateInfoHook<Vulkan_StorageFeaturesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR>          m_DeviceCreateInfoHook;
        GetPhysicalDeviceFeaturesHook<Vulkan_StorageFeaturesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR> m_GetPhysicalDeviceFeaturesHook;
        VulkanDeviceFeaturePrintHook<Vulkan_StorageFeaturesHook, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR>  m_VulkanDeviceFeaturePrintHook;

        void Register( Vulkan& vulkan ) override
        {
            vulkan.AddExtensionHooks( &m_DeviceCreateInfoHook, &m_GetPhysicalDeviceFeaturesHook, &m_VulkanDeviceFeaturePrintHook );
        }
        virtual void PrintFeatures() const;
        virtual void PopulateRequestedFeatures() { RequestedFeatures = AvailableFeatures; }
        VkPhysicalDevice16BitStorageFeatures RequestedFeatures;
    };

}; // namespace
