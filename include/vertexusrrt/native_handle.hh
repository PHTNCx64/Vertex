//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#if defined (_WIN32) || defined(_WIN64)
#include <windows.h>
#ifdef STATUS_TIMEOUT
#undef STATUS_TIMEOUT
#endif
using native_handle = HANDLE;
#elif defined (__linux__) || defined(__linux) || defined (linux)
using native_handle = int;
constexpr native_handle INVALID_HANDLE_VALUE = -1;
#elif defined (__APPLE__) || defined(__MACH__)
#include <mach/port.h>
using native_handle = mach_port_t;
#endif

enum class ProcessArchitecture
{
    X86,
    X86_64,
    ARM64,
    UNKNOWN,
};
