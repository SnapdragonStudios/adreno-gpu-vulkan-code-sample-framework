//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <optional>
#include <string>
#include "json/include/nlohmann/json_fwd.hpp"

// forward declarations
class AssetManager;
class LightList;
struct SpotLightData;

using Json = nlohmann::json;

/// @brief Loads lights from a light json file (described by lightSchema.json)
/// @ingroup Light
class LightLoader
{
    LightLoader(const LightLoader&) = delete;
    LightLoader& operator=(const LightLoader&) = delete;
public:
    LightLoader() {}

    /// @brief Load a LightList from the given light json file
    /// @param filename light json filename (including extension)
    /// @param intensityCutoff minimum intensity (used to calculate range for lights where range is not specified).
    std::optional<LightList> Load(AssetManager& assetManager, const std::string& filename, const float intensityCutoff);

    /// @brief Save a LightList to a json object
    /// @param json object that will be filled (cleared and then filled) with the lightlist data
    /// @param lightList lights to save
    /// @param originalLightList (optional) lights to difference against and only save differences (requires list members to match exactly)
    void Save(Json& json, const LightList& lightList, const LightList* const originalLightList);

    void ParseDifferences(LightList& modifiedLightList, const Json& jsonDifferences, float intensityCutoff);

private:
    /// @brief Parse the light list data out of the given Json.
    /// Used internally in the Load functions.
    LightList Parse(const Json&, float intensityCutoff);

    friend void to_json(Json& json, const LightList& lightList);
    static void to_json_int(Json& json, const LightList& lightList);
};
