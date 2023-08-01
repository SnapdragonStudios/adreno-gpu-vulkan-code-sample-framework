//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include <stdio.h>

#include "profile.h"

// Need LOGX macros
#include "os_common.h"

#if defined(SYS_PROFILING_ENABLED) && defined(SYS_PROFILE_ATRACE)

#include <dlfcn.h>

void *(*ATrace_beginSection) (const char* sectionName);
void *(*ATrace_endSection) (void);
void *(*ATrace_isEnabled) (void);

typedef void *(*fp_ATrace_beginSection) (const char* sectionName);
typedef void *(*fp_ATrace_endSection) (void);
typedef void *(*fp_ATrace_isEnabled) (void);

void InitializeATrace()
{
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib != NULL) 
    {
        ATrace_beginSection =
            reinterpret_cast<fp_ATrace_beginSection >(
                dlsym(lib, "ATrace_beginSection"));
        ATrace_endSection =
            reinterpret_cast<fp_ATrace_endSection >(
                dlsym(lib, "ATrace_endSection"));
        ATrace_isEnabled =
            reinterpret_cast<fp_ATrace_isEnabled >(
                dlsym(lib, "ATrace_isEnabled"));
        LOGI("ATrace Profiling Initialized");
    }
    else
    {
        LOGE("Failed to open libandroid.so");
    }
}

void ShutdownATrace()
{

}

#endif

