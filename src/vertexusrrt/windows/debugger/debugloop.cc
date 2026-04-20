//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>
#include <vertexusrrt/watchpoint_throttle.hh>

#include <windows.h>

#include <format>
#include <mutex>

namespace
{
    debugger::TickEventResult handle_exception(debugger::TickState& state, const DEBUG_EVENT& event)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;

        const auto msg = std::format("[Vertex] Exception: code=0x{:08X} addr=0x{:016X} firstChance={} thread={}\n",
                               ExceptionRecord.ExceptionCode,
                               reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress),
                               dwFirstChance,
                               event.dwThreadId);
        OutputDebugStringA(msg.c_str());

        if (ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT ||
            ExceptionRecord.ExceptionCode == debugger::STATUS_WX86_BREAKPOINT)
        {
            return debugger::handle_exception_breakpoint(state, event);
        }

        if (ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP ||
            ExceptionRecord.ExceptionCode == debugger::STATUS_WX86_SINGLE_STEP)
        {
            return debugger::handle_exception_single_step(state, event);
        }

        return debugger::handle_exception_general(state, event);
    }

    debugger::TickEventResult handle_debug_event(debugger::TickState& state, const DEBUG_EVENT& event)
    {
        switch (event.dwDebugEventCode)
        {
        case CREATE_PROCESS_DEBUG_EVENT:
            return debugger::handle_create_process(state, event);

        case EXIT_PROCESS_DEBUG_EVENT:
            return debugger::handle_exit_process(state, event);

        case CREATE_THREAD_DEBUG_EVENT:
            return debugger::handle_create_thread(state, event);

        case EXIT_THREAD_DEBUG_EVENT:
            return debugger::handle_exit_thread(state, event);

        case LOAD_DLL_DEBUG_EVENT:
            return debugger::handle_load_dll(state, event);

        case UNLOAD_DLL_DEBUG_EVENT:
            return debugger::handle_unload_dll(state, event);

        case OUTPUT_DEBUG_STRING_EVENT:
            return debugger::handle_output_string(state, event);

        case EXCEPTION_DEBUG_EVENT:
            return handle_exception(state, event);

        default:
            return debugger::TickEventResult{.continueStatus = DBG_CONTINUE};
        }
    }
}

namespace debugger
{
    DWORD execute_resume(TickState& state)
    {
        const DWORD threadId = state.pendingEvent.dwThreadId;
        const DebugCommand cmd = state.pendingCommand;

        switch (state.lastPauseReason)
        {
        case PauseReason::UserBreakpoint:
        {
            if (!set_trap_flag(threadId, true))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            std::optional<std::uint64_t> precomputed{};
            if (cmd == DebugCommand::StepOver)
            {
                precomputed = get_step_over_target(threadId);
            }
            else if (cmd == DebugCommand::StepOut)
            {
                precomputed = get_step_out_target(threadId);
            }

            set_breakpoint_step_over(state.lastPauseAddress, threadId, cmd, precomputed);
            return DBG_CONTINUE;
        }
        case PauseReason::Watchpoint:
        {
            if (cmd == DebugCommand::Continue)
            {
                WatchpointData wpData{};
                if (get_watchpoint_info(state.lastPauseWatchpointId, &wpData) == STATUS_OK)
                {
                    std::ignore = temporarily_disable_watchpoint_on_all_threads(state.lastPauseWatchpointId);
                    set_watchpoint_step_over(state.lastPauseWatchpointId, wpData.registerIndex, threadId);

                    if (!set_trap_flag(threadId, true))
                    {
                        return DBG_EXCEPTION_NOT_HANDLED;
                    }
                    return DBG_CONTINUE;
                }
            }
            [[fallthrough]];
        }
        default:
        {
            switch (cmd)
            {
            case DebugCommand::Continue:
                return process_continue_command(state);
            case DebugCommand::StepInto:
                return process_step_into_command(state, threadId);
            case DebugCommand::StepOver:
                return process_step_over_command(state, threadId);
            case DebugCommand::StepOut:
                return process_step_out_command(state, threadId);
            case DebugCommand::RunToAddress:
                return process_run_to_address_command(state);
            default:
                return DBG_CONTINUE;
            }
        }
        }
    }

