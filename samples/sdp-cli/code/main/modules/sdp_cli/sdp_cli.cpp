//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "sdp_cli.hpp"
#include "../../helpers/console_common.hpp"
#include "../../helpers/imgui_extensions.hpp"
#include "imgui.h"
#include "implot/implot.h"
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    constexpr const char* DRAG_DROP_SECTION_NAME = "DRAG_DROP_SECTION";
    constexpr const char* SDP_CLI_DEVICE_PATH    = "/data/local/tmp/sdpcli/sdpcli";

    uint64_t ParseNumber(const std::string& input) 
    {
        try 
        {
            size_t idx;
            uint64_t result = std::stoul(input, &idx, 10);

            if (idx < input.size())
            {
                char suffix = input[idx];
                switch (suffix) 
                {
                    case 'k':
                        result *= 1000;
                        break;
                    case 'M':
                        result *= 1000000;
                        break;
                    // Add more cases for other suffixes if needed
                    default:
                        throw std::invalid_argument("Invalid suffix: " + std::string(1, suffix));
                }
            }
            return result;

        } 
        catch (...) 
        {
            return 0;
        }
    }

    template <typename _Ty>
    _Ty ExtractValueFromThermalInfo(const std::string& input, size_t N) 
    {
        // Find the position of the first occurrence of '|'
        size_t start_pos = input.find('|');
        if (start_pos == std::string::npos) 
        {
            return _Ty(0);
        }

        // Extract the substring after the first '|'
        std::string values_str = input.substr(start_pos + 1);
        std::replace(values_str.begin(), values_str.end(), '|', ' ');

        // Create a stringstream to tokenize the values
        std::istringstream iss(values_str);
        std::string token;
        std::vector<float> values;

        // Tokenize the values using space as delimiter
        while (std::getline(iss, token, ' ')) 
        {
            // Convert the token to an integer (or 0 if NA is encountered)
            if (token.find("NA") != std::string::npos) 
            {
                values.push_back(0);
            } 
            else if (token.empty()) 
            {
                continue;
            } 
            else 
            {
                try 
                {
                    values.push_back(std::stof(token));
                } 
                catch (const std::invalid_argument&) 
                {
                    return _Ty(0);
                }
            }
        }

        // Check if N is within valid range
        if (N >= values.size()) 
        {
            return _Ty(0);
        }

        // Return the Nth value
        return static_cast<_Ty>(values[N]);
    }
}

SDP::SDP(std::string device_name)
    : ModuleInterface(std::move(device_name))
{
    std::memset(m_sdp_cli_path.data(), 0, m_sdp_cli_path.size());
    std::memset(m_profilling_layer_path.data(), 0, m_profilling_layer_path.size());

    // See if the CLI is already present on the device so we can skip up setup phase
    m_has_sdp_cli = DoesDeviceFileExist(m_device_name, SDP_CLI_DEVICE_PATH);
}

SDP::~SDP()
{
}

const char* SDP::GetModuleName()
{
    return "SDP CLI";
}

bool SDP::Initialize() 
{
    RefreshAsyncUpdateThread();

    return true;
}

void SDP::RemoveToolFromDevice()
{
    RunADBCommandForDevice(
        m_device_name,
        std::string("shell rm ")
        .append(SDP_CLI_DEVICE_PATH));
}

void SDP::Reset()
{
    ModuleInterface::Reset();

    std::lock_guard l(m_safety);

    for (auto& [category, map] : m_realtime_options_by_category)
    {
        for (auto& [key, entry] : map)
        {
            entry.value.Reset(true);
        }
    }

    FixDisabledGroupEntries();
}

void SDP::Pause()
{
    ModuleInterface::Pause();
}

void SDP::Stop()
{
    ModuleInterface::Stop();
}

void SDP::Record()
{
    ModuleInterface::Record();
}

void SDP::Resume()
{
    ModuleInterface::Resume();
}

void SDP::Update(float time_elapsed)
{
    ModuleInterface::Update(time_elapsed);
}

