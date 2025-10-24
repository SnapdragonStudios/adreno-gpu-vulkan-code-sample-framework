//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "vulkan.hpp"
#include "system/os_common.h"
#include <array>
#include <cassert>

class Vulkan;

///
/// Classes and Helpers for requesting and registering Vulkan extensions
/// 


template<typename T_BASESTRUCTURE>
class ExtensionHook
{
public:
    using tBase = T_BASESTRUCTURE;

    virtual VkStructureType StructureType() const = 0;
    virtual VkBaseOutStructure* Obtain( tBase* ) = 0;
    virtual void Release(VkBaseOutStructure* pObtainedStructure) {};

    ExtensionHook<tBase>* pNext = nullptr;
};


// Template foo to determine if an structure contains a "void* pBase" member.
template <typename T, typename U = int>
struct Has_pNext : std::false_type { };
// Specialization for the true case
template <typename T>
struct Has_pNext <T, decltype((void)T::pNext, 0)> : std::true_type { };


// Template to define the entry point structure of an 'extension chain'
// Extensions can add hooks in to this chain so they are added into the pNext chain of the base vulkan structure and/or to hook in to common functionality (such as querying Vulkan extension properties).
//
template<typename T_BASESTRUCTURE>
class ExtensionChain
{
    ExtensionChain( const ExtensionChain& ) = delete;
    ExtensionChain& operator=( const ExtensionChain& ) = delete;

public:
    ExtensionChain() noexcept {}

    /// @brief 
    /// @param pBase pointer to 'base' class that these extensions extend
    /// @return number of extensions pushed/appended to the base
    size_t PushExtensions(T_BASESTRUCTURE* pBase)
    {
        size_t extensionsPushed = 0;
        VkBaseOutStructure* pPrev = (VkBaseOutStructure *)pBase;
        for (auto* pHook = pExtensionsHooks; pHook != nullptr; pHook = pHook->pNext)
        {
            VkBaseOutStructure* pExtensionStruct = pHook->Obtain( pBase );
            if (pExtensionStruct)   // nullptr denotes we are not hooking in to the pNext chain
            {
                if constexpr (Has_pNext<T_BASESTRUCTURE>::value)
                {
                    // Insert the obtained extension structure in to the next chain, make sure the chain is in the same order as the pExtensionsHooks (not reversed, which would be simpler) so that the PopExtensions iterates in the correct direction also
                    assert(pBase);
                    pExtensionStruct->pNext = (VkBaseOutStructure*)pPrev->pNext;
                    pPrev->pNext = pExtensionStruct;
                    pPrev = pExtensionStruct;
                    ++extensionsPushed;
                }
                else
                {
                    // Obtain should return nullptr for any hooks that do not insert themselves into a pNext chain!
                    assert(0);
                }
            }
        }
        return extensionsPushed;
    };

    /// @brief Remove pushed extensions from pNext chain of the given structure.
    /// Expects/demands that the extensions to be popped were the last things added to the given structure's pNext chain (and they havent been re-arranged)
    /// @param pBase 
    /// @return number of extensions popped/removed from the base
    size_t PopExtensions(T_BASESTRUCTURE* pBase)
    {
        assert(pBase);
        size_t extensionsPopped = 0;
        VkBaseOutStructure* pNext = (VkBaseOutStructure*) pBase->pNext;
        for (auto* pHook = pExtensionsHooks; pHook != nullptr && pNext != nullptr; pHook = pHook->pNext)
        {
            if (pHook->StructureType() == pNext->sType)
            {
                pHook->Release( pNext );
                pBase->pNext = pNext->pNext;
                pNext = (VkBaseOutStructure*) pBase->pNext;
                ++extensionsPopped;
            }
        }
        return extensionsPopped;
    }

