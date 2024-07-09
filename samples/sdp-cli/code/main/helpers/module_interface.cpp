//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "module_interface.hpp"
#include "portable-file-dialogs/portable-file-dialogs.h"
#include <string>

ModuleInterface::ModuleInterface(std::string device_name)
    : m_device_name(std::move(device_name))
{
}

void ModuleInterface::RemoveToolFromDevice()
{
}

void ModuleInterface::Reset()
{
    m_recording_time = 0.0;
    m_active_time    = 0.0;
}

void ModuleInterface::Pause()
{
    m_previous_state = m_state;
    m_state          = ModuleState::PAUSED;
}

void ModuleInterface::Stop()
{
    Reset();
    m_previous_state = m_state;
    m_state          = ModuleState::STOPPED;
    m_recording_time = 0.0;
    m_active_time    = 0.0;
}

void ModuleInterface::Record()
{
    Reset();
    m_previous_state = m_state;
    m_state          = ModuleState::RECORDING;
    m_recording_time = 0.0;
}

void ModuleInterface::Resume()
{
    const auto previous_state_local = m_previous_state;
    m_previous_state                = m_state;

    switch (previous_state_local)
    {
        case ModuleState::RECORDING:
            m_state = ModuleState::RECORDING;
            break;
            
        case ModuleState::ACTIVE:
            m_state = ModuleState::ACTIVE;
            break;
          
        default:
            Reset();
            m_state = ModuleState::STOPPED;
            break;
    }
}

void ModuleInterface::Update(float time_elapsed)
{
    if (m_state == ModuleState::RECORDING)
    {
        m_recording_time += time_elapsed;
        m_active_time     += time_elapsed;
    }
    else if (m_state == ModuleState::ACTIVE)
    {
        m_active_time += time_elapsed;
    }
}

void ModuleInterface::Draw(float time_elapsed)
{
}

std::string ModuleInterface::RequestFileSelection(std::string_view file_name) 
{
    auto open_file_query = pfd::open_file(std::string(file_name), pfd::path::home());
    const auto selection = open_file_query.result();
    if (selection.empty())
    {
        return std::string();
    }

    return selection[0];
}