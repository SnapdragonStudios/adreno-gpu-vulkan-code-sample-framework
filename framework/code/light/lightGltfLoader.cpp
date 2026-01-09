//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "lightGltfLoader.hpp"
#include "light.hpp"
#include "lightList.hpp"
#include "lightData.hpp"
#include "mesh/meshLoader.hpp"
#include <sstream>

constexpr float cfWattsToCandela = 683.0f / (4.0f * glm::pi<float>());

// Blender outputs light power to gltf in Watts (which is wrong).  gltf spec requests pointlight should be in Candela (lm / sr)
static float WattsToCandela(float W)
{
    return W * cfWattsToCandela;
}

// Change out of range (HDR) colors to 0-1 range and adjust intensity to compensate.
static void FixColorRange(glm::vec3& color, float& intensity)
{
    // limit colors to 0-1 range (3dsMax seems to export out of range) and apply any scale to the intensity instead
    float maxScale = glm::max(glm::max(color[0], color[1]), color[2]);
    float intensityScale = std::max(maxScale, 1.0f);
    color /= intensityScale;
    intensity *= intensityScale;
}

//if not blender: watts = gltf_intensity * 4 * pi / 638/*kv*/;
//            watts = gltf_intensity * 4 * pi * pow(sin(half cone angle), 2) / 638/*kv*/
//if blender: watts = intensity (incorrect correct value expoter from blender) (for spot and point)

