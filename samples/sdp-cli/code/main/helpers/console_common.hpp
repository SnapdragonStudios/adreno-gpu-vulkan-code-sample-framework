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

/*
*/
std::vector<std::string> GetConnectedDeviceList();

/*
*/
std::string RunADBCommandForDevice(std::string_view device_name, std::string_view command);

/*
*/
void EnterDeviceRootMode(std::string_view device_name, bool wait_for_device = true);

/*
*/
bool DoesDeviceFileExist(std::string_view device_name, std::string_view file_path);

/*
*/
std::string GetDeviceFileContents(std::string_view device_name, std::string_view file_path);