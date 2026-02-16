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
    extern PluginRuntime::DisasmMode get_disasm_mode(PluginRuntime::ProcessArchitecture arch);
    extern StatusCode register_architecture_metadata(PluginRuntime::ProcessArchitecture arch);
}

StatusCode handle_process_opened(const ProcessEventData* eventData)
{
    if (!g_pluginRuntime)
    {
        return STATUS_ERROR_GENERAL;
    }

    HANDLE processHandle = (eventData && eventData->processHandle)
        ? static_cast<HANDLE>(eventData->processHandle)
        : get_native_handle();

    if (!processHandle || processHandle == INVALID_HANDLE_VALUE)
    {
        g_pluginRuntime->vertex_log_error("Process opened but no valid handle");
        return STATUS_ERROR_PROCESS_NOT_FOUND;
    }

    const auto arch = PluginRuntime::detect_process_architecture(processHandle);
    g_pluginRuntime->vertex_log_info("Process opened - detected architecture: %s",
        PluginRuntime::get_architecture_name(arch));

    const StatusCode status = EventInternal::register_architecture_metadata(arch);

    if (status == STATUS_OK)
    {
        if (PluginRuntime::init_disassembler(EventInternal::get_disasm_mode(arch)) != STATUS_OK)
        {
            g_pluginRuntime->vertex_log_warn("Failed to initialize disassembler for process");
        }
        else
        {
            g_pluginRuntime->vertex_log_info("Disassembler initialized for %s", PluginRuntime::get_architecture_name(arch));
        }
    }

    return status;
}
