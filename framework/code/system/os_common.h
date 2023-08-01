//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

/// @defgroup System
/// OS and 'system' level framework code.
/// @file os_common.hpp

#include <cstdint>

/// Set the application name (used primarily by Android to get correct path to app)
void        OS_SetApplicationName(const char* applicationName);

/// Return number of CPU cores (or 0 if unknown)
uint32_t    OS_GetNumCores();

/// Get current CPU (wall) time in ms (thousandths).
uint32_t    OS_GetTimeMS();

/// Get current CPU (wall) time in us (millionths).
uint64_t    OS_GetTimeUS();

/// Sleep this thread for the given number of ms
void        OS_SleepMs(uint32_t ms);

//
// Logging.
// Writes to logcat on Android and console on Windows.
//
// LOGI - Information
// LOGW - Warning
// LOGE - Error
//
#if OS_ANDROID
// Need log messages
#include <android/log.h>

// Need some log functions
extern const char* gpAndroidAppName;
extern const char* gpAndroidAppShortName;
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,  gpAndroidAppShortName, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN,  gpAndroidAppShortName, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, gpAndroidAppShortName, __VA_ARGS__))

#elif OS_WINDOWS

void LOGI(const char* pszFormat, ...);
void LOGW(const char* pszFormat, ...);
void LOGE(const char* pszFormat, ...);

#else

#error "Must define an OS_xxx!"

#endif // OS_WINDOWS
