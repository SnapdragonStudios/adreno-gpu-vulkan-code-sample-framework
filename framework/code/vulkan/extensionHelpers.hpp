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


template<typename T, VkStructureType T_FEATURESSTRUCTURE_TYPE>
class DeviceCreateInfoHook final : public ExtensionHook<VkDeviceCreateInfo>
{
    friend T;
    explicit DeviceCreateInfoHook( T* _Parent ) : ExtensionHook<VkDeviceCreateInfo>(), Parent( _Parent ) {}
    VkStructureType StructureType() const override { return T_FEATURESSTRUCTURE_TYPE; };
    VkBaseOutStructure* Obtain( tBase* ) override {
        if (Parent->Status == VulkanExtensionStatus::eLoaded)
        {
            Parent->PopulateRequestedFeatures();
            Parent->RequestedFeatures.pNext = nullptr;
            return (VkBaseOutStructure*)&Parent->RequestedFeatures;
        }
        else
            return nullptr;
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


/// @brief Helper template for extensions that are initialized through the vkCreateDevice and queried through vkGetPhysicalDeviceProperties2
/// Default behaviour assumes that the application will do a query before the vkCreateDevice and the properties populated in that call are then required/desired by the vkCreateDevice.
/// Overriding 'PopulateRequestedProperties' allows for changes to this behaviour (called when the properties 'chain' for vkCreateDevice is being constructed).
template<typename T_PROPERTIESSTRUCTURE, VkStructureType T_PROPERTIESSTRUSTURE_TYPE>
class VulkanDevicePropertiesExtensionHelper : public VulkanExtension<VulkanExtensionType::eDevice>
{
public:
    using tBase = VulkanExtension<VulkanExtensionType::eDevice>;
    VulkanDevicePropertiesExtensionHelper( std::string extensionName, VulkanExtensionStatus status ) : tBase( extensionName, status ), m_GetPhysicalDevicePropertiesHook( this ), m_VulkanDevicePropertiesPrintHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        vulkan.AddExtensionHooks( &m_GetPhysicalDevicePropertiesHook, &m_VulkanDevicePropertiesPrintHook );
    }
    virtual void PrintProperties() const = 0;

    T_PROPERTIESSTRUCTURE       Properties          {T_PROPERTIESSTRUSTURE_TYPE};
private:
    GetPhysicalDevicePropertiesHook<VulkanDevicePropertiesExtensionHelper, T_PROPERTIESSTRUSTURE_TYPE> m_GetPhysicalDevicePropertiesHook;
    VulkanDevicePropertiesPrintHook<VulkanDevicePropertiesExtensionHelper, T_PROPERTIESSTRUSTURE_TYPE> m_VulkanDevicePropertiesPrintHook;
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


/// @brief Helper template for device extensions that have properties queried through
/// VkPhysicalDeviceProperties2 AND want function pointer lookup.
/// These extensions do not pass a structure in to VkCreateDevice.
template<typename T_PROPERTIESSTRUCTURE, VkStructureType T_PROPERTIESSTRUCTURE_TYPE>
class VulkanPropertiesAndFunctionPointerExtensionHelper : public VulkanDevicePropertiesExtensionHelper<T_PROPERTIESSTRUCTURE, T_PROPERTIESSTRUCTURE_TYPE>
{
    using tBase = VulkanDevicePropertiesExtensionHelper<T_PROPERTIESSTRUCTURE, T_PROPERTIESSTRUCTURE_TYPE>;
public:
    explicit VulkanPropertiesAndFunctionPointerExtensionHelper( std::string extensionName, VulkanExtensionStatus status ) : tBase( extensionName, status ), m_InstanceFunctionPointerHook( this ), m_DeviceFunctionPointerHook( this )
    {}

    void Register( Vulkan& vulkan ) override
    {
        tBase::Register( vulkan );
        vulkan.AddExtensionHooks( &m_InstanceFunctionPointerHook );
        vulkan.AddExtensionHooks( &m_DeviceFunctionPointerHook );
    }

protected:
    friend class InstanceFunctionPointerHook< VulkanPropertiesAndFunctionPointerExtensionHelper>;
    friend class DeviceFunctionPointerHook< VulkanPropertiesAndFunctionPointerExtensionHelper>;
    virtual void LookupFunctionPointers( VkInstance ) = 0;
    virtual void LookupFunctionPointers( VkDevice, PFN_vkGetDeviceProcAddr ) = 0;

private:
    InstanceFunctionPointerHook<VulkanPropertiesAndFunctionPointerExtensionHelper> m_InstanceFunctionPointerHook;
    DeviceFunctionPointerHook<VulkanPropertiesAndFunctionPointerExtensionHelper> m_DeviceFunctionPointerHook;
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
