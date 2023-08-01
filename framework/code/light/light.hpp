//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "lightData.hpp"

/// @brief Templated light class
/// @ingroup Light
/// Contains a light data type and provides accessors to that data (template allows for a consistant interface between the different data types)
template<typename T_LIGHTDATA>
class Light
{
    Light(const Light&) = delete;
public:
    Light(const T_LIGHTDATA& d) : m_LightData(d) {}
    Light(Light&& light) = default;
    Light& operator=(const Light&) = default;

    const auto& GetPosition() const     { return m_LightData.Position; }
    const auto& GetDirection() const    { return m_LightData.Direction; }   ///< normalized direction of spotlight
    const auto& GetSpotAngle() const    { return m_LightData.SpotAngle; }   ///< outer cone angle (center to edge) in radians
    const auto& GetEmissionRadius()const{ return m_LightData.Radius; }
    const auto& GetColor() const        { return m_LightData.Color; }
    const auto& GetRange() const        { return m_LightData.Range; }
    const auto& GetIntensity() const    { return m_LightData.Intensity; }
    const auto& GetName() const         { return m_LightData.Name; }
    const auto& GetNodeId() const       { return m_LightData.NodeId; }

    void SetPosition(const glm::vec3 pos)   { m_LightData.Position = pos; }
    void SetDirection(const glm::vec3 dir)  { m_LightData.Direction = dir; }

protected:
    friend class LightLoader;
    friend class LightGltfProcessor;
    friend class LightList;
    T_LIGHTDATA m_LightData;
};
