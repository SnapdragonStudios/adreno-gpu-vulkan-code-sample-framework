//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "console_helper.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <chrono>
#include <string>
#include <string_view>

// Windows stuff
#include <windows.h>
#include <io.h>

#define COMMAND_LINE_BUFFER_SIZE 1024

ConsoleHelper::ConsoleHelper()
{
}

ConsoleHelper::~ConsoleHelper()
{
    if (m_is_asynchronous)
    {
        TerminateAsynchronousCommand(true);
    }

    // Ensure all threads died (even the ones we didn't wait for)
    for (auto& active_thread : m_non_waited_threads)
    {
        if (active_thread.joinable())
            active_thread.join();
    }
}

std::string ConsoleHelper::ExecuteSynchronousCommand(std::string_view in_cmd)
{
    m_is_asynchronous = false;

    char buffer[COMMAND_LINE_BUFFER_SIZE];

    FILE* pipe = _popen(in_cmd.data(), "r");
    if (!pipe)
    {
        return "ERROR";
    }

    auto process_handle = _get_osfhandle(_fileno(pipe));

    if(!feof(pipe)) 
    {
        while(fgets(buffer, 1024, pipe) != NULL)
        {
            m_command_result += buffer;
        }
    }

    if (pipe != nullptr)
    {
        TerminateProcess((HANDLE)(process_handle), 0);
        _pclose(pipe);
    }

    return m_command_result;
}

void ConsoleHelper::ExecuteAsynchronousCommand(
        std::string_view                   cmd, 
        std::optional<OnCommandLineOutput> command_line_callback,
        bool                               ignore_first_line)
{
    m_is_asynchronous = true;

    if (command_line_callback)
    {
        m_command_line_callback = std::move(command_line_callback.value());
    }

    // Hold a hard copy since thread will manage its lifetime
    std::string cmd_copy = cmd.data();

    // Grab the current async version index
    const int async_version_index = m_async_thread_version;

    m_async_command_thread = std::thread([cmd_copy, ignore_first_line, async_version_index, this]() -> void
    {
        char buffer[COMMAND_LINE_BUFFER_SIZE];
        bool should_ignore_next_line = ignore_first_line;

        FILE* pipe = _popen(cmd_copy.data(), "r");
        if (!pipe)
        {
            return;
        }

        m_safety.lock();
        m_async_command_handle = _get_osfhandle(_fileno(pipe));
        m_safety.unlock();

        while (!feof(pipe) && m_async_command_handle && async_version_index == m_async_thread_version) 
        {
            if(fgets(buffer, 1024, pipe) != NULL && !should_ignore_next_line)
            {
                std::lock_guard l(m_safety);

                // Check it again here in case it changed before we acquired the lock
                if (async_version_index != m_async_thread_version)
                {
                    continue;
                }

                m_command_result += buffer;

                if (m_command_line_callback)
                {
                    m_command_line_callback(buffer);
                }
            }

            should_ignore_next_line = false;
        }

        std::lock_guard l(m_safety);

        // Only close the pipe if the async thread index is still the same, otherwise it was already closed
        // elsewhere
        if (pipe != nullptr && async_version_index == m_async_thread_version)
        {
            _pclose(pipe);
            TerminateProcess((HANDLE)(m_async_command_handle), 0);
            m_async_command_handle = 0;
        }

#if 0
        if (pipe != nullptr)
        {
            _pclose(pipe);

            if (m_async_command_handle)
            {
                TerminateProcess((HANDLE)(m_async_command_handle), 0);
                m_async_command_handle = 0;
            }
        }
#endif
    });
}

std::string ConsoleHelper::GetAsynchronousCommandResult(bool terminate)
{
    m_safety.lock();
    auto result = m_command_result;
    m_command_result.clear();
    m_safety.unlock();

    if (terminate && m_is_asynchronous) 
    {
        TerminateAsynchronousCommand();
    }

    return std::move(result);
}

void ConsoleHelper::TerminateAsynchronousCommand(bool wait)
{
    m_safety.lock();
    if (m_async_command_handle)
    {
        TerminateProcess((HANDLE)(m_async_command_handle), 0);
        m_async_command_handle = 0;
    }

    m_async_thread_version++;

    m_safety.unlock();

    if (wait)
    {
        if (m_async_command_thread.joinable())
            m_async_command_thread.join();
    }
    else
    {
        m_non_waited_threads.push_back(std::move(m_async_command_thread));
        m_async_command_thread = std::thread();
    }
}

#if 0


std::string exec(const char* cmd, bool wait = true) 
{
    while(!s_enable && wait)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    FILE* pipe = _popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[1024];
    std::string result = "";
    while(!feof(pipe)) 
    {
        if(fgets(buffer, 1024, pipe) != NULL)
        {
            result += buffer;
            _fclose_nolock(pipe);
            pipe = nullptr;
            break;
        }
    }
    if (pipe != nullptr)
    {
        _pclose(pipe);
    }

    if (!result.empty())
    {
        m_val = result;
    }

    return result;
}

#endif