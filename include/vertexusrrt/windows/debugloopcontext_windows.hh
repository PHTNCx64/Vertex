//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/debugloopcontext_common.hh>

#include <windows.h>
#ifdef STATUS_TIMEOUT
#undef STATUS_TIMEOUT
#endif

namespace debugger
{
    using dbg_thread_id_t = DWORD;
    using dbg_process_id_t = DWORD;
    using dbg_continue_status_t = DWORD;
    constexpr dbg_continue_status_t DBG_STATUS_CONTINUE = DBG_CONTINUE;
    constexpr dbg_continue_status_t DBG_STATUS_NOT_HANDLED = DBG_EXCEPTION_NOT_HANDLED;

    struct TickEventResult final
    {
        dbg_continue_status_t continueStatus{DBG_STATUS_CONTINUE};
        bool shouldPause{false};
        bool processExited{false};
        bool isInternal{false};
        PauseReason pauseReason{PauseReason::None};
        std::uint64_t pauseAddress{};
        std::uint32_t pauseBreakpointId{};
        std::uint32_t pauseWatchpointId{};
    };

    struct TickState final
    {
        std::uint32_t attachedProcessId{};
        std::uint32_t currentThreadId{};
        bool initialBreakpointPending{false};
        bool pauseRequested{false};
        bool stopRequested{false};
        bool passException{false};

        DebugCommand pendingCommand{DebugCommand::None};
        std::uint64_t pendingTargetAddress{};

        bool hasPendingEvent{false};
        DEBUG_EVENT pendingEvent{};
        dbg_continue_status_t pendingContinueStatus{DBG_STATUS_CONTINUE};
        PauseReason lastPauseReason{PauseReason::None};
        std::uint64_t lastPauseAddress{};
        std::uint32_t lastPauseBreakpointId{};
        std::uint32_t lastPauseWatchpointId{};

        std::optional<::DebuggerCallbacks> callbacks{};
        std::mutex callbackMutex{};
    };
}
