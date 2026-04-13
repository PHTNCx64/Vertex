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
    extern PluginRuntime::DisasmMode get_disasm_mode(PluginRuntime::ProcessArchitecture arch);
    extern StatusCode register_architecture_metadata(PluginRuntime::ProcessArchitecture arch);
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

StatusCode handle_process_opened(const ProcessEventData* eventData)
{
    if (!g_pluginRuntime)
    {
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    std::ignore = eventData;

    const native_handle handle = get_native_handle();
    if (handle == INVALID_HANDLE_VALUE)
    {
        g_pluginRuntime->vertex_log_error("Process opened but no valid handle");
        return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
    }

    const auto arch = map_architecture(get_process_architecture());
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
