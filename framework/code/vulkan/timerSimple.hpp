//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @file timerSimple.hpp
/// Vulkan timer pool implementation (simple timers - current time and average per timer).
/// @ingroup Vulkan

#include "timerPool.hpp"


/// Data for a single timer.
struct TimerSimple final : public TimerPoolBase::TimerBase
{
    auto operator<=>(const std::pair<std::string_view, uint32_t> a ) const noexcept
    {
        auto result = Name.compare( a.first );
        return result == 0 ? (int(a.second) - int(DeviceQueueFamilyIndex)) : result;
    }
    auto operator<=>(const TimerSimple& a) const noexcept
    {
        auto result = Name.compare(a.Name);
        return result == 0 ? (int(a.DeviceQueueFamilyIndex) - int(DeviceQueueFamilyIndex)) : result;
    }
    explicit TimerSimple(std::string_view name, uint32_t deviceQueueFamilyIndex) noexcept : Name(name), DeviceQueueFamilyIndex(deviceQueueFamilyIndex) {}

    /// @brief Reset this timer (discard old accumulated results)
    /// @param currentFrame frame that this timer was reset on (invalidate any in-flight frames until we start getting results for this frame index again) OR -1 if we dont want to ignore the inflight frames
    void Reset(int currentFrame)
    {
        CompletedCount = 0;
        TotalCompletedTicks = 0;
        LastStartTick = 0;
        LastStopTick = 0;
        InvalidatedFrame = currentFrame;
    }

    const std::string Name;
    int InflightCount = 0;
    int CompletedCount = 0;
    uint64_t TotalCompletedTicks = 0;
    uint64_t LastStartTick = 0;
    uint64_t LastStopTick = 0;
    int InvalidatedFrame = -1;  // frame index of the frame (buffer) this timer was reset on, used to invalidate in-flight results.  -1 denotes the timer is valid.
    uint32_t DeviceQueueFamilyIndex = 0;    // cannot split timers acrtoss queues and get reliable results (according to Vulkan spec)
protected:
    void Update(uint32_t whichFrame, uint64_t startTick, uint64_t stopTick) override;
};


class TimerPoolSimple : public TTimerPool<TimerSimple>
{
public:
    TimerPoolSimple(Vulkan& vulkan) noexcept : TTimerPool<TimerSimple>(vulkan) {};

    double GetTimeInMs(const TimerSimple& timer) const;
    double GetAverageTimeInMs(const TimerSimple& timer) const;
    long long GetTimeInUs(const TimerSimple& timer) const;
    long long GetAverageTimeInUs(const TimerSimple& timer) const;

    /// Log the results from a collection of timers (logs using LOGI)
    void Log(const tTimers& timers) const;
    void Log2( const TimerPoolSimple::tTimers& timers ) const;
    /// Take a snapshot of the current timer pool and reset the cumulative counters
    TimerPoolSimple::tTimers SnapshotAndReset();
};
