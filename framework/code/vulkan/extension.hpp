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
#include <cassert>

class Vulkan;

///
/// Classes and Helpers for requesting and registering Vulkan extensions
/// 


template<typename T_BASESTRUCTURE>
class ExtensionHook
{
public:
    typedef T_BASESTRUCTURE tBase;

    virtual VkStructureType StructureType() const = 0;
    virtual VkBaseOutStructure* Obtain( tBase* ) = 0;
    void Release(VkBaseOutStructure* pObtainedStructure) {};

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
        for (auto* pHook = pExtensionsHooks; pHook != nullptr; pHook = pHook->pNext)
        {
            if (pHook->StructureType() == pNext->sType)
            {
                pHook->Release(pNext);
                pBase->pNext = pNext->pNext;
                pNext = (VkBaseOutStructure*)pBase->pNext;
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


/// @brief Class describing a Vulkan extension and base class for defining extension behaviour and hooks into Vulkan calls
class VulkanExtension
{
    VulkanExtension( const VulkanExtension& ) = delete;             // Functionality built on VulkanExtension assumes this class remains fixed in memory
    VulkanExtension& operator=( const VulkanExtension& ) = delete;
public:
    enum eStatus { eUninitialized, eOptional, eRequired, eLoaded } Status = eUninitialized;

    VulkanExtension(std::string name, VulkanExtension::eStatus status = eUninitialized, uint32_t version = 0) noexcept : Name(name), Status(status), Version(version) {}
    const std::string   Name; 
    uint32_t            Version;            // Extension version (from Driver)

    virtual void Register(Vulkan& vulkan) {}
};


