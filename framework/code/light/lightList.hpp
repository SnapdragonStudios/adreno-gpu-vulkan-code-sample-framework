//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <vector>
#include <string>

template<typename T> class Light;
struct PointLightData;
struct SpotLightData;
struct DirectionalLightData;

/// Container for lists of lights (in each supported light type)
/// @ingroup Light
class LightList
{
    LightList& operator=(const LightList&) = delete;
public:
    LightList() noexcept;
    LightList(std::vector< Light<PointLightData> >&&, std::vector< Light<SpotLightData> >&&, std::vector< Light<DirectionalLightData> >&&, std::string&& nameStrings) noexcept;
    LightList(const LightList&) noexcept;
    LightList(LightList&&) noexcept;
    LightList& operator=(LightList&&) noexcept;
    ~LightList();

    LightList Copy() const; ///< Return a copy of this object.  Use this rather than implementing copy constructor/operator which may be called un-intentionally.

    const auto& GetPointLights() const          { return m_PointLights; }
    const auto& GetSpotLights() const           { return m_SpotLights; }
    const auto& GetDirectionalLights() const    { return m_DirectionalLights; }

    auto& GetPointLights()                      { return m_PointLights; }
    auto& GetSpotLights()                       { return m_SpotLights; }
    auto& GetDirectionalLights()                { return m_DirectionalLights; }

    bool empty() const { return m_PointLights.empty() && m_SpotLights.empty() && m_DirectionalLights.empty(); }

protected:
    std::vector<Light<PointLightData> >         m_PointLights;
    std::vector<Light<SpotLightData> >          m_SpotLights;
    std::vector<Light<DirectionalLightData>>    m_DirectionalLights;
    std::string                                 m_NameStrings;          // Contains all the names pointed to by various lights, single string but contains '\0' delimiters splitting each name.
};
