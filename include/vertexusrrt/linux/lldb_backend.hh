//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <sdk/api.h>

#include <lldb/API/LLDB.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Debugger
{
    struct BreakpointEntry final
    {
        lldb::break_id_t lldbId{};
        std::uint64_t address{};
        BreakpointType type{};
        bool isHardwareData{false};
        BreakpointCondition condition{};
        BreakpointAction action{};
    };

    struct WatchpointEntry final
    {
        lldb::watch_id_t lldbId{};
        std::uint64_t address{};
        std::uint32_t size{};
        WatchpointType type{};
    };

    struct LldbBackendState final
    {
        lldb::SBDebugger debugger{};
        lldb::SBTarget target{};
        lldb::SBProcess process{};
        lldb::SBListener listener{};

        std::optional<DebuggerCallbacks> callbacks{};
        std::mutex callbackMutex{};

        std::unordered_map<std::uint32_t, BreakpointEntry> breakpoints{};
        std::uint32_t nextBreakpointId{1};

        std::unordered_map<std::uint32_t, WatchpointEntry> watchpoints{};
        std::uint32_t nextWatchpointId{1};

        std::mutex breakpointMutex{};

        std::vector<BreakpointInfo> cachedBreakpointInfos{};
        std::vector<WatchpointInfo> cachedWatchpointInfos{};

        bool isWineProcess{false};

        std::atomic<bool> continueOnWatchpointHit{false};
    };

    [[nodiscard]] std::uint32_t find_vertex_breakpoint_id(const LldbBackendState& state, lldb::break_id_t lldbId);
    [[nodiscard]] std::uint32_t find_vertex_watchpoint_id(const LldbBackendState& state, lldb::watch_id_t lldbId);

    void initialize_lldb();
    void terminate_lldb();

    [[nodiscard]] LldbBackendState& get_backend_state();
}
