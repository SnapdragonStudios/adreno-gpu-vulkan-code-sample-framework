//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "timerSimple.hpp"
#include <algorithm>


void TimerSimple::Update(uint32_t whichFrame, uint64_t startTick, uint64_t stopTick)
{
    // Check that the timer hasn't been reset (recently) and we are not still getting out of date results from inflight timers (recorded before the reset was called on the cpu and subsequently executed on the gpu)

    if (InvalidatedFrame != -1)
    {
        if (InvalidatedFrame != int(whichFrame))
            return;
        InvalidatedFrame = -1;  // timer is valid until next Reset
    } 
    uint64_t elapsedTicks = stopTick - startTick;
    LastStartTick = startTick;
    LastStopTick = stopTick;
    if (elapsedTicks < 1000000000)
    {
        TotalCompletedTicks += elapsedTicks;
        ++CompletedCount;
    }
    else
    {
        // >= 10 seconds is probably an issue!
        //assert(elapsedTicks < 10000000000); // 10 seconds!
    }
}

double TimerPoolSimple::GetTimeInMs(const TimerSimple& timer) const
{
    auto lastCompletedTicks = timer.LastStopTick - timer.LastStartTick;
    return (double(GetTimestampPeriod()) * double(lastCompletedTicks)) / 1000000.0;
}

double TimerPoolSimple::GetAverageTimeInMs(const TimerSimple& timer) const
{
    return (timer.CompletedCount <= 0) ? 0.0 : (double(GetTimestampPeriod()) * double(timer.TotalCompletedTicks) / double(timer.CompletedCount)) / 1000000.0;
}

long long TimerPoolSimple::GetTimeInUs(const TimerSimple& timer) const
{
    auto lastCompletedTicks = timer.LastStopTick - timer.LastStartTick;
    return (long long)((double(GetTimestampPeriod()) * double(lastCompletedTicks)) / 1000.0);
}

long long TimerPoolSimple::GetAverageTimeInUs(const TimerSimple& timer) const
{
    return (timer.CompletedCount <= 0) ? 0LL : (long long)((double(GetTimestampPeriod()) * double(timer.TotalCompletedTicks) / double(timer.CompletedCount)) / 1000.0);
}

void TimerPoolSimple::Log( const TimerPoolSimple::tTimers& timers ) const
{
    for (const auto& timer : timers)
    {
        if (timer.TotalCompletedTicks > 0)
            LOGI( "Timer %s :\t%.3fms\t(avg %.3fms)", timer.Name.c_str(), GetTimeInMs(timer), GetAverageTimeInMs(timer) );
    }
}

void TimerPoolSimple::Log2( const TimerPoolSimple::tTimers& timers ) const
{
    struct TimerEvent
    {
        const TimerSimple* pTimer;
        bool isStop;                // false=event is start of pTimer, true=event is stop of pTimer
    };

    std::vector<TimerEvent> timersInEventOrder;
    timersInEventOrder.resize( timers.size() * 2 );
    for (size_t timerIdx=0; const auto& timer: timers)
    {
        timersInEventOrder[timerIdx * 2]     = { &timer, false };
        timersInEventOrder[timerIdx * 2 + 1] = { &timer, true };
        ++timerIdx;
    }
    std::sort( std::begin( timersInEventOrder ), std::end( timersInEventOrder ), []( auto a, auto b ) -> bool { return (a.isStop ? a.pTimer->LastStopTick : a.pTimer->LastStartTick) < (b.isStop ? b.pTimer->LastStopTick : b.pTimer->LastStartTick); } );

    constexpr uint32_t cMaxTimerOverlaps = 4;
    constexpr uint32_t cMaxNameLength = 20;
    constexpr uint32_t cQueueFamilyWidth = cMaxTimerOverlaps * 2 + cMaxNameLength;
    constexpr uint32_t cMaxQueueFamilies = 3;
    struct 
    {
        std::array<TimerEvent, cMaxTimerOverlaps> timerOverlap;
        int overlaps = 0;
        int familyIndentCharacters = -1;
    } QueueFamilyOverlaps[cMaxQueueFamilies];
    int numUsedFamilies = 0;


    const TimerSimple* pPrevTimer = nullptr;
    char rangesText[cQueueFamilyWidth * cMaxQueueFamilies + 1];
    memset( rangesText, ' ', sizeof( rangesText ) - 1 );
    rangesText[sizeof( rangesText ) - 1] = 0;

    LOGI( "GPU timers:" );

    for (const auto& timerEvent : timersInEventOrder)
    {
        const auto queueFamily = std::min(timerEvent.pTimer->DeviceQueueFamilyIndex, cMaxQueueFamilies-1); // ok to lump everything into the 'last' queue family
        auto& overlaps = QueueFamilyOverlaps[queueFamily];
        if (overlaps.familyIndentCharacters == -1)
            overlaps.familyIndentCharacters = cQueueFamilyWidth * numUsedFamilies++;
        const char*const name = timerEvent.pTimer->Name.c_str();
        const size_t nameLength = timerEvent.pTimer->Name.length();
        if (!timerEvent.isStop)
        {
            // start event
            overlaps.timerOverlap[overlaps.overlaps] = timerEvent;
            if (overlaps.overlaps < cMaxTimerOverlaps - 1)
                ++overlaps.overlaps;
            if (timerEvent.pTimer->TotalCompletedTicks > 0)
            {
                size_t textOffset = overlaps.overlaps*2 + overlaps.familyIndentCharacters;
//                LOGI( "%s", rangesText );
//                rangesText[textOffset] = '-';
//                LOGI( "%s", rangesText);
                size_t textEndOffset = textOffset + snprintf( &rangesText[textOffset], sizeof( rangesText ) - textOffset - 1, "-%.*s", cMaxNameLength, name );
                rangesText[textEndOffset] = ' ';
                LOGI( "%s", rangesText );
                memset( &rangesText[textOffset + 2], ' ', cMaxNameLength );
                textEndOffset = textOffset + snprintf( &rangesText[textOffset], sizeof( rangesText ) - textOffset - 1, " (%.3fms avg)", GetAverageTimeInMs( *timerEvent.pTimer ) );
                rangesText[textEndOffset] = ' ';
                LOGI( "%s", rangesText );
                rangesText[textOffset] = ' ';
                rangesText[textOffset+1] = '|';
                memset( &rangesText[textOffset + 2], ' ', cMaxNameLength);
                //LOGI( "%*c/ %s%*c%.3fms (avg %.3fms)", overlaps.overlaps, ' ', name, 16 - nameLength - overlaps.overlaps + queueFamily * cQueueFamilyTabSize, ' ', GetTimeInMs( *timerEvent.pTimer ), GetAverageTimeInMs( *timerEvent.pTimer ) );
            }
        }
        else
        {
            rangesText[overlaps.overlaps*2 + 1 + overlaps.familyIndentCharacters] = ' ';
            //LOGI( "%s", rangesText );
            //LOGI( "%*c\\ %s%*c", overlaps.overlaps, ' ', name, 16 - nameLength - overlaps.overlaps + queueFamily * cQueueFamilyTabSize, ' ');
            while (overlaps.overlaps > 0 && timerEvent.pTimer->LastStopTick >= overlaps.timerOverlap[overlaps.overlaps - 1].pTimer->LastStopTick)
            {
                rangesText[overlaps.overlaps*2 + 1 + overlaps.familyIndentCharacters] = ' ';
                --overlaps.overlaps;
            }
        }
    }
}