bool LightGltfProcessor::operator()(const tinygltf::Model& ModelData)
{
    m_lightList = std::make_unique<LightList>();

    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    std::vector<Light<PointLightData>> pointLights;
    std::vector<Light<SpotLightData>> spotLights;
    std::vector<Light<DirectionalLightData>> directionalLights;

    std::ostringstream nameStrings;

    const float defaultIntensityCutoff = m_defaultIntensityCutoff;

    // Go through all the scene nodes, grabbing all the lights
    bool success = MeshLoader::RecurseModelNodes(ModelData, SceneData.nodes, [this, &pointLights, &spotLights, &directionalLights, &nameStrings, defaultIntensityCutoff](const tinygltf::Model& ModelData, const MeshLoader::NodeTransform& Transform, const tinygltf::Node& NodeData) -> bool {

        const static std::string khrLightsExtensionName("KHR_lights_punctual");
        const static std::string pointlightTypeName("point");
        const static std::string spotlightTypeName("spot");
        const static std::string directionalLightTypeName("directional");

        const auto it = NodeData.extensions.find(khrLightsExtensionName);
        if (it != NodeData.extensions.end())
        {
            const int lightIdx = it->second.Get("light").Get<int>();
            const tinygltf::Light light = ModelData.lights[lightIdx];

            size_t NodeIdx = &NodeData - &ModelData.nodes[0];
            if (NodeIdx < 0 || NodeIdx >= ModelData.nodes.size())
                NodeIdx = -1;

            if (light.type == pointlightTypeName)
            {
                PointLightData pointLight{};
                pointLight.Position = { Transform.Matrix[3][0], Transform.Matrix[3][1], Transform.Matrix[3][2] };
                pointLight.Intensity = (float)light.intensity;      // intensity in candela
                if (light.color.size() >= 3)
                {
                    pointLight.Color = { light.color[0], light.color[1], light.color[2] };
                    FixColorRange( pointLight.Color, pointLight.Intensity );
                }
                else
                    pointLight.Color = glm::vec3(1.0f);
                pointLight.Radius = 0.1f;   // this is wrong!  light intensity is defined as being infinite at radius 0... gltf doesnt have a 'radius' defined for pointlights or spotlights

                //pointLight.Range = (float)light.range;
                //if (pointLight.Range == 0.0f)
                // Always override the range saved to the gltf... to change this we need so save the cutoff value per-light (and calculate cutoff for lights where the range is taken from the gltf)
                pointLight.Range = sqrtf(pointLight.Intensity / defaultIntensityCutoff);
                pointLight.NodeId = (int)NodeIdx;
                if (!NodeData.name.empty())
                {
                    pointLight.Name = (const char*)nullptr + nameStrings.tellp();
                    nameStrings.write(NodeData.name.c_str(), NodeData.name.size() + 1/*includes terminator*/);
                }
                pointLights.emplace_back(pointLight);
            }
            else if (light.type == spotlightTypeName)
            {
                SpotLightData spotLight{};
                spotLight.Position = { Transform.Matrix[3][0], Transform.Matrix[3][1], Transform.Matrix[3][2] };
                spotLight.Intensity = (float)light.intensity;       // intensity in candela
                if (light.color.size() >= 3)
                {
                    spotLight.Color = { light.color[0], light.color[1], light.color[2] };
                    FixColorRange(spotLight.Color, spotLight.Intensity);
                }
                else
                    spotLight.Color = glm::vec3(1.0f);
                spotLight.Radius = 0.1f;   // this is wrong!  light intensity is defined as being infinite at radius 0... gltf doesnt have a 'radius' defined for pointlights or spotlights
                //spotLight.Range = (float)light.range;
                //if (spotLight.Range == 0.0f)
                // Always override the range saved to the gltf... to change this we need so save the cutoff value per-light (and calculate cutoff for lights where the range is taken from the gltf)
                spotLight.Range = sqrtf(spotLight.Intensity / defaultIntensityCutoff) * 0.5f / powf(cosf((float)light.spot.outerConeAngle), 2.0f);
                spotLight.SpotAngle = (float)light.spot.outerConeAngle;    // in radians
                spotLight.Direction = -glm::normalize(glm::vec3{ Transform.Matrix[2][0], Transform.Matrix[2][1],Transform.Matrix[2][2] });// light is along direction of -z axis
                spotLight.NodeId = (int)NodeIdx;
                if (!NodeData.name.empty())
                {
                    spotLight.Name = (const char*)nullptr + nameStrings.tellp();
                    nameStrings.write(NodeData.name.c_str(), NodeData.name.size() + 1/*includes terminator*/);
                }
                spotLights.emplace_back(spotLight);
            }
            else if (light.type == directionalLightTypeName && directionalLights.size() < m_maxDirectionals)
            {
                DirectionalLightData directionalLight{};
                directionalLight.Position = { Transform.Matrix[3][0], Transform.Matrix[3][1], Transform.Matrix[3][2] };
                if (light.color.size() >= 3)
                {
                    directionalLight.Color = { light.color[0], light.color[1], light.color[2] };
                    FixColorRange(directionalLight.Color, directionalLight.Intensity);
                }
                else
                    directionalLight.Color = glm::vec3(1.0f);
                directionalLight.Intensity = (float)light.intensity;    // intensity in lux
                directionalLight.Direction = -glm::normalize(glm::vec3{ Transform.Matrix[2][0], Transform.Matrix[2][1],Transform.Matrix[2][2] });// light is along direction of -z axis
                directionalLights.emplace_back(directionalLight);
            }
        }
        return true;
        });

    if (!success)
        return false;

    // Fix up the string pointers now all the names are in nameStrings (and the underlying string-stream memory will no longer shift around).
    std::string nameStringsStr = nameStrings.str();
    const char* pStringsStart = nameStringsStr.c_str();
    for (auto& pointLight : pointLights)
        pointLight.m_LightData.Name = pStringsStart + (size_t)pointLight.m_LightData.Name;
    for (auto& spotLight : spotLights)
        spotLight.m_LightData.Name = pStringsStart + (size_t)spotLight.m_LightData.Name;

    // Move the completed light lists into the LightList container
    m_lightList = std::make_unique<LightList>(std::move(pointLights), std::move(spotLights), std::move(directionalLights), std::move(nameStringsStr));
    return true;
}
