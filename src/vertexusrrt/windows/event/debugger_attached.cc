//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/arch_registers.hh>
#include <vertexusrrt/disassembler.hh>

#include <Windows.h>

extern native_handle& get_native_handle();
extern Runtime* g_pluginRuntime;

namespace EventInternal
{
    extern StatusCode register_for_debugging(PluginRuntime::ProcessArchitecture arch);
}

StatusCode handle_debugger_attached(const ProcessEventData* eventData)
{
    if (!g_pluginRuntime)
    {
        return STATUS_ERROR_GENERAL;
    }

    const HANDLE processHandle = (eventData && eventData->processHandle)
        ? static_cast<HANDLE>(eventData->processHandle)
        : get_native_handle();

    if (!processHandle || processHandle == INVALID_HANDLE_VALUE)
    {
        g_pluginRuntime->vertex_log_error("Debugger attached but no valid process handle");
        return STATUS_ERROR_PROCESS_NOT_FOUND;
    }

    const auto arch = PluginRuntime::detect_process_architecture(processHandle);
    g_pluginRuntime->vertex_log_info("Debugger attached - architecture: %s",
        PluginRuntime::get_architecture_name(arch));

    return EventInternal::register_for_debugging(arch);
}
