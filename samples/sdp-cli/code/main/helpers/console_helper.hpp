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
#include <string_view>
#include <mutex>
#include <functional>
#include <optional>

class ConsoleHelper
{
    using OnCommandLineOutput = std::function<void(std::string_view)>;

public:
    ConsoleHelper();
    ~ConsoleHelper();

    /*
    */
    std::string ExecuteSynchronousCommand(std::string_view in_cmd);

    /*
    */
    void ExecuteAsynchronousCommand(
        std::string_view                   cmd, 
        std::optional<OnCommandLineOutput> command_line_callback = std::nullopt,
        bool                               ignore_first_line     = false);

    /*
    */
    std::string GetAsynchronousCommandResult(bool terminate = false);

    /*
    */
    void TerminateAsynchronousCommand(bool wait = false);

    /*
    */
    inline bool IsAsynchronousCommandActive() const
    {
        return m_async_command_handle != 0;
    }

private:
    
    std::thread              m_async_command_thread;
    int64_t                  m_async_command_handle = 0;
    std::string              m_command_result;
    std::mutex               m_safety;
    bool                     m_is_asynchronous = false;
    OnCommandLineOutput      m_command_line_callback;
    std::vector<std::thread> m_non_waited_threads;
    std::atomic<int>         m_async_thread_version = 0;
};