    bool AddExtensionHook(ExtensionHook<T_BASESTRUCTURE>* pHook)
    {
        assert(pHook != nullptr);
        assert(pHook->pNext == nullptr);
        pHook->pNext = pExtensionsHooks;
        pExtensionsHooks = pHook;
        return true;
    }
    void RemoveExtensionHook(ExtensionHook<T_BASESTRUCTURE>* pHook)
    {
        assert(pHook != nullptr);
        if (pHook == pExtensionsHooks)
            pExtensionsHooks = pHook->pNext;
        else
        {
            auto* pCandidateHook = pExtensionsHooks;
            while (pCandidateHook != nullptr)
            {
                if (pCandidateHook->pNext == pHook)
                {
                    pCandidateHook->pNext = pHook->pNext;
                    pHook->pNext = nullptr;
                    break;
                }
                pCandidateHook = pCandidateHook->pNext;
            }
        }
    }
    ExtensionHook<T_BASESTRUCTURE>* pExtensionsHooks = nullptr;
};


/// @brief Enumeration of the Vulkan extension type (eg Device extension or Instance extansion).
enum class VulkanExtensionType { eDevice, eInstance, eLayer };

/// @brief Enumeration of the Vulkan extension status (eg loading state of an extension).
enum class VulkanExtensionStatus { eUninitialized, eOptional, eRequired, eLoaded };

/// @brief Enumeration for how extension is to be loaded.
/// eDefault - request extension from vulkan normally (to spec).  THIS IS THE OPTION TO USE ALMOST ALWAYS!
/// eForceLoad - ignore list of extensions reported by vulkan driver and request this extension anyways.  This is for developement/private driver extensions use at your own risk.
/// eSkipRequest - if not in the list of extensions reported by vulkan dont request this extension, but act as if it is enabled (do initialization and mark extension as loaded).  This is for developement/private driver extensions use at your own risk.
enum class VulkanExtensionLoadMode { eDefault, eForceLoad, eSkipRequest };

/// @brief Class describing a Vulkan extension base for defining extension behaviour and hooks into Vulkan calls
class VulkanExtensionBase
{
    VulkanExtensionBase(const VulkanExtensionBase&) = delete;             // Functionality built on VulkanExtension assumes this class remains fixed in memory
    VulkanExtensionBase& operator=(const VulkanExtensionBase&) = delete;
public:
    static constexpr std::array<const char* const, 4> cStatusNames{"Uninitialized", "Optional", "Required", "Loaded"};

    VulkanExtensionBase(std::string name, VulkanExtensionStatus status = VulkanExtensionStatus::eUninitialized, uint32_t version = 0) noexcept : Name(name), Status(status), Version(version) {}
    virtual ~VulkanExtensionBase() = default;

    const std::string                   Name;
    VulkanExtensionStatus               Status = VulkanExtensionStatus::eUninitialized;
    VulkanExtensionLoadMode             LoadMode = VulkanExtensionLoadMode::eDefault;
    uint32_t                            Version = 0;            ///< Extension version (from Driver)

    /// Register this extension with Vulkan. Typically will call Vulkan::AddExtensionHooks if the extension needs to hook in to any functionality.
    virtual void Register(Vulkan& vulkan) {/*does not have to be derived from (or do anything)*/ }
private:
};


/// @brief Class describing a Vulkan extension templated against the what the extension is extending (ie Instance or Device extsnion)
template<VulkanExtensionType T_TYPE>
class VulkanExtension : public VulkanExtensionBase
{
    VulkanExtension( const VulkanExtension& ) = delete;             // Functionality built on VulkanExtension assumes this class remains fixed in memory
    VulkanExtension& operator=( const VulkanExtension& ) = delete;
public:
    static const VulkanExtensionType Type = T_TYPE;

    VulkanExtension( std::string name, VulkanExtensionStatus status = VulkanExtensionStatus::eUninitialized, uint32_t version = 0 ) noexcept : VulkanExtensionBase(name, status, version ) {}
    virtual ~VulkanExtension() = default;

    virtual void PostLoad() {}      //< Function called after extension is loaded (eg after VkCreateDevice).  Can be overridden.
};
