//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "console_common.hpp"
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

std::vector<std::string> GetConnectedDeviceList()
{
    ConsoleHelper console_helper;
    auto console_output = console_helper.ExecuteSynchronousCommand("adb devices");

    std::vector<std::string> device_names;
    std::istringstream iss(console_output);
    std::string line;

    // Skip the first line (header)
    std::getline(iss, line);

    // Read each subsequent line and extract the device name
    while (std::getline(iss, line))
    {
        std::istringstream line_stream(line);
        std::string device_name;
        line_stream >> device_name; // Assuming the device name is the first token
        if (!device_name.empty()) 
        {
            device_names.push_back(device_name);
        }
    }

    return device_names;
}

std::string RunADBCommandForDevice(std::string_view device_name, std::string_view command)
{
    ConsoleHelper console_helper;
    const std::string command_string = std::string("adb -s ").append(device_name).append(" ").append(command);
    return console_helper.ExecuteSynchronousCommand(command_string);
}

void EnterDeviceRootMode(std::string_view device_name, bool wait_for_device)
{
    ConsoleHelper console_helper;

    const std::string enter_root_command  = std::string("adb -s ").append(device_name).append(" root");
    const std::string wait_device_command = std::string("adb -s ").append(device_name).append(" wait-for-device");

    console_helper.ExecuteSynchronousCommand(enter_root_command);
    if (wait_for_device)
    {
        console_helper.ExecuteSynchronousCommand(wait_device_command);
    }
}

bool DoesDeviceFileExist(std::string_view device_name, std::string_view file_path)
{
    ConsoleHelper console_helper;

    const std::string find_file_command = std::string("adb -s ").append(device_name).append(" shell ls ").append(file_path);
    const auto console_output           = console_helper.ExecuteSynchronousCommand(find_file_command);

    return console_output.find(file_path) != std::string::npos;
}

std::string GetDeviceFileContents(std::string_view device_name, std::string_view file_path)
{
    if (!DoesDeviceFileExist(device_name, file_path))
    {
        return std::string();
    }

    ConsoleHelper console_helper;
    const std::string cat_file_command = std::string("adb -s ").append(device_name).append(" cat ").append(file_path);
    const auto console_output          = console_helper.ExecuteSynchronousCommand(cat_file_command);

    return console_output;
}