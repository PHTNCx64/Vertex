//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/process_internal.hh>

#include <sdk/log.h>

#include <array>
#include <filesystem>

extern ProcessArchitecture get_process_architecture();
extern Runtime* g_pluginRuntime;
extern std::optional<ProcessArchitecture> detect_dll_architecture(const std::filesystem::path& dllPath);
extern std::optional<void*> resolve_remote_export(std::string_view moduleName, std::string_view functionName);

extern StatusCode remote_thread_inject(const char* path);
extern StatusCode manual_map_inject(const char* path);

namespace
{
    constexpr std::array<InjectionMethod, 2> g_injectionMethods{{
        {
            "CreateRemoteThread Injection",
            "Basic injection that uses remote threads and LoadLibrary to inject a DLL into the target process.",
            remote_thread_inject
        },
        {
            "Manual Map Injection",
            "Advanced injection technique that manually maps a DLL into memory without using LoadLibrary. The DLL does not appear in the module list and bypasses standard loader mechanisms.",
            manual_map_inject
        }
    }};
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_injection_methods(VertexInjectionMethod** methods, std::uint32_t* count)
{
    *count = static_cast<std::uint32_t>(g_injectionMethods.size());
    if (methods)
    {
        *methods = const_cast<VertexInjectionMethod*>(g_injectionMethods.data());
    }
    return StatusCode::STATUS_OK;
}
