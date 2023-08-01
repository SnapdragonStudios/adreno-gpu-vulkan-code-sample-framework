//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @file timerPool.hpp
/// Support for Vulkan timers (using vulkan query / query pool).
/// Base class and template that gpu timer functionality can be built upon.
/// @ingroup Vulkan

#include "vulkan.hpp"
#include "memory/vulkan/uniform.hpp"
#include <set>
#include <stack>
#include <string>


/// @brief Timer pool base class
/// Hardware level management, issuing, and reading of GPU timer queries.
class TimerPoolBase
{
public:
    TimerPoolBase( Vulkan& vulkan ) noexcept;
    TimerPoolBase( const TimerPoolBase& ) = delete;
    TimerPoolBase& operator=( const TimerPoolBase& ) = delete;
    virtual ~TimerPoolBase();

    bool Initialize( uint32_t maxTimers );
    void Destroy();

    // Identifier for a single timer
    typedef int TimerId;

    /// @brief Record a 'start' timer on the given gpu command list
    /// @param commandBuffer command buffer to add the 'start' timer to
    /// @return timerId (to be used with @StopGpuTimer).
    /// @param deviceQueueFamilyIndex index of the Vulkan device queue
    TimerId StartGpuTimer( VkCommandBuffer commandBuffer, const std::string_view& timerName, int deviceQueueFamilyIndex );

    /// @brief Record a 'stop' timer on the given gpu command list
    /// @param timerId from a previous @StartGpuTimer
    void StopGpuTimer( VkCommandBuffer commandBuffer, TimerId timerId );

    /// @brief Function to mark the given timers as being no longer on a command list.
    /// Once a timer is no longer on a command list it can be considered for re-use (although we may still be waiting for its results)
    void TimerCommandBufferReset(std::span<TimerId> timerIds);

    /// Record the GPU commands to read back this frame's GPU timer 'results' in to the m_VulkanQueryResults buffer and reset the queries ready for next frame.
    /// Should be recorded at the end of each frame.
    void ReadResults(VkCommandBuffer commandBuffer, uint32_t whichFrame);

    /// Read (back) the last completed frame's results written to m_VulkanQueryResults (by GPU via ReadResults) and update the Timers.
    /// Once this is completed the timers can be displayed etc via GetResults()
    virtual void UpdateResults(uint32_t whichFrame);

    class TimerBase {
    public:
        virtual void Update(uint32_t whichFrame, uint64_t startTick, uint64_t stopTick) = 0;
    };

protected:
    /// Function to return the Timer(Base) that is uniquely identified by 'timerName' and the queue family.
    /// Expected to create a new timer if one does not already exist. 
    virtual TimerBase* FindOrAddTimer(std::string_view timerName, uint32_t deviceQueueFamilyIndex) = 0;

    /// Get the number of nanoseconds in a single GPU timer tick (
    float GetTimestampPeriod() const { return m_TimeStampPeriod; }

private:

    void ResetQueryPool(uint32_t firstResetQuery, uint32_t queryResetCount);

    Vulkan&                         m_Vulkan;
    PFN_vkResetQueryPoolEXT         m_vkResetQueryPoolEXT = nullptr;    // host pool reset function, pulled from the VK_EXT_host_query_reset extension.
    std::vector<uint64_t>           m_DeviceQueueValidTimerBitMask;     // mask of bits that are valid in the value returned from the vulkan gpu timer query (per device queue family)
    float                           m_TimeStampPeriod = 0.0f;           // number of nanoseconds represented by single 'tick' returned from the vulkan gpu timer query.
    VkQueryPool                     m_VulkanQueryPool = VK_NULL_HANDLE;
    int                             m_MaxUsedTimerIndex = -1;
    uint32_t                        m_TimerCount = 0;                   // Maximum number of timers available for use.
    std::stack<uint32_t>            m_FreeQueryPoolEntries;

    // Data for the vulkan queries (currently scheduled/running queries)...  timer slots are only released when the command list they were added to is reset.
    struct VulkanQueryResult {
        uint64_t Timestamp;     // Timestamp recorded by the GPU
        uint64_t Availability;  // non zero indicates Timestamp is available (and valid)
    };
    typedef int tDeviceQueueFamilyIndex;
    struct TimerQuery {
        TimerBase* pTimer = nullptr;                        // Timer that this query will update (multiple TimerQuery can point to the same timer)
        bool RecordedInCommandList = false;                 // flag to indicate if this query is currently in a command buffer (once the timer is not in a command buffer it is a candidate for reuse)
    };
    std::vector<TimerQuery> m_TimerQueries;                 // Each timer query maps to pair of queries in m_VulkanQueryPool (one for start time and one for stop time).

    // Query results are copied into this buffer at the end of every frame, just before the query results are reset.
    // Resetting on the CPU is fraught with syncronization issues, this way should allow us to support dynamic command buffers
    // and static (and secondary) command buffers!
    UniformArray<Vulkan, NUM_VULKAN_BUFFERS> m_VulkanQueryResults;
};


template<typename T>
class TTimerPool : public TimerPoolBase
{
public:
    TTimerPool(Vulkan& vulkan) noexcept : TimerPoolBase(vulkan) {};

    typedef std::set<T, std::less<> > tTimers;

    TimerBase* FindOrAddTimer(std::string_view timerName, uint32_t deviceQueueFamilyIndex ) {
        auto it = m_Timers.find( std::pair { timerName, deviceQueueFamilyIndex } );
        if (it == m_Timers.end())
        {
            it = m_Timers.emplace( timerName, deviceQueueFamilyIndex ).first;
        }
        assert( deviceQueueFamilyIndex == it->DeviceQueueFamilyIndex );
        return const_cast<TimerBase*>(static_cast<const TimerBase*>(&(*it)));  // ok to remove constness so long as we dont mess with the sort key (eg timerName)
    }

    /// Return the current set of timer data (data is updated by TimerPoolBase::UpdateResults)
    const auto& GetResults() const { return m_Timers; }

    /// Reset the currently stored results
    /// @param currentFrame frame on which we are resetting (ie current frame/swapchain index) or -1 if we are resetting but dont want to invalidate results that are already inflight (on command buffers submitted or waiting to be submitted to the GPU)
    void ResetTimers(int currentFrame) { for (auto& timer : m_Timers) const_cast<T&>(timer).Reset(currentFrame); }

protected:
    tTimers                         m_Timers;
};
