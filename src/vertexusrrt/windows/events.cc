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

namespace
{
    bool g_architecture_registered = false;

    PluginRuntime::DisasmMode get_disasm_mode(PluginRuntime::ProcessArchitecture arch)
    {
        switch (arch)
        {
            case PluginRuntime::ProcessArchitecture::X86:
            case PluginRuntime::ProcessArchitecture::ARM64_X86:
                return PluginRuntime::DisasmMode::X86_32;
            case PluginRuntime::ProcessArchitecture::ARM64:
                return PluginRuntime::DisasmMode::ARM64;
            case PluginRuntime::ProcessArchitecture::X86_64:
            default:
                return PluginRuntime::DisasmMode::X86_64;
        }
    }

    StatusCode register_architecture_metadata(PluginRuntime::ProcessArchitecture arch)
    {
        if (g_architecture_registered)
        {
            g_pluginRuntime->vertex_clear_registry();
            g_architecture_registered = false;
        }

        g_pluginRuntime->vertex_log_info("Registering %s architecture metadata",
            PluginRuntime::get_architecture_name(arch));

        StatusCode status = PluginRuntime::register_architecture(g_pluginRuntime, arch);
        if (status == STATUS_OK)
        {
            g_architecture_registered = true;
        }
        return status;
    }

    StatusCode register_for_debugging(const PluginRuntime::ProcessArchitecture arch)
    {
        const StatusCode status = register_architecture_metadata(arch);
        if (status == STATUS_OK)
        {
            if (PluginRuntime::init_disassembler(get_disasm_mode(arch)) != STATUS_OK)
            {
                g_pluginRuntime->vertex_log_warn("Failed to initialize disassembler");
            }
            else
            {
                g_pluginRuntime->vertex_log_info("Disassembler initialized for %s",
                    PluginRuntime::get_architecture_name(arch));
            }
        }
        return status;
    }
}

StatusCode handle_process_opened(const ProcessEventData* eventData)
{
    if (!g_pluginRuntime)
        return STATUS_ERROR_GENERAL;

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

    StatusCode status = register_architecture_metadata(arch);

    if (status == STATUS_OK)
    {
        if (PluginRuntime::init_disassembler(get_disasm_mode(arch)) != STATUS_OK)
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

StatusCode handle_debugger_attached(const ProcessEventData* eventData)
{
    if (!g_pluginRuntime)
        return STATUS_ERROR_GENERAL;

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

    return register_for_debugging(arch);
}
