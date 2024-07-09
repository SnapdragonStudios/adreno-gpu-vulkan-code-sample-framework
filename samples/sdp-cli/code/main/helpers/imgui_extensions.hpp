//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include <span>

namespace ImGui
{
	IMGUI_API bool RangeSliderFloat(const char* label, float* v1, float* v2, float v_min, float v_max, const char* display_format = "(%.3f, %.3f)", float power = 1.0f);

} // namespace ImGui

namespace ImPlot
{
    // double (*PlotTooltipGetter)(const PlotTooltipEntry& entry, int idx, void* user_data)
    
    template <typename PlotTooltipEntry, typename PlotTooltipGetter>
    int BinarySearch(const PlotTooltipEntry& entry, PlotTooltipGetter entry_value_getter, void* user_data, int l, int r, double x, double epsilon) 
    {
        if (r >= l) {
            int mid = l + (r - l) / 2;
            if (std::fabs(entry_value_getter(entry, mid, user_data) - x) < epsilon)
                return mid;
            if (entry_value_getter(entry, mid, user_data) > x)
                return BinarySearch(entry, entry_value_getter, user_data, l, mid - 1, x, epsilon);
            return BinarySearch(entry, entry_value_getter, user_data, mid + 1, r, x, epsilon);
        }
        return -1;
    }

    template <typename PlotTooltipEntryGroup, typename PlotTooltipGetter>
    void PlotTooltip(
        const char*           label_id, 
        PlotTooltipEntryGroup entry_groups,
        PlotTooltipGetter     entry_value_getter, 
        int                   value_count, 
        double                epsilon,
        void*                 user_data     = nullptr, 
        float                 width_percent = 0.25f)
    {
        if (entry_groups.empty())
        {
            return;
        }

        const auto& reference_entry = entry_groups.back();

        // get ImGui window DrawList
        ImDrawList* draw_list = ImPlot::GetPlotDrawList();

        // calc real value width
        double half_width = value_count > 1 ? (entry_value_getter(reference_entry, 1, user_data) - entry_value_getter(reference_entry, 0, user_data)) * width_percent : width_percent;

        // custom tool
        if (ImPlot::IsPlotHovered()) 
        {
            ImPlotPoint mouse   = ImPlot::GetPlotMousePos();
            // mouse.x             = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x), ImPlotTimeUnit_Day).ToDouble();
            float  tool_l       = ImPlot::PlotToPixels(mouse.x - half_width * 1.5, mouse.y).x;
            float  tool_r       = ImPlot::PlotToPixels(mouse.x + half_width * 1.5, mouse.y).x;
            float  tool_t       = ImPlot::GetPlotPos().y;
            float  tool_b       = tool_t + ImPlot::GetPlotSize().y;
            ImPlot::PushPlotClipRect();
            draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(128,128,128,64));
            ImPlot::PopPlotClipRect();
            // find mouse location index

            int idx = BinarySearch(reference_entry, entry_value_getter, user_data, 0, value_count - 1, mouse.x, epsilon);
            // render tool tip (won't be affected by plot clip rect)
            if (idx != -1) 
            {
                ImGui::BeginTooltip();

                for (auto& entry : entry_groups)
                {
                    const auto value = entry_value_getter(entry, idx, user_data);

                    ImGui::Text("Day:   $%.2f", static_cast<float>(value));
                }

#if 0                
                char buff[32];
                ImPlot::FormatDate(ImPlotTime::FromDouble(xs[idx]),buff,32,ImPlotDateFmt_DayMoYr,ImPlot::GetStyle().UseISO8601);
                ImGui::Text("Day:   %s",  buff);
                ImGui::Text("Open:  $%.2f", opens[idx]);
                ImGui::Text("Close: $%.2f", closes[idx]);
                ImGui::Text("Low:   $%.2f", lows[idx]);
                ImGui::Text("High:  $%.2f", highs[idx]);
#endif
                ImGui::EndTooltip();
            }
        }
    }


} // namespace ImPlot