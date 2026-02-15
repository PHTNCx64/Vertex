//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#if defined (_WIN32) || defined(_WIN64)
#include <Windows.h>
using native_handle = HANDLE;

enum class ProcessArchitecture
{
    X86,
    X86_64,
    ARM64,
    UNKNOWN,
};

#elif defined (__linux__) || defined(__linux) || defined (linux)
using native_handle = int;
#elif defined (__APPLE__) || defined(__MACH__)
#include <mach/port.h>
using native_handle = mach_port_t;
#endif /* defined (_WIN32) || defined(_WIN64) */