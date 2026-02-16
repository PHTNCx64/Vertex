//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <Windows.h>

#include <atomic>

extern native_handle& get_native_handle();

namespace
{
    std::atomic<ProcessArchitecture> g_cachedArchitecture{ProcessArchitecture::UNKNOWN};

    ProcessArchitecture detect_process_architecture()
    {
        const HANDLE process = get_native_handle();
        if (!process || process == INVALID_HANDLE_VALUE)
        {
            return ProcessArchitecture::UNKNOWN;
        }

        auto* IsWow64Process2Func = reinterpret_cast<BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*)>(GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "IsWow64Process2"));

        if (IsWow64Process2Func)
        {
            USHORT processMachine{};
            USHORT nativeMachine{};
            if (IsWow64Process2Func(process, &processMachine, &nativeMachine))
            {
                if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN)
                {
                    switch (nativeMachine)
                    {
                    case IMAGE_FILE_MACHINE_AMD64:
                        return ProcessArchitecture::X86_64;
                    case IMAGE_FILE_MACHINE_ARM64:
                        return ProcessArchitecture::ARM64;
                    case IMAGE_FILE_MACHINE_I386:
                        return ProcessArchitecture::X86;
                    default:
                        break;
                    }
                }
                else
                {
                    switch (processMachine)
                    {
                    case IMAGE_FILE_MACHINE_I386:
                        return ProcessArchitecture::X86;
                    default:
                        break;
                    }
                }
            }
        }
        else
        {
            BOOL isWow64 = FALSE;
            if (IsWow64Process(process, &isWow64))
            {
                if (isWow64)
                {
                    return ProcessArchitecture::X86;
                }
                return ProcessArchitecture::X86_64;
            }
        }

        return ProcessArchitecture::UNKNOWN;
    }
}

void cache_process_architecture()
{
    const ProcessArchitecture arch = detect_process_architecture();
    g_cachedArchitecture.store(arch, std::memory_order_release);
}

void clear_process_architecture()
{
    g_cachedArchitecture.store(ProcessArchitecture::UNKNOWN, std::memory_order_release);
}

ProcessArchitecture get_process_architecture()
{
    const ProcessArchitecture cached = g_cachedArchitecture.load(std::memory_order_acquire);
    if (cached != ProcessArchitecture::UNKNOWN)
    {
        return cached;
    }
    return detect_process_architecture();
}
