//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cstdint>
#include <string>
#include "json/include/nlohmann/json_fwd.hpp"
#undef ERROR

enum ModuleState
{
    ACTIVE,
    RECORDING, // Also implies on ACTIVE
    PAUSED,
    STOPPED,
    ERROR,
    VIEW_ONLY, // Normally when loaded from disk, implies on STOPPED
};

class ModuleInterface
{
public:

    ModuleInterface(std::string device_name);
    virtual ~ModuleInterface() = default;

    /*
    */
    virtual const char* GetModuleName() = 0;

    /*
    * Initialize this module
    */
    virtual bool Initialize() = 0;

    /*
    * De-inject the tool, if any
    * This module should be shutdown afterwards since there are no guarantees it can continue
    * to support requests
    */
    virtual void RemoveToolFromDevice();

    /*
    */
    virtual void Reset();

    /*
    * Pause this module
    */
    virtual void Pause();

    /*
    * Stop this module
    */
    virtual void Stop();

    /*
    * Begin recording process
    */
    virtual void Record();

    /*
    * Goes back to recording or active, depending on the previous operation mode
    */
    virtual void Resume();

    /*
    */
    virtual void Update(float time_elapsed);

    /*
    */
    virtual void Draw(float time_elapsed);

    /*
    */
    inline ModuleState GetState() const
    {
        return m_state;
    }

protected:

    /*
    */
    std::string RequestFileSelection(std::string_view file_name);

protected:
    
    std::string m_device_name;
    ModuleState m_state          = ModuleState::ACTIVE;
    ModuleState m_previous_state = ModuleState::ACTIVE;
    float       m_recording_time = 0.0;
    float       m_active_time    = 0.0;
};
