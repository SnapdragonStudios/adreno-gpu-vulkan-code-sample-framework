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
class VulkanDeviceFeaturesExtensionHelper : public VulkanExtension<VulkanExtensionType::eDevice>
{
public:
    using tBase = VulkanExtension<VulkanExtensionType::eDevice>;
    VulkanDeviceFeaturesExtensionHelper( std::string extensionName, VulkanExtensionStatus status ) : tBase( extensionName, status ), m_DeviceCreateInfoHook( this ), m_GetPhysicalDeviceFeaturesHook( this ), m_VulkanDeviceFeaturePrintHook( this )
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
    VulkanDeviceFeaturePropertiesExtensionHelper( std::string extensionName, VulkanExtensionStatus status )
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
        if (Parent->Status == VulkanExtensionStatus::eLoaded)
            Parent->LookupFunctionPointers( pBase->vkDevice, pBase->fpGetDeviceProcAddr );
        return nullptr;
    }
    T* Parent;
};


template<VulkanExtensionType T_TYPE>
class VulkanFunctionPointerExtensionHelper : public VulkanExtension<T_TYPE>
{
public:
    explicit VulkanFunctionPointerExtensionHelper( std::string extensionName, VulkanExtensionStatus status ) : VulkanExtension<T_TYPE>( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
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
    explicit VulkanFeaturesAndFunctionPointerExtensionHelper( std::string extensionName, VulkanExtensionStatus status ) : tBase( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
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
    VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper( std::string extensionName, VulkanExtensionStatus status ) : tBase( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
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

#if VK_EXT_mesh_shader

    struct Ext_VK_KHR_mesh_shader : public VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        VkPhysicalDeviceMeshShaderPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT>
    {
        static constexpr auto Name = VK_EXT_MESH_SHADER_EXTENSION_NAME;
        explicit Ext_VK_KHR_mesh_shader(VulkanExtensionStatus status = VulkanExtensionStatus::eRequired) : VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper(Name, status)
        {}

        void PopulateRequestedFeatures() override
        {
            RequestedFeatures = AvailableFeatures;
            RequestedFeatures.multiviewMeshShader = VK_FALSE;                   //if we need multiview then device needs setting up for multiview also
            RequestedFeatures.primitiveFragmentShadingRateMeshShader = VK_FALSE;//if we need fragment shading rate then device needs setting up for it (and we need to revisit this flag!)
        }
        void LookupFunctionPointers(VkInstance) override {}
        void LookupFunctionPointers(VkDevice, PFN_vkGetDeviceProcAddr) override;
        void PrintFeatures() const override;
        void PrintProperties() const override;
        PFN_vkCmdDrawMeshTasksEXT                   m_vkCmdDrawMeshTasksEXT = nullptr;
        PFN_vkCmdDrawMeshTasksIndirectEXT           m_vkCmdDrawMeshTasksIndirectEXT = nullptr;
        PFN_vkCmdDrawMeshTasksIndirectCountEXT      m_vkCmdDrawMeshTasksIndirectCountEXT = nullptr;
    };

#endif // VK_EXT_mesh_shader

#if VK_KHR_shader_float16_int8

    struct Ext_VK_KHR_shader_float16_int8 : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceShaderFloat16Int8FeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME;
        explicit Ext_VK_KHR_shader_float16_int8( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };
    
#endif // VK_KHR_shader_float16_int8

#if VK_EXT_shader_image_atomic_int64

    struct Ext_VK_EXT_shader_image_atomic_int64 : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT>
    {
        static constexpr auto Name = VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME;
        explicit Ext_VK_EXT_shader_image_atomic_int64( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
            : VulkanDeviceFeaturesExtensionHelper( Name, status )
        {}
        void PrintFeatures() const override;
    };

#endif // VK_EXT_shader_image_atomic_int64

#if VK_EXT_index_type_uint8

    struct Ext_VK_EXT_index_type_uint8 : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceIndexTypeUint8FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT>
    {
        static constexpr auto Name = VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME;
        explicit Ext_VK_EXT_index_type_uint8( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
            : VulkanDeviceFeaturesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
    };

#endif // VK_EXT_index_type_uint8

#if VK_KHR_shader_subgroup_extended_types

    struct Ext_VK_KHR_shader_subgroup_extended_types : public VulkanDeviceFeaturesExtensionHelper<VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME;
        explicit Ext_VK_KHR_shader_subgroup_extended_types( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
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
        explicit Ext_VK_EXT_descriptor_indexing( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
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
        explicit Ext_VK_KHR_8bit_storage( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanDeviceFeaturesExtensionHelper(Name, status)
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
        explicit Ext_VK_KHR_portability_subset( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper(Name, status)
        {}
        void PrintFeatures() const override;
        void PrintProperties() const override;
    };

#else

    // 'dummy' implementation of Ext_VK_KHR_portability_subset for when vulkan headers do not contain 'VK_KHR_portability_subset'
    struct Ext_VK_KHR_portability_subset : public VulkanExtension<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = "VK_KHR_portability_subset";
        Ext_VK_KHR_portability_subset( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanExtension( Name, status ) {}
    };

#endif // VK_KHR_portability_subset

#if VK_KHR_fragment_shading_rate

    struct Ext_VK_KHR_fragment_shading_rate : public VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR>
    {
        static constexpr auto Name = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
        explicit Ext_VK_KHR_fragment_shading_rate( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper( Name, status )
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

    struct Ext_VK_KHR_create_renderpass2 : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
        explicit Ext_VK_KHR_create_renderpass2( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers(VkInstance vkInstance) override;
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/ }
        PFN_vkCreateRenderPass2KHR      m_vkCreateRenderPass2KHR = nullptr;
        PFN_vkCmdBeginRenderPass2KHR    m_vkCmdBeginRenderPass2KHR = nullptr;
        PFN_vkCmdNextSubpass2KHR        m_vkCmdNextSubpass2KHR = nullptr;
        PFN_vkCmdEndRenderPass2KHR      m_vkCmdEndRenderPass2KHR = nullptr;
    };

#endif // VK_KHR_create_renderpass2

#if VK_KHR_draw_indirect_count

    struct Ext_VK_KHR_draw_indirect_count : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME;
        explicit Ext_VK_KHR_draw_indirect_count( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
        void LookupFunctionPointers(VkInstance vkInstance) override;
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/}
        PFN_vkCmdDrawIndirectCountKHR        m_vkCmdDrawIndirectCountKHR = nullptr;
        PFN_vkCmdDrawIndexedIndirectCountKHR m_vkCmdDrawIndexedIndirectCountKHR = nullptr;
    };

#endif // VK_KHR_draw_indirect_count

#if VK_EXT_hdr_metadata

    struct Ext_VK_EXT_hdr_metadata : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = VK_EXT_HDR_METADATA_EXTENSION_NAME;
        explicit Ext_VK_EXT_hdr_metadata(VulkanExtensionStatus status = VulkanExtensionStatus::eRequired) : VulkanFunctionPointerExtensionHelper(Name, status) {}
        void LookupFunctionPointers( VkInstance vkInstance ) override {/*no instance functions*/}
        void LookupFunctionPointers( VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr ) override
        {
            m_vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT) fpGetDeviceProcAddr( vkDevice, "vkSetHdrMetadataEXT" );
        }
        PFN_vkSetHdrMetadataEXT             m_vkSetHdrMetadataEXT = nullptr;
    };

#endif // VK_EXT_hdr_metadata

#if VK_EXT_debug_utils

    struct Ext_VK_EXT_debug_utils : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        explicit Ext_VK_EXT_debug_utils( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
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

    struct Ext_VK_EXT_debug_marker : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
        explicit Ext_VK_EXT_debug_marker( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanFunctionPointerExtensionHelper( Name, status ) {}
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
        explicit Ext_VK_EXT_subgroup_size_control( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : VulkanDeviceFeaturePropertiesExtensionHelper( Name, status ), m_ShaderCreateHook(this)
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
        explicit Ext_VK_EXT_host_query_reset( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( Name, status )
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

#if VK_ARM_tensors

    struct Ext_VK_ARM_tensors : public VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceTensorFeaturesARM, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TENSOR_FEATURES_ARM,
        VkPhysicalDeviceTensorPropertiesARM, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TENSOR_PROPERTIES_ARM>
    {
        static constexpr auto Name = VK_ARM_TENSORS_EXTENSION_NAME;

        explicit Ext_VK_ARM_tensors(VulkanExtensionStatus status = VulkanExtensionStatus::eRequired)
            : VulkanFeaturesPropertiesAndFunctionPointerExtensionHelper(Name, status)
        {
        }

        void PopulateRequestedFeatures() override
        {
            RequestedFeatures.sType = AvailableFeatures.sType;
            RequestedFeatures.tensorNonPacked = AvailableFeatures.tensorNonPacked;
            RequestedFeatures.shaderTensorAccess = AvailableFeatures.shaderTensorAccess;
            RequestedFeatures.shaderStorageTensorArrayDynamicIndexing = AvailableFeatures.shaderStorageTensorArrayDynamicIndexing;
            RequestedFeatures.shaderStorageTensorArrayNonUniformIndexing = AvailableFeatures.shaderStorageTensorArrayNonUniformIndexing;
            RequestedFeatures.descriptorBindingStorageTensorUpdateAfterBind = AvailableFeatures.descriptorBindingStorageTensorUpdateAfterBind;
            RequestedFeatures.tensors = AvailableFeatures.tensors;

            // Forcing a few features while the extension is private:
            RequestedFeatures.shaderTensorAccess = true;
            RequestedFeatures.tensors = true;
        }

        void LookupFunctionPointers(VkInstance) override {}

        void LookupFunctionPointers(VkDevice device, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr) override
        {
            m_vkCreateTensorARM = reinterpret_cast<PFN_vkCreateTensorARM>(fpGetDeviceProcAddr(device, "vkCreateTensorARM"));
            m_vkDestroyTensorARM = reinterpret_cast<PFN_vkDestroyTensorARM>(fpGetDeviceProcAddr(device, "vkDestroyTensorARM"));
            m_vkCreateTensorViewARM = reinterpret_cast<PFN_vkCreateTensorViewARM>(fpGetDeviceProcAddr(device, "vkCreateTensorViewARM"));
            m_vkDestroyTensorViewARM = reinterpret_cast<PFN_vkDestroyTensorViewARM>(fpGetDeviceProcAddr(device, "vkDestroyTensorViewARM"));
            m_vkGetTensorMemoryRequirementsARM = reinterpret_cast<PFN_vkGetTensorMemoryRequirementsARM>(fpGetDeviceProcAddr(device, "vkGetTensorMemoryRequirementsARM"));
            m_vkBindTensorMemoryARM = reinterpret_cast<PFN_vkBindTensorMemoryARM>(fpGetDeviceProcAddr(device, "vkBindTensorMemoryARM"));
            m_vkGetDeviceTensorMemoryRequirementsARM = reinterpret_cast<PFN_vkGetDeviceTensorMemoryRequirementsARM>(fpGetDeviceProcAddr(device, "vkGetDeviceTensorMemoryRequirementsARM"));
            m_vkCmdCopyTensorARM = reinterpret_cast<PFN_vkCmdCopyTensorARM>(fpGetDeviceProcAddr(device, "vkCmdCopyTensorARM"));
            m_vkGetTensorOpaqueCaptureDescriptorDataARM = reinterpret_cast<PFN_vkGetTensorOpaqueCaptureDescriptorDataARM>(fpGetDeviceProcAddr(device, "vkGetTensorOpaqueCaptureDescriptorDataARM"));
            m_vkGetTensorViewOpaqueCaptureDescriptorDataARM = reinterpret_cast<PFN_vkGetTensorViewOpaqueCaptureDescriptorDataARM>(fpGetDeviceProcAddr(device, "vkGetTensorViewOpaqueCaptureDescriptorDataARM"));
        }

        void PrintFeatures() const override;
        void PrintProperties() const override;

        PFN_vkCreateTensorARM m_vkCreateTensorARM = nullptr;
        PFN_vkDestroyTensorARM m_vkDestroyTensorARM = nullptr;
        PFN_vkCreateTensorViewARM m_vkCreateTensorViewARM = nullptr;
        PFN_vkDestroyTensorViewARM m_vkDestroyTensorViewARM = nullptr;
        PFN_vkGetTensorMemoryRequirementsARM m_vkGetTensorMemoryRequirementsARM = nullptr;
        PFN_vkBindTensorMemoryARM m_vkBindTensorMemoryARM = nullptr;
        PFN_vkGetDeviceTensorMemoryRequirementsARM m_vkGetDeviceTensorMemoryRequirementsARM = nullptr;
        PFN_vkCmdCopyTensorARM m_vkCmdCopyTensorARM = nullptr;
        PFN_vkGetTensorOpaqueCaptureDescriptorDataARM m_vkGetTensorOpaqueCaptureDescriptorDataARM = nullptr;
        PFN_vkGetTensorViewOpaqueCaptureDescriptorDataARM m_vkGetTensorViewOpaqueCaptureDescriptorDataARM = nullptr;
    };

#endif // VK_ARM_tensors

#if VK_ARM_data_graph

    struct Ext_VK_ARM_data_graph : public VulkanFeaturesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceDataGraphFeaturesARM, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DATA_GRAPH_FEATURES_ARM>
    {
        using tBase = VulkanFeaturesAndFunctionPointerExtensionHelper;
        static constexpr auto Name = VK_ARM_DATA_GRAPH_EXTENSION_NAME;

        explicit Ext_VK_ARM_data_graph(VulkanExtensionStatus status = VulkanExtensionStatus::eRequired)
            : tBase(Name, status)
        {
        }

        void PopulateRequestedFeatures() override
        {
            RequestedFeatures.sType = AvailableFeatures.sType;
            RequestedFeatures.dataGraph = AvailableFeatures.dataGraph;
            RequestedFeatures.dataGraphUpdateAfterBind = AvailableFeatures.dataGraphUpdateAfterBind;
            RequestedFeatures.dataGraphSpecializationConstants = AvailableFeatures.dataGraphSpecializationConstants;
            RequestedFeatures.dataGraphDescriptorBuffer = AvailableFeatures.dataGraphDescriptorBuffer;
            RequestedFeatures.dataGraphShaderModule = AvailableFeatures.dataGraphShaderModule;

            // Forcing a few features while the extension is private:
            RequestedFeatures.dataGraph = true;
        }

        void PrintFeatures() const override;
        void LookupFunctionPointers(VkInstance vkInstance) override 
        { 
            m_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM>(
                vkGetInstanceProcAddr(vkInstance, "vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM"));
            m_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM>(
                vkGetInstanceProcAddr(vkInstance, "vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM"));
        }

        void LookupFunctionPointers(VkDevice vkDevice, PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr) override
        {
            m_vkCreateDataGraphPipelinesARM = reinterpret_cast<PFN_vkCreateDataGraphPipelinesARM>(
                fpGetDeviceProcAddr(vkDevice, "vkCreateDataGraphPipelinesARM"));
            m_vkCreateDataGraphPipelineSessionARM = reinterpret_cast<PFN_vkCreateDataGraphPipelineSessionARM>(
                fpGetDeviceProcAddr(vkDevice, "vkCreateDataGraphPipelineSessionARM"));
            m_vkGetDataGraphPipelineSessionBindPointRequirementsARM = reinterpret_cast<PFN_vkGetDataGraphPipelineSessionBindPointRequirementsARM>(
                fpGetDeviceProcAddr(vkDevice, "vkGetDataGraphPipelineSessionBindPointRequirementsARM"));
            m_vkGetDataGraphPipelineSessionMemoryRequirementsARM = reinterpret_cast<PFN_vkGetDataGraphPipelineSessionMemoryRequirementsARM>(
                fpGetDeviceProcAddr(vkDevice, "vkGetDataGraphPipelineSessionMemoryRequirementsARM"));
            m_vkBindDataGraphPipelineSessionMemoryARM = reinterpret_cast<PFN_vkBindDataGraphPipelineSessionMemoryARM>(
                fpGetDeviceProcAddr(vkDevice, "vkBindDataGraphPipelineSessionMemoryARM"));
            m_vkDestroyDataGraphPipelineSessionARM = reinterpret_cast<PFN_vkDestroyDataGraphPipelineSessionARM>(
                fpGetDeviceProcAddr(vkDevice, "vkDestroyDataGraphPipelineSessionARM"));
            m_vkCmdDispatchDataGraphARM = reinterpret_cast<PFN_vkCmdDispatchDataGraphARM>(
                fpGetDeviceProcAddr(vkDevice, "vkCmdDispatchDataGraphARM"));
            m_vkGetDataGraphPipelineAvailablePropertiesARM = reinterpret_cast<PFN_vkGetDataGraphPipelineAvailablePropertiesARM>(
                fpGetDeviceProcAddr(vkDevice, "vkGetDataGraphPipelineAvailablePropertiesARM"));
            m_vkGetDataGraphPipelinePropertiesARM = reinterpret_cast<PFN_vkGetDataGraphPipelinePropertiesARM>(
                fpGetDeviceProcAddr(vkDevice, "vkGetDataGraphPipelinePropertiesARM"));
        }

        // Function pointers
        PFN_vkCreateDataGraphPipelinesARM m_vkCreateDataGraphPipelinesARM = nullptr;
        PFN_vkCreateDataGraphPipelineSessionARM m_vkCreateDataGraphPipelineSessionARM = nullptr;
        PFN_vkGetDataGraphPipelineSessionBindPointRequirementsARM m_vkGetDataGraphPipelineSessionBindPointRequirementsARM = nullptr;
        PFN_vkGetDataGraphPipelineSessionMemoryRequirementsARM m_vkGetDataGraphPipelineSessionMemoryRequirementsARM = nullptr;
        PFN_vkBindDataGraphPipelineSessionMemoryARM m_vkBindDataGraphPipelineSessionMemoryARM = nullptr;
        PFN_vkDestroyDataGraphPipelineSessionARM m_vkDestroyDataGraphPipelineSessionARM = nullptr;
        PFN_vkCmdDispatchDataGraphARM m_vkCmdDispatchDataGraphARM = nullptr;
        PFN_vkGetDataGraphPipelineAvailablePropertiesARM m_vkGetDataGraphPipelineAvailablePropertiesARM = nullptr;
        PFN_vkGetDataGraphPipelinePropertiesARM m_vkGetDataGraphPipelinePropertiesARM = nullptr;
        PFN_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM m_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM = nullptr;
        PFN_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM m_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM = nullptr;
    };

#endif // VK_ARM_data_graph
#if VK_KHR_timeline_semaphore

    struct Ext_VK_KHR_timeline_semaphore : public VulkanDeviceFeaturePropertiesExtensionHelper<
        VkPhysicalDeviceTimelineSemaphoreFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
        VkPhysicalDeviceTimelineSemaphorePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR >
    {
        using tBase = VulkanDeviceFeaturePropertiesExtensionHelper;
        static constexpr auto Name = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
        explicit Ext_VK_KHR_timeline_semaphore( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( Name, status )
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
        explicit Ext_VK_KHR_synchronization2(VulkanExtensionStatus status = VulkanExtensionStatus::eRequired) : tBase(Name, status)
        {}
        void PrintFeatures() const override;
        void LookupFunctionPointers(VkInstance vkInstance) override;
        void LookupFunctionPointers(VkDevice, PFN_vkGetDeviceProcAddr) override {/*no device functions*/ }

        PFN_vkCmdSetEvent2KHR        m_vkCmdSetEvent2KHR        = nullptr;
        PFN_vkCmdResetEvent2KHR      m_vkCmdResetEvent2KHR      = nullptr;
        PFN_vkCmdWaitEvents2KHR      m_vkCmdWaitEvents2KHR      = nullptr;
        PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR = nullptr;
        PFN_vkQueueSubmit2KHR        m_vkQueueSubmit2KHR        = nullptr;
        PFN_vkCmdWriteTimestamp2KHR  m_vkCmdWriteTimestamp2KHR  = nullptr;
    };

#endif // VK_KHR_synchronization2

#if VK_QCOM_tile_properties

    struct Ext_VK_QCOM_tile_properties : public VulkanFeaturesAndFunctionPointerExtensionHelper<
        VkPhysicalDeviceTilePropertiesFeaturesQCOM, (VkStructureType)VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM>
    {
            using tBase = VulkanFeaturesAndFunctionPointerExtensionHelper;
            static constexpr auto Name = VK_QCOM_TILE_PROPERTIES_EXTENSION_NAME;
            explicit Ext_VK_QCOM_tile_properties( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( Name, status )
            {}
            void LookupFunctionPointers( VkInstance vkInstance ) override  {/*no instance functions*/ };
            void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override;
            void PrintFeatures() const override;
            PFN_vkGetDynamicRenderingTilePropertiesQCOM     m_vkGetDynamicRenderingTilePropertiesQCOM = nullptr;
            PFN_vkGetFramebufferTilePropertiesQCOM          m_vkGetFramebufferTilePropertiesQCOM = nullptr;
    };

#endif // VK_QCOM_tile_properties

#if VK_KHR_ray_tracing_position_fetch

    struct Ext_VK_KHR_ray_tracing_position_fetch : public VulkanDeviceFeaturesExtensionHelper<
        VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR>
    {
        static constexpr auto Name = VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME;
        explicit Ext_VK_KHR_ray_tracing_position_fetch( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
            : VulkanDeviceFeaturesExtensionHelper( Name, status )
        {}
        void PrintFeatures() const override;
    };

#endif // VK_KHR_ray_tracing_position_fetch

#if VK_EXT_scalar_block_layout

    struct Ext_VK_EXT_scalar_block_layout : public VulkanDeviceFeaturesExtensionHelper<
        VkPhysicalDeviceScalarBlockLayoutFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT>
    {
        static constexpr auto Name = VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME;
        explicit Ext_VK_EXT_scalar_block_layout( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired )
            : VulkanDeviceFeaturesExtensionHelper( Name, status )
        {}
        void PrintFeatures() const override;
    };

#endif // VK_EXT_scalar_block_layout

#if VK_KHR_get_physical_device_properties2

    // Instance extension
    struct Ext_VK_KHR_get_physical_device_properties2 : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eInstance>
    {
        using tBase = VulkanFunctionPointerExtensionHelper;
        static constexpr auto Name = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
        explicit Ext_VK_KHR_get_physical_device_properties2(VulkanExtensionStatus status = VulkanExtensionStatus::eRequired) : tBase(Name, status)
        {}
        void LookupFunctionPointers(VkInstance vkInstance) override;
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/ }
        PFN_vkGetPhysicalDeviceFeatures2KHR             m_vkGetPhysicalDeviceFeatures2KHR = nullptr;
        PFN_vkGetPhysicalDeviceFormatProperties2KHR     m_vkGetPhysicalDeviceFormatProperties2KHR = nullptr;
        PFN_vkGetPhysicalDeviceImageFormatProperties2KHR m_vkGetPhysicalDeviceImageFormatProperties2KHR = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties2KHR     m_vkGetPhysicalDeviceMemoryProperties2KHR = nullptr;
        PFN_vkGetPhysicalDeviceProperties2KHR           m_vkGetPhysicalDeviceProperties2KHR = nullptr;
    };

#endif // VK_KHR_get_physical_device_properties2

#if VK_KHR_surface

    // Instance extension
    struct Ext_VK_KHR_surface : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eInstance>
    {
        using tBase = VulkanFunctionPointerExtensionHelper;
        static constexpr auto Name = VK_KHR_SURFACE_EXTENSION_NAME;
        explicit Ext_VK_KHR_surface( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( Name, status )
        {}
        void LookupFunctionPointers( VkInstance vkInstance ) override;
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/ }
        PFN_vkDestroySurfaceKHR m_vkDestroySurfaceKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR m_vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR m_vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR m_vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    };

#endif // VK_KHR_surface

#if VK_KHR_get_surface_capabilities2

    // Instance extension
    struct Ext_VK_KHR_get_surface_capabilities2 : public VulkanFunctionPointerExtensionHelper<VulkanExtensionType::eInstance>
    {
        using tBase = VulkanFunctionPointerExtensionHelper;
        static constexpr auto Name = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
        explicit Ext_VK_KHR_get_surface_capabilities2( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( Name, status )
        {}
        void LookupFunctionPointers( VkInstance vkInstance ) override;
        void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) override {/*no device functions*/ }
        PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR  m_vkGetPhysicalDeviceSurfaceCapabilities2KHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceFormats2KHR       m_vkGetPhysicalDeviceSurfaceFormats2KHR = nullptr;
    };

#endif // VK_KHR_get_surface_capabilities2

    //
    // Vulkan 1.1 (VK_VERSION_1_1) provided features/properties.
    // Same interface as other extensions but do not need to be added to the list of extension names on vkCreateDevice.
    //

    struct Vulkan_SubgroupPropertiesHook : public VulkanExtension<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = "SubgroupProperties";
        using tBase = VulkanExtension<VulkanExtensionType::eDevice>;
        Vulkan_SubgroupPropertiesHook& operator=( const Vulkan_SubgroupPropertiesHook& ) = delete;
        Vulkan_SubgroupPropertiesHook( const Vulkan_SubgroupPropertiesHook& ) = delete;
        explicit Vulkan_SubgroupPropertiesHook( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( Name, status ), m_GetPhysicalDevicePropertiesHook( this ), m_VulkanDevicePropertiesPrintHook( this )
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

    struct Vulkan_StorageFeaturesHook : public VulkanExtension<VulkanExtensionType::eDevice>
    {
        static constexpr auto Name = "StorageFeatures";
        using tBase = VulkanExtension<VulkanExtensionType::eDevice>;
        Vulkan_StorageFeaturesHook& operator=( const Vulkan_StorageFeaturesHook& ) = delete;
        Vulkan_StorageFeaturesHook( const Vulkan_StorageFeaturesHook& ) = delete;
        explicit Vulkan_StorageFeaturesHook( VulkanExtensionStatus status = VulkanExtensionStatus::eRequired ) : tBase( std::string(), status ), m_DeviceCreateInfoHook(this), m_GetPhysicalDeviceFeaturesHook(this), m_VulkanDeviceFeaturePrintHook(this)
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
        VkPhysicalDevice16BitStorageFeatures RequestedFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR };
    };

}; // namespace
