//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "lightLoader.hpp"
#include "light.hpp"
#include "lightList.hpp"
#include "lightData.hpp"
#include "nlohmann/json.hpp"
#include "system/assetManager.hpp"
#include "system/glm_common.hpp"
#include "system/os_common.h"

namespace glm
{
    static void from_json(const Json& j, glm::vec3& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }
    static void from_json(const Json& j, glm::vec4& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
    static void to_json(Json& j, const glm::vec3& v) {
        j = Json({ v.x,v.y,v.z });
    }
    static void to_json(Json& j, const glm::vec4& v) {
        j = Json({ v.x,v.y,v.z,v.w });
    }
}

static void from_json(const Json& j, LightDataBase& lightData) {
    lightData = {};
    auto it = j.find("Brightness");
    if (it != j.end()) it->get_to(lightData.Intensity);
    it = j.find("Color");
    if (it != j.end())
        it->get_to(lightData.Color);
    else
        lightData.Color = glm::vec4(1.0f);
}

static void to_json(Json& j, const LightDataBase& lightData) {
    j = Json{ {"Brightness", lightData.Intensity}, { "Color", lightData.Color } };
}

static void from_json(const Json& j, PointLightData& pointData) {
    pointData = {};
    from_json(j, static_cast<LightDataBase&>(pointData));
    auto it = j.find("Position");
    if (it != j.end()) it->get_to(pointData.Position);
    it = j.find("Radius");
    if (it != j.end()) it->get_to(pointData.Radius);
    it = j.find("Range");
    if (it != j.end())
        it->get_to(pointData.Range);
    else
        pointData.Range = 0.0f;
}

static void to_json(Json& j, const PointLightData& pointData) {
    to_json(j, static_cast<const LightDataBase&>(pointData));
    j["Position"] = pointData.Position;
    j["Radius"] = pointData.Radius;
    if (pointData.Name && *pointData.Name)
        j["Name"] = pointData.Name;
}

static void from_json(const Json& j, SpotLightData& spotData) {
    spotData = {};
    from_json(j, static_cast<LightDataBase&>(spotData));
    auto it = j.find("Position");
    if (it != j.end()) it->get_to(spotData.Position);
    it = j.find("Direction");
    if (it != j.end()) it->get_to(spotData.Direction);
    it = j.find("Radius");
    if (it != j.end()) it->get_to(spotData.Radius);
    it = j.find("SpotAngle");
    if (it != j.end()) it->get_to(spotData.SpotAngle);
}

static void to_json(Json& j, const SpotLightData& spotData) {
    to_json(j, static_cast<const LightDataBase&>(spotData));
    j["Position"] = spotData.Position;
    j["Direction"] = spotData.Direction;
    j["Radius"] = spotData.Radius;
    j["SpotAngle"] = spotData.SpotAngle;
    if (spotData.Name && *spotData.Name)
        j["Name"] = spotData.Name;
}

static void from_json(const Json& j, DirectionalLightData& directionalData) {
    directionalData = {};
    from_json(j, static_cast<LightDataBase&>(directionalData));
    auto it = j.find("Direction");
    if (it != j.end()) it->get_to(directionalData.Direction);
    it = j.find("Position");
    if (it != j.end()) it->get_to(directionalData.Position);
}

static void to_json(Json& j, const DirectionalLightData& directionalData) {
    to_json(j, static_cast<const LightDataBase&>(directionalData));
    j["Direction"] = directionalData.Direction;
    j["Position"] = directionalData.Position;
}


std::optional<LightList> LightLoader::Load(AssetManager& assetManager, const std::string& filename, const float intensityCutoff)
{
    Json json;
    {
        std::vector<char> data;
        if (!assetManager.LoadFileIntoMemory(filename, data))
        {
            return std::nullopt;
        }
        json = Json::parse(data);
    }
    return std::make_optional<LightList>( Parse(json, intensityCutoff) );
}


void to_json(Json& json, const LightList& lightList)
{
    LightLoader::to_json_int(json, lightList);
}

void LightLoader::to_json_int(Json& json, const LightList& lightList)
{
    for (const auto& light : lightList.GetPointLights())
    {
        Json j;
        to_json(j, light.m_LightData);
        json["PointLights"].push_back(j);
    }

    for (const auto& light : lightList.GetSpotLights())
    {
        Json j;
        to_json(j, light.m_LightData);
        json["SpotLights"].push_back(j);
    }

    for (const auto& light : lightList.GetDirectionalLights())
    {
        Json j;
        to_json(j, light.m_LightData);
        json["DirectionalLights"].push_back(j);
    }
}

// Erase any light data members from a json array of lights that are unchanged from a json array containing the 'original' light objects.
// Used so we can save json data with just the changes to a set of lights.
// The 'Name' for each object is retained and assumed (required) to be a unique id.
// Ordering of the two arrays is also assumed to be identical.
static void EraseIdenticalLightData(Json& lightArray, const Json& originalLightArray)
{
    for (size_t lightIdx = 0; lightIdx < lightArray.size(); ++lightIdx)
    {
        if (originalLightArray.size() < lightIdx)
            break;
        const auto& originalLight = originalLightArray[lightIdx];
        auto& light = lightArray[lightIdx];
        if (!light.contains("Name") || light["Name"] == originalLight["Name"])
        {
            for (auto memberIt = light.begin(); memberIt != light.end(); )
            {
                // erase any values in the light that match the original EXCEPT 'Name' which we want to keep.
                if (memberIt.key()!="Name" && originalLight.contains(memberIt.key()) && originalLight[memberIt.key()] == memberIt.value())
                    memberIt = light.erase(memberIt);
                else
                    ++memberIt;
            }
        }
    }
}

/// @brief Merge differencesLightArray on top of lightArray
/// Expects data to be in same order between the two arrays.
static void MergeLightData(Json& lightArray, const Json& differencesLightArray)
{
    for (size_t lightIdx = 0; lightIdx < lightArray.size(); ++lightIdx)
    {
        if (lightIdx >= differencesLightArray.size())
            break;

        auto& light = lightArray[lightIdx];
        auto& differenceLight = differencesLightArray[lightIdx];
        if (!light.contains("Name") || light["Name"] == differenceLight["Name"])
        {
            for (auto memberIt = differenceLight.begin(); memberIt != differenceLight.end(); ++memberIt)
            {
                // Copy any differences over
                if ((memberIt.key().compare("Name") != 0) && light.contains(memberIt.key()))
                    light[memberIt.key()] = memberIt.value();
            }
        }
    }
}

void LightLoader::Save(Json &json, const LightList&lightList, const LightList*const originalLightList)
{
    json = Json(lightList);

    if (originalLightList)
    {
        // Find matching entries and remove them!
        Json jsonOriginal(*originalLightList);

        if (json.contains("DirectionalLights") && jsonOriginal.contains("DirectionalLights"))
        {
            const auto& originalLightArray = jsonOriginal["DirectionalLights"];
            auto& lightArray = json["DirectionalLights"];
            EraseIdenticalLightData(lightArray, originalLightArray);
        }

        if (json.contains("PointLights") && jsonOriginal.contains("PointLights"))
        {
            const auto& originalLightArray = jsonOriginal["PointLights"];
            auto& lightArray = json["PointLights"];
            EraseIdenticalLightData(lightArray, originalLightArray);
        }

        if (json.contains("SpotLights") && jsonOriginal.contains("SpotLights"))
        {
            const auto& originalLightArray = jsonOriginal["SpotLights"];
            auto& lightArray = json["SpotLights"];
            EraseIdenticalLightData(lightArray, originalLightArray);
        }
    }
}

LightList LightLoader::Parse(const Json& json, float intensityCutoff)
{
    std::vector< Light<PointLightData> > pointLights;
    std::vector< Light<SpotLightData> > spotLights;
    std::vector< Light<DirectionalLightData> > directionalLights;

    std::string combinedNames;

    for (const auto& el : json.items())
    {
        if (el.key().compare("PointLights") == 0)
        {
            // Array of 'PointLight" objects
            const auto& ar = el.value();
            pointLights.reserve(ar.size());
            for (const auto& item : ar)
            {
                pointLights.emplace_back(item.get<PointLightData>());
                if (item.contains("Name"))
                {
                    pointLights.back().m_LightData.Name = (const char*)(combinedNames.size());
                    combinedNames.append(item["Name"].get<std::string>());
                    combinedNames.push_back('\0');
                }
            }
        }
        else if (el.key().compare("SpotLights") == 0)
        {
            // Array of 'SpotLight" objects
            const auto& ar = el.value();
            spotLights.reserve(ar.size());
            for (const auto& item : ar)
            {
                spotLights.emplace_back(item.get<SpotLightData>());
                if (item.contains("Name"))
                {
                    spotLights.back().m_LightData.Name = (const char*)(combinedNames.size());
                    combinedNames.append(item["Name"].get<std::string>());
                    combinedNames.push_back('\0');
                }
            }
        }
        else if (el.key().compare("DirectionalLights") == 0)
        {
            // Array of 'DirectionalLight" objects
            const auto& ar = el.value();
            directionalLights.reserve(ar.size());
            for (const auto& item : ar)
            {
                directionalLights.emplace_back(item.get<DirectionalLightData>());
            }
        }
    }

    // Fix up all the string pointers now we have a 'final' string with all the names concatenated  (are curently just offsets into the final string)
    ptrdiff_t namePtrAdd = (ptrdiff_t)combinedNames.c_str();
    for (auto& point : pointLights)
        point.m_LightData.Name += namePtrAdd;
    for (auto& spot : spotLights)
        spot.m_LightData.Name += namePtrAdd;

    // For all lights without a range, calculate from the given cutoff.
    if (intensityCutoff > 0.0f)
    {
        for (auto& light : pointLights)
            if (light.m_LightData.Range == 0.0f)
                light.m_LightData.Range = sqrtf(light.m_LightData.Intensity / intensityCutoff);
        for (auto& light : spotLights)
            if (light.m_LightData.Range == 0.0f)
                light.m_LightData.Range = sqrtf(light.m_LightData.Intensity / intensityCutoff);
    }

    return{ std::move(pointLights), std::move(spotLights), std::move(directionalLights), std::move(combinedNames) };
}

void LightLoader::ParseDifferences(LightList& modifiedLightList, const Json& jsonDifferences, float intensityCutoff)
{
    Json jsonMerged(modifiedLightList);

    if (jsonDifferences.contains("PointLights") && jsonMerged.contains("PointLights"))
    {
        // merge points
        MergeLightData(jsonMerged["PointLights"], jsonDifferences["PointLights"]);
    }
    if (jsonDifferences.contains("SpotLights") && jsonMerged.contains("SpotLights"))
    {
        // merge spots
        MergeLightData(jsonMerged["SpotLights"], jsonDifferences["SpotLights"]);
    }
    if (jsonDifferences.contains("DirectionalLights") && jsonMerged.contains("DirectionalLights"))
    {
        // merge directionals
        MergeLightData(jsonMerged["DirectionalLights"], jsonDifferences["DirectionalLights"]);
    }

    LightList newLightList = Parse(jsonMerged, intensityCutoff);

    for (size_t lightIdx = 0; lightIdx < modifiedLightList.GetDirectionalLights().size(); ++lightIdx)
    {
        auto& modifiedLight = modifiedLightList.GetDirectionalLights()[lightIdx];
        auto nodeId = modifiedLight.GetNodeId();
        modifiedLight = newLightList.GetDirectionalLights()[lightIdx];
        modifiedLight.m_LightData.NodeId = nodeId;
    }
    for (size_t lightIdx = 0; lightIdx < modifiedLightList.GetPointLights().size(); ++lightIdx)
    {
        auto& modifiedLight = modifiedLightList.GetPointLights()[lightIdx];
        const char* pName = modifiedLight.GetName();
        auto nodeId = modifiedLight.GetNodeId();
        modifiedLight = newLightList.GetPointLights()[lightIdx];
        modifiedLight.m_LightData.Name = pName;
        modifiedLight.m_LightData.NodeId = nodeId;
    }
    for (size_t lightIdx = 0; lightIdx < modifiedLightList.GetSpotLights().size(); ++lightIdx)
    {
        auto& modifiedLight = modifiedLightList.GetSpotLights()[lightIdx];
        const char* pName = modifiedLight.GetName();
        auto nodeId = modifiedLight.GetNodeId();
        modifiedLight = newLightList.GetSpotLights()[lightIdx];
        modifiedLight.m_LightData.Name = pName;
        modifiedLight.m_LightData.NodeId = nodeId;
    }

    // For all lights without a range, calculate from the given cutoff.
    if (intensityCutoff > 0.0f)
    {
        for (auto& light : modifiedLightList.GetPointLights())
            if (light.m_LightData.Range == 0.0f)
                light.m_LightData.Range = sqrtf(light.m_LightData.Intensity / intensityCutoff);
        for (auto& light : modifiedLightList.GetSpotLights())
            if (light.m_LightData.Range == 0.0f)
                light.m_LightData.Range = sqrtf(light.m_LightData.Intensity / intensityCutoff);
    }
}

