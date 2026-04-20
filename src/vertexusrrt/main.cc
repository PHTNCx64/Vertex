//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <sdk/api.h>

#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/disassembler.hh>
#include <vertexusrrt/watchpoint_throttle.hh>

#if defined(__linux__)
#include <vertexusrrt/linux/debugger_options.hh>
#include <vertexusrrt/linux/lldb_backend.hh>
#endif

#include <string>

native_handle& get_native_handle();
extern "C" void clear_module_cache();
Runtime* g_pluginRuntime = nullptr;

extern StatusCode handle_process_opened(const ProcessEventData* eventData);
extern StatusCode handle_debugger_attached(const ProcessEventData* eventData);

namespace ProcessInternal
{
    StatusCode invalidate_handle();
}

namespace
{
    constexpr PluginInformation g_pluginInformation = {
        .pluginName = "Vertex User-Mode Runtime",
        .pluginVersion = "0.1",
        .pluginDescription = "Implements functionality using the host operating system's user-mode APIs",
        .pluginAuthor = "PHTNC",
        .apiVersion = VERTEX_TARGET_API_VERSION(VERTEX_MAJOR_API_VERSION, VERTEX_MINOR_API_VERSION, VERTEX_PATCH_API_VERSION),
        .featureCapability = VERTEX_FEATURE_RUN_MODE_STANDARD
    };
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_init(PluginInformation* pluginInfo, Runtime* runtime, [[maybe_unused]] bool singleThreadModeInit)
{
    if (!pluginInfo || !runtime)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    *pluginInfo = g_pluginInformation;
    g_pluginRuntime = runtime;

    const StatusCode disasmStatus = PluginRuntime::init_disassembler(PluginRuntime::DisasmMode::X86_64);
    if (disasmStatus != StatusCode::STATUS_OK)
    {
        const char* errMsg = PluginRuntime::get_last_disassembler_error();
        g_pluginRuntime->vertex_log_error("Failed to initialize disassembler (Capstone): %s", errMsg ? errMsg : "unknown error");
    }
    else
    {
        g_pluginRuntime->vertex_log_info("Disassembler (Capstone) initialized successfully.");
    }

#if defined(__linux__)
    debugger_options::register_ui_panel();
#endif

    debugger::register_watchpoint_throttle_ui();

    g_pluginRuntime->vertex_log_info("Vertex User-Mode Runtime initialized.");

    return StatusCode::STATUS_OK;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_exit()
{
    std::ignore = vertex_debugger_set_callbacks(nullptr);

    const auto detachStatus = vertex_debugger_detach();
    if (detachStatus != StatusCode::STATUS_OK &&
        detachStatus != StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED &&
        g_pluginRuntime)
    {
        g_pluginRuntime->vertex_log_warn("vertex_exit: debugger detach returned status %d", static_cast<int>(detachStatus));
    }

#if defined(__linux__)
    Debugger::terminate_lldb();
#endif

    clear_module_cache();
    PluginRuntime::cleanup_disassembler();

    if (g_pluginRuntime)
    {
        g_pluginRuntime->vertex_log_info("Vertex User-Mode Runtime shutting down.");
    }

    g_pluginRuntime = nullptr;
    return StatusCode::STATUS_OK;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_event(const Event event, const void* data)
{
    switch (event)
    {
        case VERTEX_PROCESS_OPENED:
            return handle_process_opened(static_cast<const ProcessEventData*>(data));

        case VERTEX_DEBUGGER_ATTACHED:
            return handle_debugger_attached(static_cast<const ProcessEventData*>(data));

        case VERTEX_PROCESS_CLOSED:
            clear_module_cache();
            std::ignore = ProcessInternal::invalidate_handle();
            if (g_pluginRuntime)
            {
                g_pluginRuntime->vertex_log_info("Process closed");
            }
            return StatusCode::STATUS_OK;

        case VERTEX_DEBUGGER_DETACHED:
            PluginRuntime::cleanup_disassembler();
            if (g_pluginRuntime)
            {
                g_pluginRuntime->vertex_log_info("Debugger detached - disassembler cleaned up");
            }
            return StatusCode::STATUS_OK;

        default:
            return StatusCode::STATUS_OK;
    }
}
