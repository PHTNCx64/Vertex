//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/arch_registers.hh>
#include <vertexusrrt/disassembler.hh>

extern native_handle& get_native_handle();
extern Runtime* g_pluginRuntime;
extern ProcessArchitecture get_process_architecture();

namespace EventInternal
{
    extern StatusCode register_for_debugging(PluginRuntime::ProcessArchitecture arch);
}

namespace
{
    [[nodiscard]] PluginRuntime::ProcessArchitecture map_architecture(const ProcessArchitecture arch)
    {
        switch (arch)
        {
        case ProcessArchitecture::X86:
            return PluginRuntime::ProcessArchitecture::X86;
        case ProcessArchitecture::X86_64:
            return PluginRuntime::ProcessArchitecture::X86_64;
        case ProcessArchitecture::ARM64:
            return PluginRuntime::ProcessArchitecture::ARM64;
        default:
            return PluginRuntime::ProcessArchitecture::Unknown;
        }
    }
}

StatusCode handle_debugger_attached([[maybe_unused]] const ProcessEventData* eventData)
{
    if (!g_pluginRuntime)
    {
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    const native_handle handle = get_native_handle();
    if (handle == INVALID_HANDLE_VALUE)
    {
        g_pluginRuntime->vertex_log_error("Debugger attached but no valid process handle");
        return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
    }

    const auto arch = map_architecture(get_process_architecture());
    g_pluginRuntime->vertex_log_info("Debugger attached - architecture: %s",
        PluginRuntime::get_architecture_name(arch));

    return EventInternal::register_for_debugging(arch);
}
