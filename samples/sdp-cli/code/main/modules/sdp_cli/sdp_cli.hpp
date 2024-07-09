//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <chrono>
#include <array>
#include <map>
#include <unordered_map>
#include <memory_resource>
#include <functional>
#include "../../helpers/module_interface.hpp"
#include "../../helpers/console_helper.hpp"
#include "../../helpers/numerical_aggregator.hpp"

class SDP : public ModuleInterface
{
    enum class GraphType
    {
        LINE,
        SHADE,
    };
    
    enum PlotSettings
    {
        PLOT_DEFAULT     = 1 << 0, // Individual graphs at the same plot, conflicts with HISTOGRAM
        PLOT_HISTOGRAM   = 1 << 1, // Only if there is a single graph on the plot, conflicts with DEFAULT
    };

    struct RealtimeOption
    {
        RealtimeOption(
            std::string_view in_name, 
            std::string_view in_legend, 
            int              in_index, 
            bool             in_always_available, 
            bool             in_is_percentage, 
            double           in_scale)
            : name(in_name)
            , legend(in_legend)
            , index(in_index)
            , always_available(in_always_available)
            , is_percentage(in_is_percentage)
            , scale(in_scale)
        {
        }
        ~RealtimeOption() = default;

        std::string                   name;
        std::string                   legend;
        int                           index            = 0;
        bool                          selected         = false;
        bool                          always_available = false;
        bool                          is_percentage    = false;
        double                        scale            = 1.0;
        NumericalAggregator<uint64_t> value;
        GraphType                     graph_type       = GraphType::LINE;
    };

public:
    SDP(std::string device_name);
    virtual ~SDP() final;

/////////////////////////////
public: // ModuleInterface //
/////////////////////////////

    /*
    */
    virtual const char* GetModuleName() final;

    /*
    * Initialize this module
    */
    virtual bool Initialize() final;

    /*
    * De-inject the tool, if any
    * This module should be shutdown afterwards since there are no guarantees it can continue
    * to support requests
    */
    virtual void RemoveToolFromDevice() final;

    /*
    */
    virtual void Reset() final;

    /*
    * Pause this module
    */
    virtual void Pause() final;

    /*
    * Stop this module
    */
    virtual void Stop() final;

    /*
    * Begin recording process
    */
    virtual void Record() final;

    /*
    * Goes back to recording or active, depending on the previous operation mode
    */
    virtual void Resume() final;

    /*
    */
    virtual void Update(float time_elapsed) final;

    /*
    */
    virtual void Draw(float time_elapsed) final;

/////////////////////////////
public: /////////////////////
/////////////////////////////

private:

    /*
    */
    float GetActiveTimeBySampleCountAndFrequency() const;

    /*
    */
    void RefreshAsyncUpdateThread(int previous_sampling_period_ms = 0);

    /*
    */
    void FixDisabledGroupEntries();

    /*
    */
    void SetupRuntimeOptions();

    /*
    */
    void ParseConsoleOutput(std::string_view console_output);

    /*
    */
    void DrawSetupSection(double time_elapsed);

    /*
    */
    void DrawGeneralSection(double time_elapsed);

    /*
    */
    void DrawPlaygroundSection(double time_elapsed);

    /*
    */
    void DrawPlaygroundOptions(double time_elapsed);

    /*
    */
    void DrawPlaygroundPlots(double time_elapsed);

////////////////////////////////////
private: // SPECIALIZATION GROUPS //
////////////////////////////////////

    /*
    */
    bool GetRuntimeShaderStallValues(
        NumericalAggregatorValueType   aggregator_visualizer_type, 
        std::pmr::vector<const char*>& busy_vs_stall_labels, 
        std::pmr::vector<int>&         busy_vs_stall_data,
        std::pmr::vector<const char*>& stalls_labels, 
        std::pmr::vector<int>&         stalls_data);
    
    /*
    */
    bool GetRuntimeShaderStageValues(
        NumericalAggregatorValueType   aggregator_visualizer_type, 
        std::pmr::vector<const char*>& labels, 
        std::pmr::vector<int>&         data);

////////////////////////////////////
private: ///////////////////////////
////////////////////////////////////
    
    std::mutex            m_safety;
    ConsoleHelper         m_console_helper;
    bool                  m_console_ignore_header       = true;
    bool                  m_has_sdp_cli                 = false;
    bool                  m_did_setup_supported_options = false;
    std::array<char, 256> m_sdp_cli_path;
    std::array<char, 256> m_profilling_layer_path;
    int                   m_sampling_period_ms    = 1000;

    // Playground stuff
    bool m_playground_selector_visible     = true;
    bool m_playground_auto_scroll          = true;
    float m_playground_history_range_begin = 0.0f;  // Seconds
    float m_playground_history_range_end   = 10.0f; // Seconds

    std::pmr::unsynchronized_pool_resource m_memory_resource;

    std::map<std::string, std::map<int, RealtimeOption>>                    m_realtime_options_by_category;
    std::unordered_map<std::string, std::reference_wrapper<RealtimeOption>> m_realtime_options_by_name;
    std::unordered_map<int, std::reference_wrapper<RealtimeOption>>         m_realtime_options_by_index;
    std::vector<std::vector<int>>                                           m_realtime_group_options_ordered;

    // NumericalAggregatorValueType m_aggregator_visualizer_type = NUMERICAL_AGGREGATOR_CURRENT;
};