    StatusCode tick_debug_loop(TickState& state, const std::uint32_t timeoutMs)
    {
        if (state.hasPendingEvent && state.pendingCommand != DebugCommand::None)
        {
            const DWORD continueStatus = execute_resume(state);

            auto contMsg = std::format("[Vertex] ContinueDebugEvent(resume): pid={} tid={} status=0x{:08X}\n",
                                       state.pendingEvent.dwProcessId, state.pendingEvent.dwThreadId, continueStatus);
            OutputDebugStringA(contMsg.c_str());

            if (!ContinueDebugEvent(state.pendingEvent.dwProcessId, state.pendingEvent.dwThreadId, continueStatus))
            {
                auto errMsg = std::format("[Vertex] ContinueDebugEvent FAILED: error={}\n", GetLastError());
                OutputDebugStringA(errMsg.c_str());
                return STATUS_DEBUG_TICK_ERROR;
            }

            state.hasPendingEvent = false;
            state.pendingCommand = DebugCommand::None;
            state.lastPauseReason = PauseReason::None;
            state.lastPauseAddress = 0;
            state.lastPauseBreakpointId = 0;
            state.lastPauseWatchpointId = 0;
        }

        if (state.hasPendingEvent)
        {
            return STATUS_DEBUG_TICK_PAUSED;
        }

        constexpr DWORD INTERNAL_TIMEOUT_MS = 1;

        for (;;)
        {
            DEBUG_EVENT debugEvent{};

            if (!WaitForDebugEventEx(&debugEvent, timeoutMs))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_SEM_TIMEOUT)
                {
                    return STATUS_DEBUG_TICK_NO_EVENT;
                }
                return STATUS_DEBUG_TICK_ERROR;
            }

            const auto [continueStatus, shouldPause, processExited, isInternal, pauseReason, pauseAddress, pauseBreakpointId, pauseWatchpointId] = handle_debug_event(state, debugEvent);

            if (shouldPause)
            {
                state.hasPendingEvent = true;
                state.pendingEvent = debugEvent;
                state.pendingContinueStatus = continueStatus;
                state.lastPauseReason = pauseReason;
                state.lastPauseAddress = pauseAddress;
                state.lastPauseBreakpointId = pauseBreakpointId;
                state.lastPauseWatchpointId = pauseWatchpointId;
                return STATUS_DEBUG_TICK_PAUSED;
            }

            if (processExited)
            {
                auto contMsg = std::format("[Vertex] ContinueDebugEvent(exit): pid={} tid={}\n",
                                           debugEvent.dwProcessId, debugEvent.dwThreadId);
                OutputDebugStringA(contMsg.c_str());

                ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);

                std::ignore = remove_temp_breakpoint();
                reset_breakpoint_manager();
                clear_all_breakpoint_step_overs();
                clear_all_watchpoint_step_overs();
                clear_watchpoint_throttle_state();
                clear_thread_handle_cache();

                return STATUS_DEBUG_TICK_PROCESS_EXITED;
            }

            auto contMsg = std::format("[Vertex] ContinueDebugEvent: pid={} tid={} status=0x{:08X}\n",
                                       debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);
            OutputDebugStringA(contMsg.c_str());

            if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus))
            {
                const auto errMsg = std::format("[Vertex] ContinueDebugEvent FAILED: error={}\n", GetLastError());
                OutputDebugStringA(errMsg.c_str());
                return STATUS_DEBUG_TICK_ERROR;
            }

            if (isInternal)
            {
                if (!WaitForDebugEventEx(&debugEvent, INTERNAL_TIMEOUT_MS))
                {
                    const DWORD error = GetLastError();
                    if (error == ERROR_SEM_TIMEOUT)
                    {
                        return STATUS_DEBUG_TICK_NO_EVENT;
                    }
                    return STATUS_DEBUG_TICK_ERROR;
                }

                const auto internalResult = handle_debug_event(state, debugEvent);

                if (internalResult.shouldPause)
                {
                    state.hasPendingEvent = true;
                    state.pendingEvent = debugEvent;
                    state.pendingContinueStatus = internalResult.continueStatus;
                    state.lastPauseReason = internalResult.pauseReason;
                    state.lastPauseAddress = internalResult.pauseAddress;
                    state.lastPauseBreakpointId = internalResult.pauseBreakpointId;
                    state.lastPauseWatchpointId = internalResult.pauseWatchpointId;
                    return STATUS_DEBUG_TICK_PAUSED;
                }

                if (internalResult.processExited)
                {
                    ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, internalResult.continueStatus);
                    std::ignore = remove_temp_breakpoint();
                    reset_breakpoint_manager();
                    clear_all_breakpoint_step_overs();
                    clear_all_watchpoint_step_overs();
                    clear_thread_handle_cache();
                    return STATUS_DEBUG_TICK_PROCESS_EXITED;
                }

                if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, internalResult.continueStatus))
                {
                    return STATUS_DEBUG_TICK_ERROR;
                }
            }

            return STATUS_DEBUG_TICK_PROCESSED;
        }
    }
}
