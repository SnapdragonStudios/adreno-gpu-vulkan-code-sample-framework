//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cassert>
#include <map>
#include <string>
#include "descriptorSetDescription.hpp"


/// Base class for the descriptor set layout.
/// Graphics api independent.
/// @ingroup Material
class DescriptorSetLayoutBase
{
    DescriptorSetLayoutBase& operator=(const DescriptorSetLayoutBase&) = delete;
    DescriptorSetLayoutBase(const DescriptorSetLayoutBase&) = delete;
protected:
    DescriptorSetLayoutBase() noexcept;
    ~DescriptorSetLayoutBase();
    DescriptorSetLayoutBase(DescriptorSetLayoutBase && other) noexcept
        : m_nameToBinding(std::move(other.m_nameToBinding))
    {}    

    bool Init(const DescriptorSetDescription&);
public:
    const auto& GetNameToBinding() const { return m_nameToBinding; }

    struct BindingTypeAndIndex {
        DescriptorSetDescription::DescriptorType type;
        uint32_t                index;          ///< index within the descriptor set
        bool                    isArray;
        bool                    isReadOnly;
    };
    struct DescriptorBinding {
        uint32_t                setIndex;       ///< descriptor set index
        BindingTypeAndIndex     setBinding;     ///< binding within the descriptor set
    };

private:
    std::map<std::string, BindingTypeAndIndex> m_nameToBinding; ///< For each named descriptor slot store the relevant binding index (one name per binding for simplicity) ///< @TODO should this be in here, or in the materialPass.
};


struct DescriptorTypeAndLocation {
    DescriptorTypeAndLocation(uint32_t _setIndex, DescriptorSetLayoutBase::BindingTypeAndIndex _typeAndIndex)
        : setIndex(_setIndex)
        , index(_typeAndIndex.index)
        , isArray(_typeAndIndex.isArray)
        , isReadOnly(_typeAndIndex.isReadOnly)
    {}
    uint32_t setIndex;  ///< index of descriptor set / table
    uint32_t index;     ///< index of descriptor within set / table
    bool isArray;       ///< is descriptor an array
    bool isReadOnly;    ///< is descriptor readonly
};


inline DescriptorSetLayoutBase::DescriptorSetLayoutBase() noexcept
{
}

inline DescriptorSetLayoutBase::~DescriptorSetLayoutBase()
{
}


inline bool DescriptorSetLayoutBase::Init(const DescriptorSetDescription& description)
{
    uint32_t index = 0;
    for (const auto& it : description.m_descriptorTypes)
    {
        BindingTypeAndIndex bindingTypeAndIndex {};
        bindingTypeAndIndex.isReadOnly = it.readOnly;
        bindingTypeAndIndex.isArray = it.count!=1;
        bindingTypeAndIndex.type = it.type;

        if (it.descriptorIndex < 0)
            // Index of < 0 denotes we want to use sequential descriptor binding indices.
            ///TODO: look for collisions or determine how/if we want to handle out of order desciptor indices or enforce shaders that have an explicit binding index to define indices for all descriptors.
            bindingTypeAndIndex.index = index++;
        else
            bindingTypeAndIndex.index = it.descriptorIndex;

        assert(it.names.size() <= 1);   ///TODO: only one name supported, needs to store the index within the descriptor as well as the binding index if we want to support this! (the 'for' loop below is not the full implementation)
        for (const auto& name : it.names)
        {
            auto nameToBindingEmplaced = m_nameToBinding.try_emplace(name, bindingTypeAndIndex);
            assert(nameToBindingEmplaced.second); // name must be unique
        }
    }
    return true;
}


/// Templated (by graphics API) derived version of DescriptorSetLayoutBase
/// Graphics api may want to specialize this.
template<typename T_GFXAPI>
class DescriptorSetLayout : public DescriptorSetLayoutBase
{
public:
    DescriptorSetLayout() noexcept = delete;                                               // Expecting that this template be specialized
    ~DescriptorSetLayout() = delete;                                                       // Expecting that this template be specialized
    DescriptorSetLayout(DescriptorSetLayout<T_GFXAPI>&&) noexcept = delete;               // Expecting that this template be specialized

    static_assert(sizeof(DescriptorSetLayout<T_GFXAPI>) != sizeof(DescriptorSetLayoutBase));   // Expecting that this template be specialized
};
