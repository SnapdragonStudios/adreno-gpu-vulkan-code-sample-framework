//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#ifdef OS_ANDROID

// These are always defined
#define SYS_PROFILING_ENABLED
#define SYS_PROFILE_ATRACE

#if defined(SYS_PROFILING_ENABLED)

#if defined(SYS_PROFILE_ATRACE)

#include <time.h>
//#include "android/trace.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

extern void *(*ATrace_beginSection) (const char* sectionName);
extern void *(*ATrace_endSection) (void);
extern void *(*ATrace_isEnabled) (void);

void InitializeATrace();
void ShutdownATrace();

#define ATRACE_INT(name,val)
#define ATRACE_INT64(name,val)

#define GROUP_GENERIC
#define GROUP_VKFRAMEWORK

#define PROFILE_FASTTIME()

//PROFILE_INITIALIZE
#define PROFILE_INITIALIZE() InitializeATrace()

//PROFILE_SHUTDOWN
#define PROFILE_SHUTDOWN() ShutdownATrace()

#define PROFILE_SCOPE_FILTERED(context, threshold, flags, format, ...)

//PROFILE_SCOPE

#define SYS_ATRACE_BEGIN(format, ...) \
   { \
       static char atrace_begin_buf[1024]; \
	   if (strchr(format,'%')){ \
		   snprintf(atrace_begin_buf, 1024, format,##__VA_ARGS__); \
		   ATrace_beginSection(atrace_begin_buf); \
	   }else{ \
	       ATrace_beginSection(format); \
	   } \
   }

#define PROFILE_SCOPE(context, flags, format, ...) SYS_ATRACE_BEGIN(format,##__VA_ARGS__)

#define PROFILE_SCOPE_END() ATrace_endSection()

//PROFILE_SCOPE_DEFAULT
#define PROFILE_SCOPE_DEFAULT(context) PROFILE_SCOPE(context, TMZF_NONE, __FUNCTION__)
#define PROFILE_SCOPE_IDLE(context)
#define PROFILE_SCOPE_STALL(context)

//PROFILE_TICK
#define PROFILE_TICK()
/*
{ \
    struct timespec now; \
    clock_gettime(CLOCK_MONOTONIC, &now); \
    ATRACE_INT64("sys_tick_ns" ,(int64_t) now.tv_sec*1000000000LL + now.tv_nsec); \
    }
*/

// PROFILE_EXIT
#define PROFILE_EXIT(context) ATrace_endSection()

#define PROFILE_EXIT_EX(context, match_id, thread_id, filename, line)

#define PROFILE_TRY_LOCK(context, ptr, lock_name, ... )

#define PROFILE_TRY_LOCK_EX(context, matcher, threshold, filename, line, ptr, lock_name, ... )

#define PROFILE_END_TRY_LOCK(context, ptr, result )

#define PROFILE_END_TRY_LOCK_EX(context, match_id, filename, line, ptr, result )

// PROFILE_BEGIN_TIME_SPAN_AT
#define PROFILE_BEGIN_TIME_SPAN(context, id, flags, name_format, ... )

// PROFILE_END_TIME_SPAN
#define PROFILE_END_TIME_SPAN(context, id, flags, name_format, ... )

#define PROFILE_BEGIN_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... )

#define PROFILE_END_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... )

#define PROFILE_SIGNAL_LOCK_COUNT(context, ptr, count, description, ... )

#define PROFILE_SET_LOCK_STATE(context, ptr, state, description, ... )

#define PROFILE_SET_LOCK_STATE_EX(context, filename, line, ptr, state, description, ... )

#define PROFILE_SET_LOCK_STATE_MIN_TIME(context, buf, ptr, state, description, ... )

#define PROFILE_SET_LOCK_STATE_MIN_TIME_EX(context, buf, filename, line, ptr, state, description, ... )

// PROFILE_THREAD_NAME
#define PROFILE_THREAD_NAME(context, thread_id, name_format, ... ) ATRACE_INT(name_format, thread_id)

#define PROFILE_LOCK_NAME(context, ptr, name_format, ... )

#define PROFILE_EMIT_ACCUMULATION_ZONE(context, zone_flags, start, count, total, zone_format, ... )

#define PROFILE_SET_VARIABLE(context, key, value_format, ... )

#define PROFILE_SET_TIMELINE_SECTION_NAME(context, name_format, ... )

//PROFILE_ENTER
#define PROFILE_ENTER(context, flags, zone_name, ... ) SYS_ATRACE_BEGIN(zone_name,##__VA_ARGS__)

#define PROFILE_ENTER_EX(context, match_id, thread_id, threshold, filename, line, flags, zone_name, ... )

#define PROFILE_ALLOC(context, ptr, size, description, ... )

#define PROFILE_ALLOC_EX(context, filename, line_number, ptr, size, description, ... )

#define PROFILE_FREE(context, ptr) ATrace_endSection()

//PROFILE_MESSAGE
#define PROFILE_MESSAGE(context, flags, format_string, ... )
#define PROFILE_LOG(context, flags, format_string, ... )
#define PROFILE_WARNING(context, flags, format_string, ... )
#define PROFILE_ERROR(context, flags, format_string, ... )

#define PROFILE_PLOT(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_F32(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_F64(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_I32(context, type, flags, value, name_format, ... ) ATRACE_INT(name_format, value)

#define PROFILE_PLOT_U32(context, type, flags, value, name_format, ... ) ATRACE_INT(name_format, (int)value)

#define PROFILE_PLOT_I64(context, type, flags, value, name_format, ... ) ATRACE_INT64(name_format, value)

#define PROFILE_PLOT_U64(context, type, flags, value, name_format, ... ) ATRACE_INT64(name_format, (int64_t)value)

#define PROFILE_PLOT_AT(context, timestamp, type, flags, value, name_format, ... )

#define PROFILE_BLOB(context, data, data_size, plugin_identifier, blob_name, ...)

#define PROFILE_DISJOINT_BLOB(context, num_pieces, data, data_sizes, plugin_identifier, blob_name, ... )

#define PROFILE_SEND_CALLSTACK(context, callstack)

#endif //defined(SYS_PROFILE_ATRACE)

#else  //defined(SYS_PROFILING_ENABLED)

#define GROUP_GENERIC              
#define GROUP_FFTPLUGIN              

#define PROFILE_FASTTIME() 

#define PROFILE_INITIALIZE() 

#define PROFILE_SHUTDOWN() 

#define PROFILE_SCOPE_FILTERED(context, threshold, flags, format, ...) 

#define PROFILE_SCOPE(context, flags, format, ...) 

#define PROFILE_SCOPE_DEFAULT(context) 
#define PROFILE_SCOPE_IDLE(context)
#define PROFILE_SCOPE_STALL(context) 

#define PROFILE_TICK()

#define PROFILE_EXIT(context) 

#define PROFILE_EXIT_EX(context, match_id, thread_id, filename, line)

#define PROFILE_TRY_LOCK(context, ptr, lock_name, ... ) 

#define PROFILE_TRY_LOCK_EX(context, matcher, threshold, filename, line, ptr, lock_name, ... ) 

#define PROFILE_END_TRY_LOCK(context, ptr, result ) 

#define PROFILE_END_TRY_LOCK_EX(context, match_id, filename, line, ptr, result )

#define PROFILE_BEGIN_TIME_SPAN(context, id, flags, name_format, ... ) 

#define PROFILE_END_TIME_SPAN(context, id, flags, name_format, ... ) 

#define PROFILE_BEGIN_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... ) 

#define PROFILE_END_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... ) 

#define PROFILE_SIGNAL_LOCK_COUNT(context, ptr, count, description, ... ) 

#define PROFILE_SET_LOCK_STATE(context, ptr, state, description, ... ) 

#define PROFILE_SET_LOCK_STATE_EX(context, filename, line, ptr, state, description, ... ) 

#define PROFILE_SET_LOCK_STATE_MIN_TIME(context, buf, ptr, state, description, ... ) 

#define PROFILE_SET_LOCK_STATE_MIN_TIME_EX(context, buf, filename, line, ptr, state, description, ... ) 

#define PROFILE_THREAD_NAME(context, thread_id, name_format, ... ) 

#define PROFILE_LOCK_NAME(context, ptr, name_format, ... ) 

#define PROFILE_EMIT_ACCUMULATION_ZONE(context, zone_flags, start, count, total, zone_format, ... ) 

#define PROFILE_SET_VARIABLE(context, key, value_format, ... ) 

#define PROFILE_SET_TIMELINE_SECTION_NAME(context, name_format, ... ) 

#define PROFILE_ENTER(context, flags, zone_name, ... ) 

#define PROFILE_ENTER_EX(context, match_id, thread_id, threshold, filename, line, flags, zone_name, ... ) 

#define PROFILE_ALLOC(context, ptr, size, description, ... ) 

#define PROFILE_ALLOC_EX(context, filename, line_number, ptr, size, description, ... ) 

#define PROFILE_FREE(context, ptr) 

#define PROFILE_MESSAGE(context, flags, format_string, ... ) 
#define PROFILE_LOG(context, flags, format_string, ... ) 
#define PROFILE_WARNING(context, flags, format_string, ... ) 
#define PROFILE_ERROR(context, flags, format_string, ... ) 

#define PROFILE_PLOT(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_F32(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_F64(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_I32(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_U32(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_I64(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_U64(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_AT(context, timestamp, type, flags, value, name_format, ... ) 

#define PROFILE_BLOB(context, data, data_size, plugin_identifier, blob_name, ...) 

#define PROFILE_DISJOINT_BLOB(context, num_pieces, data, data_sizes, plugin_identifier, blob_name, ... )

#define PROFILE_SEND_CALLSTACK(context, callstack) 

#endif //defined(SYS_PROFILING_ENABLED)

#endif // OS_ANDROID

