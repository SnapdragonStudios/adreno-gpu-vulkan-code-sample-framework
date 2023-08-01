//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "lightListGui.hpp"
#include "light.hpp"
#include "lightLoader.hpp"
#include "imgui/imgui.h"

LightListGui::LightListGui(const LightList& original) : m_LightListOriginal( original.Copy() )
{
}

bool LightListGui::UpdateGui(LightList& lightList)
{
    bool modified = false;

    for(size_t lightIdx = 0;lightIdx< lightList.GetDirectionalLights().size(); ++lightIdx)
    {
        const auto& light = lightList.GetDirectionalLights()[lightIdx];

        bool open = ImGui::CollapsingHeader("Directional light", ImGuiTreeNodeFlags_AllowItemOverlap);
        ImGui::SameLine();
        if (ImGui::Button("Restore from GLTF"))
        {
            lightList.GetDirectionalLights()[lightIdx] = m_LightListOriginal.GetDirectionalLights()[lightIdx];
            modified = true;
        }
        if (open)
        {
            modified |= ImGui::ColorEdit3("Color", const_cast<float*>(&light.GetColor().x), ImGuiColorEditFlags_NoAlpha);
            modified |= ImGui::SliderFloat("Intensity", const_cast<float*>(&light.GetIntensity()), 0.0f, 100.0f);
            modified |= ImGui::InputFloat3("Direction", const_cast<float*>(&light.GetDirection().x), "%.2f");
        }
    }
    if (!lightList.GetPointLights().empty())
    {
        if (ImGui::CollapsingHeader("Point lights"))
        {
            for (size_t lightIdx = 0; lightIdx < lightList.GetPointLights().size(); ++lightIdx)
            {
                const auto& light = lightList.GetPointLights()[lightIdx];

                ImGui::PushID((void*)&light);
                modified |= ImGui::ColorEdit3("Color", const_cast<float*>(&light.GetColor().x), ImGuiColorEditFlags_NoAlpha);
                modified |= ImGui::SliderFloat("Intensity", const_cast<float*>(&light.GetIntensity()), 0.0f, 30.0f);
                ImGui::PopID();
            }
        }
    }
    if (!lightList.GetSpotLights().empty())
    {
        if (ImGui::CollapsingHeader("Spot lights"))
        {
            for (size_t lightIdx = 0; lightIdx < lightList.GetSpotLights().size(); ++lightIdx)
            {
                const auto& light = lightList.GetSpotLights()[lightIdx];

                ImGui::PushID((void*)&light);
                bool open = ImGui::CollapsingHeader(light.GetName() ? light.GetName() : "unnamed", ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen);
                if (ImGui::SameLine(), ImGui::Button("Restore from GLTF"))
                {
                    lightList.GetSpotLights()[lightIdx] = m_LightListOriginal.GetSpotLights()[lightIdx];
                    modified = true;
                }
                if (open)
                {
                    ImGui::ColorEdit3("Color", const_cast<float*>(&light.GetColor().x), ImGuiColorEditFlags_NoAlpha);
                    modified |= ImGui::SliderFloat("Intensity", const_cast<float*>(&light.GetIntensity()), 0.0f, 30.0f);
                    float spotAngleDegrees = 57.2958f * light.GetSpotAngle();
                    if (ImGui::SliderFloat("Angle", &spotAngleDegrees, 0.0f, 45.0f))
                    {
                        float* pAngleRad = const_cast<float*>(&light.GetSpotAngle());
                        *pAngleRad = spotAngleDegrees * (1.0f / 57.2958f);
                        modified = true;
                    }
                }
                ImGui::PopID();
            }
        }
    }
    return modified;
}
