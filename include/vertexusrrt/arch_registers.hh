//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <sdk/api.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace PluginRuntime
{
    enum class ProcessArchitecture
    {
        Unknown,
        X86,
        X86_64,
        ARM64,
        ARM64_X86,
    };

#ifdef _WIN32
    [[nodiscard]] ProcessArchitecture detect_process_architecture(HANDLE processHandle);
#endif

    [[nodiscard]] const char* get_architecture_name(ProcessArchitecture arch);

    [[nodiscard]] StatusCode register_architecture(const Runtime* runtime, ProcessArchitecture arch);

    [[nodiscard]] StatusCode register_windows_exceptions(const Runtime* runtime);

} // namespace PluginRuntime