void SDP::Draw(float time_elapsed)
{
    ModuleInterface::Draw(time_elapsed);

    if (!m_has_sdp_cli)
    {
        DrawSetupSection(time_elapsed);
        return;
    }

    if (!m_did_setup_supported_options && m_has_sdp_cli)
    {
        SetupRuntimeOptions();
        RefreshAsyncUpdateThread();
        m_did_setup_supported_options = true;
    }

    ImGui::SetNextItemWidth(120);
    const auto previous_sampling_period_ms = m_sampling_period_ms;
    if(ImGui::DragInt("Sampling Period", &m_sampling_period_ms, 100.0f, 100, 1000))
    {
        RefreshAsyncUpdateThread(previous_sampling_period_ms);
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    if (ImGui::Button("Reset"))
    {
        Reset();
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("SDP CLI Module")) 
    {
#if 0
        if (ImGui::BeginTabItem("General")) 
        {
            DrawGeneralSection(time_elapsed);
            ImGui::EndTabItem();
        }
#endif

        if (ImGui::BeginTabItem("Runtime")) 
        {
            DrawPlaygroundSection(time_elapsed);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void SDP::FixDisabledGroupEntries()
{
    /*
    * Here we orphan entries that are part of a group (2+ entries) but aren't selected in
    * the options
    * We basically detect such entries, add into the oprhan vector below and re-add them
    * as their own separated groups, as if a merge never happened
    */

    std::pmr::vector<int> orphan_entries(&m_memory_resource);

    for (auto& ordered_group : m_realtime_group_options_ordered)
    {
        for (int i = ordered_group.size()-1; i>=0; i--)
        {
            auto ordered_key = ordered_group[i];

            for (auto& [category, map] : m_realtime_options_by_category)
            {
                auto iter = map.find(ordered_key);
                if (iter == map.end())
                {
                    continue;
                }

                if (!iter->second.selected && ordered_group.size() > 1)
                {
                    orphan_entries.push_back(ordered_key);
                    ordered_group.erase(ordered_group.begin() + i);
                }
            }   
        }
    }

    for(auto orphan_entry_key : orphan_entries)
    {
        m_realtime_group_options_ordered.push_back({orphan_entry_key});
    }
}

void SDP::SetupRuntimeOptions()
{
    auto command_result = RunADBCommandForDevice(m_device_name, 
    std::string("shell ")
    .append(SDP_CLI_DEVICE_PATH)
    .append(" r l"));
    
    std::vector<std::string> option_lines;
    size_t pos          = 0;
    size_t previous_pos = 0;
    while ((pos = command_result.find('\n', pos)) != std::string::npos) 
    {
        option_lines.push_back(command_result.substr(previous_pos, pos - previous_pos));
        pos++;
        previous_pos = pos;
    }

    // Erase the header
    option_lines.erase(option_lines.begin(), option_lines.begin() + 5);

    std::unordered_map<std::string, int> name_to_index_mapping;

    for (size_t i = 0; i < option_lines.size(); ++i) 
    {
        std::string metric_index_str = option_lines[i].substr(0, option_lines[i].find('.'));

        std::string metric_name = option_lines[i].substr(option_lines[i].find('.') + 1);
        metric_name.erase(0, metric_name.find_first_not_of(" \t"));
        metric_name.erase(metric_name.find_last_not_of(" \t") + 1);

        const auto metric_index = std::stoi(metric_index_str);
        name_to_index_mapping.emplace(std::move(metric_name), metric_index);
    }

    auto& default_category      = m_realtime_options_by_category.emplace("Default", std::map<int, RealtimeOption>()).first->second;
    auto& stages_category       = m_realtime_options_by_category.emplace("Shader Stages", std::map<int, RealtimeOption>()).first->second;
    auto& stage_processing      = m_realtime_options_by_category.emplace("Shader Processing", std::map<int, RealtimeOption>()).first->second;
    auto& stalls_category       = m_realtime_options_by_category.emplace("Stalls", std::map<int, RealtimeOption>()).first->second;
    auto& primitives_category   = m_realtime_options_by_category.emplace("Primitive Processing", std::map<int, RealtimeOption>()).first->second;
    auto& instructions_category = m_realtime_options_by_category.emplace("Instructions", std::map<int, RealtimeOption>()).first->second;
    auto& texture_category      = m_realtime_options_by_category.emplace("Stage Textures", std::map<int, RealtimeOption>()).first->second;
    auto& memory_category       = m_realtime_options_by_category.emplace("Memory", std::map<int, RealtimeOption>()).first->second;
    auto& unknown_category       = m_realtime_options_by_category.emplace("Unknown", std::map<int, RealtimeOption>()).first->second;

    auto AddRealtimeOption = [&](
        auto&            category, 
        std::string_view name, 
        std::string_view legend, 
        bool             always_available, 
        bool             is_percentage, 
        double           scale, 
        bool             deleted_from_local_map = true)
    {
        const auto string_name = std::string(name); // I wish std::string_view hash operator was the same as std::string
        if(auto iter = name_to_index_mapping.find(string_name); iter != name_to_index_mapping.end())
        {
            auto entry_index = iter->second;
            auto result      = category.emplace(entry_index, RealtimeOption(name, legend, entry_index, always_available, is_percentage, scale));
            auto& entry      = result.first->second;
            m_realtime_options_by_name.emplace(name, std::ref(entry));
            m_realtime_options_by_index.emplace(entry_index, std::ref(entry));

            if (deleted_from_local_map)
            {
                name_to_index_mapping.erase(string_name);
            }
        }
    };

    /*
    * TODO: The scale sometimes is not properly applied:
    * Values are returned as 000.000, if the value is higher than 1k, the returned value will be
    * like 1.022k, which might cause a wrong value to be displayed since we are currently just
    * removing the "." from the output string
    * Maybe this is correct, IDK, but I should re-evaluate to make sure
    */

    // Setup realtime entries
    AddRealtimeOption(default_category, "Clocks / Second", "Clocks", false, false, 1.0);
    AddRealtimeOption(default_category, "GPU % Utilization", "Percentage", false, true, 0.001);
    AddRealtimeOption(default_category, "GPU % Bus Busy", "Percentage", false, true, 0.001);
    AddRealtimeOption(stalls_category, "% BVH Fetch Stall", "Percentage", true, true, 0.001);
    AddRealtimeOption(stalls_category, "% Vertex Fetch Stall", "Percentage", true, true, 0.001);
    AddRealtimeOption(stalls_category, "% Texture Fetch Stall", "Percentage", true, true, 0.001);
    AddRealtimeOption(stalls_category, "L1 Texture Cache Miss Per Pixel", "Count", false, false, 1.0);
    AddRealtimeOption(stalls_category, "% Texture L1 Miss", "Percentage", false, true, 0.001);
    AddRealtimeOption(stalls_category, "% Texture L2 Miss", "Percentage", false, true, 0.001);
    AddRealtimeOption(stalls_category, "% Stalled on System Memory", "Percentage", true, true, 0.001);
    AddRealtimeOption(stalls_category, "% Instruction Cache Miss", "Percentage", false, true, 0.001);
    AddRealtimeOption(primitives_category, "Pre-clipped Polygons/Second", "Count", false, false, 1.0);
    AddRealtimeOption(primitives_category, "% Prims Trivially Rejected", "Percentage", false, true, 0.001);
    AddRealtimeOption(primitives_category, "% Prims Clipped", "Percentage", false, true, 0.001);
    AddRealtimeOption(primitives_category, "Average Vertices / Polygon", "Count", false, false, 1.0);
    AddRealtimeOption(primitives_category, "Reused Vertices / Second", "Count", false, false, 1.0);
    AddRealtimeOption(primitives_category, "Average Polygon Area", "Average Area", false, false, 0.001);
    AddRealtimeOption(stages_category, "% RTU Busy", "Percentage", false, true, 0.001);
    AddRealtimeOption(instructions_category, "RTU Ray Box Intersections Per Instruction", "Intersections", false, false, 1.0);
    AddRealtimeOption(instructions_category, "RTU Ray Triangle Intersections Per Instruction", "Intersections", false, false, 1.0);
    AddRealtimeOption(stalls_category, "Average BVH Fetch Latency Cycles", "Cycles", false, false, 1.0);
    AddRealtimeOption(stalls_category, "% Shaders Busy", "Percentage", true, true, 0.001);
    AddRealtimeOption(stalls_category, "% Shaders Stalled", "Percentage", true, true, 0.001);
    AddRealtimeOption(stage_processing, "Vertices Shaded / Second", "Count", false, false, 1.0);
    AddRealtimeOption(stage_processing, "Fragments Shaded / Second", "Count", false, false, 1.0);
    AddRealtimeOption(instructions_category, "Vertex Instructions / Second", "Instructions", false, false, 1.0);
    AddRealtimeOption(instructions_category, "Fragment Instructions / Second", "Instructions", false, false, 1.0);
    AddRealtimeOption(instructions_category, "Fragment ALU Instructions / Sec (Full)", "Instructions", false, false, 1.0);
    AddRealtimeOption(instructions_category, "Fragment ALU Instructions / Sec (Half)", "Instructions", false, false, 1.0);
    AddRealtimeOption(instructions_category, "Fragment EFU Instructions / Second", "Instructions", false, false, 1.0);
    AddRealtimeOption(texture_category, "Textures / Vertex", "Count", false, false, 0.001);
    AddRealtimeOption(texture_category, "Textures / Fragment", "Count", false, false, 0.001);
    AddRealtimeOption(stage_processing, "ALU / Vertex", "Count", false, false, 1.0);
    AddRealtimeOption(stage_processing, "ALU / Fragment", "Count", false, false, 1.0);
    AddRealtimeOption(stage_processing, "EFU / Fragment", "Count", false, false, 1.0);
    AddRealtimeOption(stage_processing, "EFU / Vertex", "Count", false, false, 1.0);
    AddRealtimeOption(stages_category, "% Time Shading Fragments", "Percentage", true, true, 0.001);
    AddRealtimeOption(stages_category, "% Time Shading Vertices", "Percentage", true, true, 0.001);
    AddRealtimeOption(stages_category, "% Time Compute", "Percentage", true, true, 0.001);
    AddRealtimeOption(stage_processing, "% Shader ALU Capacity Utilized", "Percentage", false, true, 0.001);
    AddRealtimeOption(stages_category, "% Time ALUs Working", "Percentage", false, true, 0.001);
    AddRealtimeOption(stages_category, "% Time EFUs Working", "Percentage", false, true, 0.001);
    AddRealtimeOption(stage_processing, "% Nearest Filtered", "Percentage", false, true, 0.001);
    AddRealtimeOption(stage_processing, "% Linear Filtered", "Percentage", false, true, 0.001);
    AddRealtimeOption(stage_processing, "% Anisotropic Filtered", "Percentage", false, true, 0.001);
    AddRealtimeOption(texture_category, "% Non-Base Level Textures", "Percentage", false, true, 0.001);
    AddRealtimeOption(stages_category, "% Texture Pipes Busy", "Percentage", false, true, 0.001);
    AddRealtimeOption(stage_processing, "% Wave Context Occupancy", "Percentage", false, true, 0.001);
    AddRealtimeOption(memory_category, "Read Total (Bytes/sec)", "Bytes", false, false, 1.0);
    AddRealtimeOption(memory_category, "Write Total (Bytes/sec)", "Bytes", false, false, 1.0);
    AddRealtimeOption(memory_category, "Texture Memory Read BW (Bytes/Second)", "Bytes", false, false, 1.0);
    AddRealtimeOption(memory_category, "Vertex Memory Read (Bytes/Second)", "Bytes", false, false, 1.0);
    AddRealtimeOption(memory_category, "SP Memory Read (Bytes/Second)", "Bytes", false, false, 1.0);
    AddRealtimeOption(memory_category, "Avg Bytes / Fragment", "Bytes", false, false, 1.0);
    AddRealtimeOption(memory_category, "Avg Bytes / Vertex", "Bytes", false, false, 1.0);
    AddRealtimeOption(default_category, "Preemptions / second", "Count", false, false, 1.0);
    AddRealtimeOption(default_category, "Avg Preemption Delay", "Delay", false, false, 1.0);
    AddRealtimeOption(default_category, "GPU Frequency", "Frequency", false, false, 1.0);

    // For each uncategorized entry, add it to the unknow category
    for (auto& [entry_name, _] : name_to_index_mapping)
    {
        AddRealtimeOption(unknown_category, entry_name, "Count", false, false, 1.0, false);
    }

    // Prepare the ordered vector, which contains an index for each of the options above
    // and can be modified by the user to change how plots are visualized
    for (auto& [category, map] : m_realtime_options_by_category)
    {
        for (auto& [key, entry] : map)
        {
            m_realtime_group_options_ordered.push_back({key});
        }
    }
}

float SDP::GetActiveTimeBySampleCountAndFrequency() const
{
    int sample_count = 0;
    for(auto& [_, map] : m_realtime_options_by_category)
    {
        for(auto& [_, option] : map)
        {
            if(option.selected || option.always_available)
            {
                sample_count = option.value.GetStoredValueCount();
                break;
            }
        }

        if (sample_count > 0)
        {
            break;
        }
    }

    return static_cast<float>(sample_count) / (1000.0f / static_cast <float> (m_sampling_period_ms));
}

void SDP::RefreshAsyncUpdateThread(int previous_sampling_period_ms)
{
    /*
    * NOTE: Don't call reset here, we don't wanna clear all graph contents but only adjust the disabled
    * ones and "correct" the just enabled ones
    */

    std::lock_guard l(m_safety);

    /*
    * Size Matching
    * 
    * In order to not need to reset existing values whenever the cmd line changes, go over each active option
    * and fill out missing entries until the time/entries match
    * 
    * Also ensure to match the sampling frequency first in case values needs to be expanded or contracted
    */
    {
        const size_t sample_count_total = m_active_time / (static_cast<double>(m_sampling_period_ms) / 1000.0);

        for (auto& [category, map] : m_realtime_options_by_category)
        {
            for (auto& [key, entry] : map)
            {
                if(entry.selected || entry.always_available)
                {
                    if (previous_sampling_period_ms != 0)
                        entry.value.MatchSamplingFrequency(previous_sampling_period_ms, m_sampling_period_ms);

                    entry.value.MatchSize(sample_count_total);
                }
            }
        }    
    }

    FixDisabledGroupEntries();

    /*
    * Build command line:
    * 
    * Prepare runtime continuous sample request, with m_sampling_period_ms as the period
    * Go over each active runtime option and include its index
    */

    std::string console_command = 
        std::string("adb -s ")
        .append(m_device_name)
        .append(" shell ")
        .append(SDP_CLI_DEVICE_PATH)
        .append(" r c -p ")
        .append(std::to_string(m_sampling_period_ms))
        .append(" ");

    bool is_first_option = true;
    for (const auto& [_, entry_ref] : m_realtime_options_by_index)
    {
        const auto& entry = entry_ref.get();
        if(entry.always_available || entry.selected)
        {
            if (!is_first_option)
            {
                console_command.append(",");
            }

            console_command.append(std::to_string(entry.index));

            is_first_option = false;
        }
    }

    // New command line = new header incomming, flag that we need to filter all initial
    // arguments again
    m_console_ignore_header = true;

    m_console_helper.TerminateAsynchronousCommand();

    m_console_helper.ExecuteAsynchronousCommand(
        console_command.data(),
        [&](std::string_view cmd_output) 
        { 
            ParseConsoleOutput(cmd_output);
        },
        true /*not really needed but we know we don't need the first line, this option will eventually be reworked*/);
}

void SDP::ParseConsoleOutput(std::string_view console_output)
{
    /*
    * Quick and dirty parse for CLI output
    * Can be optimized for sure
    * 
    * The #ifdef section below correspond to the type of output we need to handle here
    */

#if 0
    sdpcli

    Running in Realtime Metrics mode...

    Requested metrics:
    Metric ID 2 is invalid and will be ignored.
    Metric ID 4: GPU % Bus Busy
    Metric ID 5 is invalid and will be ignored.
    Metric ID 7: % Vertex Fetch Stall
    Metric ID 9: L1 Texture Cache Miss Per Pixel
    Metric ID 12: % Stalled on System Memory
    Metric ID 14 is invalid and will be ignored.

    Itr | 4       | 7       | 9         | 12      | 15        | 17      |
    1   |   0.024 |   0.009 |     0.000 |   0.000 |    27.534 |  57.143 |
    2   |   0.097 |   0.009 |     0.000 |   0.000 |    27.468 |  57.143 |
    3   |   0.132 |   0.009 |     0.000 |   0.000 |    27.565 |  57.143 |
    4   |   0.111 |   0.009 |     0.000 |   0.000 |    27.555 |  57.143 |
#endif

    // Search for the last line of the header
    // When found, we cache the info to avoid future str comparissions
    if (m_console_ignore_header && console_output.find("Itr") == std::string::npos)
    {
        m_console_ignore_header = false;
        return;
    }

    // Find the position of the first occurrence of '|'
    size_t start_pos = console_output.find('|');
    if (start_pos == std::string::npos) 
    {
        return;
    }

    // Extract the substring after the first '|'
    std::string values_str = std::string(console_output.substr(start_pos + 1));
    std::replace(values_str.begin(), values_str.end(), '|', ' ');
    std::erase(values_str, '.');

    // Create a stringstream to tokenize the values
    std::istringstream iss(values_str);
    std::string token;

    for (auto& [_, entry_ref] : m_realtime_options_by_index)
    {
        auto& entry = entry_ref.get();
        if(!entry.selected && !entry.always_available)
        {
            continue;
        }

        bool did_find_value = false; // Just a guard in case cmdline changes between versions, ensuring
                                        // we don't end up with undefined behavior
        while (std::getline(iss, token, ' ')) 
        {
            if (token.empty() || token == "\n") 
            {
                continue;
            }
            
            entry.value.AddEntry(static_cast<uint64_t>(static_cast<double>(ParseNumber(token)) * entry.scale));
            did_find_value = true;
            break;
        }

        assert(did_find_value);
    }
}

void SDP::DrawSetupSection(double time_elapsed)
{
    ImGui::Text("Select SDP CLI File:");
    ImGui::InputTextWithHint("##sdp_cli", "SDP CLI File", m_sdp_cli_path.data(), m_sdp_cli_path.size(), ImGuiInputTextFlags_ReadOnly);

    ImGui::SameLine();

    if (ImGui::Button("Select File##cli")) 
    {
        auto selected_file = RequestFileSelection("SDP CLI");

        if (selected_file.size() < m_sdp_cli_path.size())
        {
            std::copy(selected_file.begin(), selected_file.end(), m_sdp_cli_path.begin());
            m_sdp_cli_path[selected_file.size()] = '\0';
        }
    }

    ImGui::Text("Select Profilling Layer File APK:");
    ImGui::InputTextWithHint("##sdp_pl", "Profilling Layer File APK", m_profilling_layer_path.data(), m_profilling_layer_path.size(), ImGuiInputTextFlags_ReadOnly);

    ImGui::SameLine();

    if (ImGui::Button("Select File##pl")) 
    {
        auto selected_file = RequestFileSelection("Profilling Layer APK");

        if (selected_file.size() < m_profilling_layer_path.size())
        {
            std::copy(selected_file.begin(), selected_file.end(), m_profilling_layer_path.begin());
            m_profilling_layer_path[selected_file.size()] = '\0';
        }
    }

    ImGui::BeginDisabled(!std::filesystem::exists(m_sdp_cli_path.data()) || !std::filesystem::exists(m_profilling_layer_path.data()));

    /*
    * These commands below are what you would normally need to run in case you were to manually run SDP CLI
    * We push the SDP CLI file into the device, give it permission and install the trace layer APK (not 
    * currently/yet used by this application)
    */
    if (ImGui::Button("Inject")) 
    {
        const auto adjusted_sdp_cli_path = std::string("\"").append(m_sdp_cli_path.data()).append("\"");
        const auto adjusted_pl_path      = std::string("\"").append(m_profilling_layer_path.data()).append("\"");

        // Push files
        RunADBCommandForDevice(m_device_name, std::string("push ").append(adjusted_sdp_cli_path).append(" ").append(SDP_CLI_DEVICE_PATH));
        RunADBCommandForDevice(m_device_name, std::string("install ").append(adjusted_pl_path));

        // Fix access
        RunADBCommandForDevice(m_device_name, std::string("shell chmod a+x ").append(SDP_CLI_DEVICE_PATH));
        m_has_sdp_cli = DoesDeviceFileExist(m_device_name, SDP_CLI_DEVICE_PATH);
        if (m_has_sdp_cli)
        {
            RefreshAsyncUpdateThread();
        }
    }

    ImGui::EndDisabled();
}

void SDP::DrawGeneralSection(double time_elapsed)
{
    if(ImGui::CollapsingHeader("Shader Stages", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::pmr::vector<const char*> labels(&m_memory_resource);
        std::pmr::vector<int>         data(&m_memory_resource);

        if (GetRuntimeShaderStageValues(NumericalAggregatorValueType::NUMERICAL_AGGREGATOR_CURRENT, labels, data))
        {
            ImPlotPieChartFlags flags = 0;

            // CHECKBOX_FLAG(flags, ImPlotPieChartFlags_Normalize);
            // CHECKBOX_FLAG(flags, ImPlotPieChartFlags_IgnoreHidden);

            if (ImPlot::BeginPlot("GPU Usage per Stage", ImVec2(320,320), ImPlotFlags_Equal | ImPlotFlags_NoMouseText)) 
            {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, 1, 0, 1);
                ImPlot::PlotPieChart(labels.data(), data.data(), labels.size(), 0.5, 0.5, 0.4, "%.2f", 90, flags);
                ImPlot::EndPlot();
            }
        }
    }
    
    if(ImGui::CollapsingHeader("Shader Stalls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::pmr::vector<const char*> busy_vs_stall_labels(&m_memory_resource);
        std::pmr::vector<int>         busy_vs_stall_data(&m_memory_resource);
        std::pmr::vector<const char*> stalls_labels(&m_memory_resource);
        std::pmr::vector<int>         stalls_data(&m_memory_resource);

        if (GetRuntimeShaderStallValues(
            NumericalAggregatorValueType::NUMERICAL_AGGREGATOR_CURRENT, 
            busy_vs_stall_labels, 
            busy_vs_stall_data,
            stalls_labels,
            stalls_data))
        {
            ImPlotPieChartFlags flags = 0;

            // CHECKBOX_FLAG(flags, ImPlotPieChartFlags_Normalize);
            // CHECKBOX_FLAG(flags, ImPlotPieChartFlags_IgnoreHidden);

            if (ImPlot::BeginPlot("Busy vs Stall", ImVec2(320,320), ImPlotFlags_Equal | ImPlotFlags_NoMouseText)) 
            {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, 1, 0, 1);
                ImPlot::PlotPieChart(busy_vs_stall_labels.data(), busy_vs_stall_data.data(), busy_vs_stall_labels.size(), 0.5, 0.5, 0.4, "%.2f", 90, flags);
                ImPlot::EndPlot();
            }

            ImGui::SameLine();


            if (ImPlot::BeginPlot("Stalls Category", ImVec2(320,320), ImPlotFlags_Equal | ImPlotFlags_NoMouseText)) 
            {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, 1, 0, 1);
                ImPlot::PlotPieChart(stalls_labels.data(), stalls_data.data(), stalls_labels.size(), 0.5, 0.5, 0.4, "%.2f", 90, flags);
                ImPlot::EndPlot();
            }
        }
    }
}

void SDP::DrawPlaygroundSection(double time_elapsed)
{
    if (ImGui::Button(m_playground_selector_visible ? "<" : ">"))
    {
        m_playground_selector_visible = !m_playground_selector_visible;
    }

    const float options_width             = 360;
    const float separator_width           = 4;
    const auto  initial_cursor_screen_pos = ImGui::GetCursorScreenPos();

    if (m_playground_selector_visible) 
    {
        if (ImGui::BeginChild("##runtime_options", ImVec2(options_width, 0)))
        {
            DrawPlaygroundOptions(time_elapsed);
        }
        ImGui::EndChild();
    
        ImGui::SetCursorScreenPos(ImVec2(initial_cursor_screen_pos.x + options_width + 1 * separator_width, initial_cursor_screen_pos.y));
        ImGui::Button("##runtime_section_button", ImVec2(separator_width, ImGui::GetContentRegionAvail().y));
        ImGui::SetCursorScreenPos(ImVec2(initial_cursor_screen_pos.x + options_width + 3 * separator_width, initial_cursor_screen_pos.y));
    }

    if (ImGui::BeginChild("##runtime_plots", ImVec2(0, 0)))
    {
        DrawPlaygroundPlots(time_elapsed);
    }
    ImGui::EndChild();
}

void SDP::DrawPlaygroundOptions(double time_elapsed)
{
    bool did_change_any_options = false;

    for (auto& [category, map] : m_realtime_options_by_category)
    {
        if (map.empty())
        {
            continue;
        }

        if (ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto& [key, entry] : map)
            {
                bool is_selected = entry.selected;
                ImGui::PushID(entry.index);
                ImGui::Checkbox("##selected", &is_selected);
                ImGui::PopID();

                did_change_any_options |= is_selected != entry.selected;

                entry.selected = is_selected;

                ImGui::SameLine();

                if (ImGui::TreeNodeEx(entry.name.c_str(), ImGuiTreeNodeFlags_Leaf))
                {
                    // Debug raw value tooltip
                    if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                    {
                        ImGui::SetTooltip("Raw value: %lu", entry.value.GetValue());
                    }

                    ImGui::TreePop();
                }
            }
        }
    }

    if (did_change_any_options)
    {
        RefreshAsyncUpdateThread();
    }
}

void SDP::DrawPlaygroundPlots(double time_elapsed)
{
    const auto local_active_time = GetActiveTimeBySampleCountAndFrequency();

    if (m_playground_auto_scroll)
    {
        m_playground_history_range_end = local_active_time;
    }

	ImGui::RangeSliderFloat(
        "History", 
        &m_playground_history_range_begin,
        &m_playground_history_range_end,
        0.0f,
        local_active_time);

    ImGui::SameLine();

    ImGui::Checkbox("Auto Scroll", &m_playground_auto_scroll);

    struct SampleGetterData
    {
        int             sample_count        = 0;
        double          sample_index_offset = 0.0;
        double          sample_ratio        = 1.0;
        RealtimeOption* entry               = nullptr;
    };

    auto base_getter = [](int idx, void* data)
    {
        const SampleGetterData& getter_data   = *static_cast<SampleGetterData*>(data);
        return ImPlotPoint((idx / getter_data.sample_ratio), 0);
    };

    auto target_getter = [](int idx, void* data)
    {
        const SampleGetterData& getter_data   = *static_cast<SampleGetterData*>(data);
        const auto              index_offset  = static_cast<int>(static_cast<double>(idx) * getter_data.sample_index_offset);
        auto                    stored_values = getter_data.entry->value.GetStoredValues();
                
        assert(index_offset < stored_values.size());

        return ImPlotPoint((idx / getter_data.sample_ratio), stored_values[index_offset]);
    };

    auto CountAxisFormatter = [](double value, char* buff, int size, void* user_data)
    {
        const char* suffix = "";
        if (value >= 1000000.0) 
        {
            value /= 1000000.0;
            suffix = "M";
        } 
        else if (value >= 1000.0) 
        {
            value /= 1000.0;
            suffix = "k";
        }

        snprintf(buff, size, "%llu%s", static_cast<uint64_t>(value), suffix);

        return static_cast<int>(value);
    };
    
    auto PercentageAxisFormatter = [](double value, char* buff, int size, void* user_data)
    {
        snprintf(buff, size, "%llu%%", static_cast<uint64_t>(value));
        return static_cast<int>(value);
    };

    auto DrawRuntimeOptionPlot = [&](
        std::span<std::reference_wrapper<RealtimeOption>> entries, 
        PlotSettings                                      plot_settings, 
        bool                                              histogram_active) -> std::optional<int>
    {
        std::optional<int> ungroup_value;

        assert(!entries.empty());

        // Use the first entry for all the base references needed below
        const auto& reference_entry = entries[0].get();

        // Grab the sample count, which is the same for all entries since we match missing values whenever
        // an option is added or removed
        const auto sample_count                      = reference_entry.value.GetStoredValueCount();
        const auto sample_count_by_active_time_ratio = static_cast<double>(sample_count) / std::max(1.0f, local_active_time);

        ImGui::PushID(reference_entry.name.c_str());
        if (ImPlot::BeginPlot("##runtime_plot_entry", ImVec2(-1,150))) 
        {
            const double horizontal_limit = (std::max(3.0f, sample_count - 3.0f));

            // Find out the Y axis limits and if they are all percentage
            double plot_y_min         = 0.0f;
            double plot_y_max         = 1.0f;
            bool   are_all_percentage = entries[0].get().is_percentage;
            for (auto& entry : entries)
            {
                plot_y_min         = std::min(plot_y_min, static_cast<double>(entry.get().value.GetMin()));
                plot_y_max         = std::max(plot_y_max, static_cast<double>(entry.get().value.GetMax()));
                are_all_percentage &= entry.get().is_percentage;
            }

            // If all values on this group are percentage values (0% - 100%), ensure the limits are properly set
            plot_y_min               = are_all_percentage ? 0 : plot_y_min;
            plot_y_max               = are_all_percentage ? 100 : plot_y_max;
            const auto limit_padding = std::abs(plot_y_max - plot_y_min) * 0.1f;

            if (!histogram_active)
            {
                ImPlot::SetupAxes("Time", nullptr, 0, are_all_percentage ? 0 : ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(
                    ImAxis_X1, 
                    m_playground_history_range_begin, 
                    m_playground_history_range_end, 
                    ImGuiCond_Always);

                /*
                * Setup Y axis legend and name
                */
                if (!are_all_percentage)
                {
                    if (entries.size() == 1)
                    {
                        ImPlot::SetupAxis(ImAxis_Y1, reference_entry.legend.c_str());
                    }
                    
                    ImPlot::SetupAxisFormat(ImAxis_Y1, CountAxisFormatter, nullptr);
                }
                else
                {
                    ImPlot::SetupAxisFormat(ImAxis_Y1, PercentageAxisFormatter, nullptr);
                }

                ImPlot::SetupAxisLimits(ImAxis_Y1, plot_y_min - limit_padding, plot_y_max + limit_padding, ImGuiCond_Always);

                for (int i = 0; i < entries.size(); i++)
                {
                    auto& entry        = entries[i];
                    auto  entry_values = entry.get().value.GetStoredValues();

                    SampleGetterData getter_data;
                    getter_data.sample_count        = std::min(1000, static_cast<int>(entry_values.size())); // Max of 1000 samples
                    getter_data.sample_index_offset = static_cast<double>(entry_values.size()) / std::max(1, getter_data.sample_count);
                    getter_data.sample_ratio        = sample_count_by_active_time_ratio;
                    getter_data.entry               = &entry.get();

                    switch (entry.get().graph_type)
                    {
                    case GraphType::LINE:
                        ImPlot::PlotLineG(entry.get().name.c_str(), target_getter, &getter_data, getter_data.sample_count);
                        break;
                
                    case GraphType::SHADE:
                        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL,0.5f);
                        ImPlot::PlotShadedG(entry.get().name.c_str(), base_getter, &getter_data, target_getter, &getter_data, getter_data.sample_count);
                        break;
                    }

                    if (ImPlot::IsPlotHovered())
                    {
                        const auto plot_mouse_values = ImPlot::GetPlotMousePos();

                        /*
                        * Do horizontal graph scrolling if the mouse wheel changed
                        */
                        const float range_difference             = m_playground_history_range_end - m_playground_history_range_begin;
                        float mouse_horizontal_range_factor      = static_cast<float>(plot_mouse_values.x) - m_playground_history_range_begin;
                        mouse_horizontal_range_factor            = mouse_horizontal_range_factor / std::max(0.001f, range_difference);
                        const float range_begin_factor           = mouse_horizontal_range_factor;
                        const float range_end_factor             = 1.0f - mouse_horizontal_range_factor;
                        const float mouse_horizontal_range_scale = 0.05f;
                        const float range_begin_value            = range_begin_factor * (range_difference * mouse_horizontal_range_scale);
                        const float range_end_value              = range_end_factor * (range_difference * mouse_horizontal_range_scale);

                        m_playground_history_range_begin -= ImGui::GetIO().MouseWheel * range_begin_value;
                        m_playground_history_range_end   += m_playground_auto_scroll ? 0.0f : ImGui::GetIO().MouseWheel * range_end_value;

                        m_playground_history_range_begin = std::max(m_playground_history_range_begin, 0.0f);
                        m_playground_history_range_end   = std::min(m_playground_history_range_end, GetActiveTimeBySampleCountAndFrequency());
                    }

                    if (ImPlot::IsLegendEntryHovered(entry.get().name.c_str()) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                    {
                        // Right click with CTRL    = swap between graph types
                        // Right click without CTRL = ungroup
                        if (ImGui::IsKeyDown(ImGuiKey_RightCtrl) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                            entry.get().graph_type = entry.get().graph_type == GraphType::LINE ? GraphType::SHADE : GraphType::LINE;
                        else
                            ungroup_value = i;
                    }
                }

                if (ImPlot::IsPlotHovered())
                {
                    /*
                    * Do tooltip draw at mouse screen pos
                    */

                    // double (*PlotTooltipGetter)(const PlotTooltipEntry& entry, int idx, void* user_data)

                    // The problem is the binary search -> If I get the positional value from the mouse, I should be able
                    // to infer into the IDX directly without doing the bin search

                    auto PlotTooltipGetter = [](const std::reference_wrapper<RealtimeOption>& entry, int idx, void* user_data) -> double
                    {
                        auto entry_values = entry.get().value.GetStoredValues();
                        assert(idx < entry_values.size());
                        return static_cast<double>(entry_values[idx]);
                    };

                    ImPlot::PlotTooltip(
                        "Test", 
                        entries,
                        PlotTooltipGetter,
                        sample_count,
                        0.1);

                    // TODO
                }
            }
            else
            {
                assert(!entries.empty());
                auto& entry        = entries.back().get();
                auto  entry_values = entry.value.GetStoredValues();

                static const ImPlotHistogramFlags hist_flags = ImPlotHistogramFlags_Density;

                ImPlot::SetupAxes(nullptr,nullptr,ImPlotAxisFlags_AutoFit,ImPlotAxisFlags_AutoFit);
                ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL,0.5f);
                ImPlot::PlotHistogram(
                    entry.name.c_str(), 
                    entry_values.data(), 
                    entry_values.size(), 
                    ImPlotBin_Sturges, 
                    1.0, 
                    ImPlotRange(entry.value.GetMin(), entry.value.GetMax()), 
                    hist_flags);
            }

            ImPlot::EndPlot();
        }

        ImGui::PopID();

        return ungroup_value;
    };

    // Handle move requests from drag and drop
    // int (1) - From
    // int (2) - To
    // bool    - If true, means from should be merged with to, if false, from should be moved to to location
    std::optional<std::tuple<int, int, bool>> move_request;

    for (int i = 0; i < m_realtime_group_options_ordered.size(); i++)
    {
        auto& ordered_group = m_realtime_group_options_ordered[i];

        std::pmr::vector<std::reference_wrapper<RealtimeOption>> entry_options_vector(&m_memory_resource);
        std::pmr::vector<int>                                    entry_index_vector(&m_memory_resource);

        for (int j = 0; j < ordered_group.size(); j++)
        {
            auto ordered_key = ordered_group[j];
            for (auto& [category, map] : m_realtime_options_by_category)
            {
                auto iter = map.find(ordered_key);
                if(iter == map.end())
                {
                    continue;
                }

                if (iter->second.selected)
                {
                    entry_options_vector.push_back(iter->second);
                    entry_index_vector.push_back(j);
                }  
            }
        }

        if (entry_options_vector.empty())
        {
            continue;
        }

        // Setup the section string
        std::pmr::string section_string(&m_memory_resource);

        for (auto& entry : entry_options_vector)
        {
            section_string.append(section_string.empty() ? "" : " | ");
            section_string.append(entry.get().name);
        }

        // Returns true if the drop area is visible
        auto SetupDragDropArea = [&move_request](int current_index, bool is_group) -> bool
        {
            if (ImGui::BeginDragDropSource()) 
            {
                ImGui::SetDragDropPayload(DRAG_DROP_SECTION_NAME, &current_index, sizeof(int));
                ImGui::EndDragDropSource();
            }

            // Only render the drop target area if indexes are not the same
            if(const ImGuiPayload* payload = ImGui::GetDragDropPayload(); payload && payload->IsDataType(DRAG_DROP_SECTION_NAME))
            {
                if(auto from_index = *(const int*)payload->Data; from_index != current_index)
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if(ImGui::AcceptDragDropPayload(DRAG_DROP_SECTION_NAME, 0))
                        {
                            move_request = std::make_tuple(from_index, current_index, is_group);
                        }

                        ImGui::EndDragDropTarget();

                        return true;
                    }
                }
            }  

            return false;
        };

        if (ImGui::CollapsingHeader(section_string.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            assert(!entry_options_vector.empty());
            const auto*   reference_entry           = &entry_options_vector.back().get();
            const auto    local_id                  = ImGui::GetID(reference_entry);
            ImGuiStorage* storage                   = ImGui::GetStateStorage();
            PlotSettings  plot_settings             = static_cast<PlotSettings>(storage->GetInt(local_id));
            const bool    can_use_histogram         = entry_options_vector.size() == 1;

            // Check if histogram mode is active, also adjust it in case it shouldn't
            bool histogram_active = can_use_histogram ? (plot_settings & PLOT_HISTOGRAM) : false;

            ImGui::BeginDisabled(!can_use_histogram);
            ImGui::PushStyleColor(ImGuiCol_Button, histogram_active ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
            ImGui::PushID(local_id);
            if (ImGui::Button("histogram"))
            {
                histogram_active = !histogram_active;
            }
            ImGui::PopID();
            ImGui::PopStyleColor();
            ImGui::EndDisabled();

            // Commit all changes back to the plot settings variable for saving
            plot_settings = static_cast<PlotSettings>(plot_settings & ~PLOT_HISTOGRAM);
            plot_settings = static_cast<PlotSettings>(plot_settings & ~PLOT_DEFAULT);
            plot_settings = static_cast<PlotSettings>(histogram_active ? plot_settings | PLOT_HISTOGRAM : plot_settings | PLOT_DEFAULT);

            // Save back the group plot settings
            storage->SetInt(local_id, static_cast<int>(plot_settings));

            // Draw plot for the entire group
            // Only proceed with ungroup requests if there are multiple entries on this group
            if (auto ungroup_index = DrawRuntimeOptionPlot(entry_options_vector, plot_settings, histogram_active); ungroup_index && ordered_group.size() > 1)
            {
                // Since we are processing in order and ordered_group is done processing, we can freely extract
                // its entry and add right after the active one at m_realtime_group_options_ordered
                auto extracted_entry = std::move(ordered_group[entry_index_vector[ungroup_index.value()]]);
                ordered_group.erase(ordered_group.begin() + entry_index_vector[ungroup_index.value()]);
                m_realtime_group_options_ordered.insert(m_realtime_group_options_ordered.begin() + i + 1, {std::move(extracted_entry)});
            }

            SetupDragDropArea(i, true);
        }

        /*
        * Draw an invisible button as a drop target, so plots can be re-ordered around
        * While the drag/drop setup returns false, we return the cursor back to the area it was before the
        * invisible button to not show any spaces between plots, if the mouse is hovering that area we
        * keep the cursor where its
        */
        const auto drag_area_begin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##drag_area_after_group", ImVec2(ImGui::GetContentRegionAvail().x, 16));
        if (!SetupDragDropArea(i, false))
        {
            ImGui::SetCursorScreenPos(drag_area_begin);
        }
    }

    if (move_request)
    {
        auto& [from_index, to_index, merge] = move_request.value();

        if (merge)
        {
            m_realtime_group_options_ordered[to_index].insert( 
                m_realtime_group_options_ordered[to_index].end(), 
                m_realtime_group_options_ordered[from_index].begin(), 
                m_realtime_group_options_ordered[from_index].end() );
            m_realtime_group_options_ordered.erase(m_realtime_group_options_ordered.begin() + from_index);
        }
        else
        {
            // Might cause memory reallocation, we don't care, rarelly happens :)
            auto entry = std::move(m_realtime_group_options_ordered[from_index]);
            m_realtime_group_options_ordered.erase(m_realtime_group_options_ordered.begin() + from_index);
            m_realtime_group_options_ordered.insert(m_realtime_group_options_ordered.begin() + to_index + 1, std::move(entry));
        }
    }
}

bool SDP::GetRuntimeShaderStallValues(
    NumericalAggregatorValueType   aggregator_visualizer_type, 
    std::pmr::vector<const char*>& busy_vs_stall_labels, 
    std::pmr::vector<int>&         busy_vs_stall_data,
    std::pmr::vector<const char*>& stalls_labels, 
    std::pmr::vector<int>&         stalls_data)
{
    auto TryFindEntry = [&](auto& entry) -> std::optional<int>
    {
        if (auto iter = m_realtime_options_by_name.find(entry); iter != m_realtime_options_by_name.end())
        {
            const auto& entry = iter->second.get();
            return static_cast<int>(entry.value.GetValue(aggregator_visualizer_type));
        }

        return std::nullopt;
    };

    auto EmplaceEntry = [&](auto& entry, auto& target_label_vector, auto& target_data_vector)
    {
        if (auto iter = m_realtime_options_by_name.find(entry); iter != m_realtime_options_by_name.end())
        {
            const auto& entry = iter->second.get();
            target_label_vector.push_back(entry.name.c_str());
            target_data_vector.push_back(static_cast<int>(entry.value.GetValue(aggregator_visualizer_type)));
        }
    };

    /*
    * Values are hardcoded and unlikely to change
    * If SDP CLI values do start to change frequently between different versions, better to fetch require entries
    * in another way
    */

    auto shaders_busy_value    = TryFindEntry("% Shaders Busy");
    auto shaders_stalled_value = TryFindEntry("% Shaders Stalled");

    EmplaceEntry("% Vertex Fetch Stall", stalls_labels, stalls_data);
    EmplaceEntry("% Texture Fetch Stall", stalls_labels, stalls_data);
    EmplaceEntry("% Stalled on System Memory", stalls_labels, stalls_data);

    if (!shaders_busy_value || !shaders_stalled_value)
    {
        return false;
    }

    // We know stalls are a percentage of busy, so lets normalize it:
    shaders_busy_value = std::max(0, shaders_busy_value.value() - shaders_stalled_value.value());

    busy_vs_stall_labels.push_back("% Shaders Busy");
    busy_vs_stall_labels.push_back("% Shaders Stalled");
    busy_vs_stall_labels.push_back("% Shaders Idle");
    busy_vs_stall_data.push_back(shaders_busy_value.value());
    busy_vs_stall_data.push_back(shaders_stalled_value.value());
    busy_vs_stall_data.push_back(100 - std::min(100, (shaders_busy_value.value() + shaders_stalled_value.value())));

    /*
    * Since we want the ratio between all stall options, accumulate the value and
    * then normalize between all options
    * 
    * If no stall is detected, add the "unknow" category
    */

    float stall_total_measured = 0;
    for (auto& value : stalls_data)
    {
        stall_total_measured += std::max(0.0f, static_cast<float>(value));
    }

    const float stall_ratio = stall_total_measured / 100.0f;
    for (auto& value : stalls_data)
    {
        value = static_cast<int>(value / std::max(0.0001f, stall_ratio));
    }

    stalls_labels.push_back("Unknown"); // Static string ref -> safe
    stalls_data.push_back(std::max(0, stall_total_measured == 0.0f ? 100 : 0));

    return busy_vs_stall_data.size() >= 2 && !stalls_data.empty();
}
bool SDP::GetRuntimeShaderStageValues(
    NumericalAggregatorValueType   aggregator_visualizer_type, 
    std::pmr::vector<const char*>& labels, 
    std::pmr::vector<int>&         data)
{
    auto EmplaceEntry = [&](auto& entry)
    {
        if (auto iter = m_realtime_options_by_name.find(entry); iter != m_realtime_options_by_name.end())
        {
            const auto& entry = iter->second.get();
            labels.push_back(entry.name.c_str());
            data.push_back(static_cast<int>(entry.value.GetValue(aggregator_visualizer_type)));
        }
    };

    /*
    * Values are hardcoded and unlikely to change
    * If SDP CLI values do start to change frequently between different versions, better to fetch require entries
    * in another way
    */
    EmplaceEntry("% Time Shading Fragments");
    EmplaceEntry("% Time Shading Vertices");
    EmplaceEntry("% Time Compute");

    int total_measured = 0;
    for (auto& value : data)
    {
        total_measured += value;
    }

    // If GPU isn't fully busy, add "other" option
    if (total_measured < 100)
    {
        labels.push_back("Other"); // Static string ref -> safe
        data.push_back(std::max(0, 100 - total_measured));
    }

    return data.size() >= 3;
}