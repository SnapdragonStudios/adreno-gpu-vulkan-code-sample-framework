//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "lightList.hpp"
#include "light.hpp"

LightList::LightList() noexcept
{}

LightList::LightList(LightList&&) noexcept = default;

LightList& LightList::operator=(LightList&&) noexcept = default;

LightList::~LightList()
{}

LightList::LightList(std::vector< Light<PointLightData> >&& p, std::vector< Light<SpotLightData> >&& s, std::vector< Light<DirectionalLightData> >&& d, std::string && nameStrings) noexcept
    : m_PointLights(std::move(p))
    , m_SpotLights(std::move(s))
    , m_DirectionalLights(std::move(d))
    , m_NameStrings(std::move(nameStrings))
{
}

LightList LightList::Copy() const
{
    std::string nameStringCopy = m_NameStrings;
    // strings in copy need pointing to the new location...
    ptrdiff_t namePtrDiff = nameStringCopy.c_str() - m_NameStrings.c_str();

    std::vector< Light<PointLightData> > pointlightCopy;
    pointlightCopy.reserve(m_PointLights.size());
    for (const auto& light : m_PointLights)
    {
        pointlightCopy.emplace_back(light.m_LightData);
        pointlightCopy.back().m_LightData.Name += namePtrDiff;
    }
    std::vector< Light<SpotLightData> > spotlightCopy;
    spotlightCopy.reserve(m_SpotLights.size());
    for (const auto& light : m_SpotLights)
    {
        spotlightCopy.emplace_back(light.m_LightData);
        spotlightCopy.back().m_LightData.Name += namePtrDiff;
    }
    std::vector< Light<DirectionalLightData> > directionalCopy;
    directionalCopy.reserve(m_DirectionalLights.size());
    for (const auto& light : m_DirectionalLights)
    {
        directionalCopy.emplace_back(light.m_LightData);
    }

    return { std::move(pointlightCopy), std::move(spotlightCopy), std::move(directionalCopy), std::move(nameStringCopy) };
}