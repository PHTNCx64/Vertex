//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

#include <format>

namespace debugger
{
    DWORD handle_exception_breakpoint(const DebugLoopContext& ctx, const DEBUG_EVENT& event,
                                       const std::stop_token& stopToken, bool& shouldWaitForCommand)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;
        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        const bool isWow64 = ctx.isWow64Process->load(std::memory_order_acquire);
        const auto exceptionAddress = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);

        auto logMsg = std::format("[Vertex] Breakpoint: addr=0x{:016X} oldState={}\n",
                                  exceptionAddress, static_cast<int>(oldState));
        OutputDebugStringA(logMsg.c_str());

        if (ctx.initialBreakpointPending->exchange(false, std::memory_order_acq_rel))
        {
            OutputDebugStringA("[Vertex] Initial attach breakpoint consumed\n");
            return DBG_CONTINUE;
        }

        if (is_temp_breakpoint_hit(exceptionAddress))
        {
            if (!remove_temp_breakpoint())
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            if (!decrement_instruction_pointer(event.dwThreadId, isWow64))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            ctx.currentState->store(VERTEX_DBG_STATE_PAUSED, std::memory_order_release);

            {
                std::scoped_lock lock{*ctx.callbackMutex};
                if (ctx.callbacks->has_value())
                {
                    const auto& cb = ctx.callbacks->value();

                    if (cb.on_single_step != nullptr)
                    {
                        DebugEvent debugEvent{};
                        debugEvent.type = VERTEX_DBG_EVENT_SINGLE_STEP;
                        debugEvent.threadId = event.dwThreadId;
                        debugEvent.address = exceptionAddress - 1;
                        debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                        debugEvent.firstChance = dwFirstChance != 0;

                        cb.on_single_step(&debugEvent, cb.user_data);
                    }

                    if (cb.on_state_changed != nullptr)
                    {
                        cb.on_state_changed(oldState, VERTEX_DBG_STATE_PAUSED, cb.user_data);
                    }
                }
            }

            shouldWaitForCommand = true;
            const auto cmd = wait_for_command(ctx, stopToken);

            switch (cmd)
            {
            case DebugCommand::Continue:
                return process_continue_command(ctx);
            case DebugCommand::StepInto:
                return process_step_into_command(ctx, event.dwThreadId, isWow64);
            case DebugCommand::StepOver:
                return process_step_over_command(ctx, event.dwThreadId, isWow64);
            case DebugCommand::StepOut:
                return process_step_out_command(ctx, event.dwThreadId, isWow64);
            case DebugCommand::RunToAddress:
                return process_run_to_address_command(ctx);
            default:
                return DBG_CONTINUE;
            }
        }

        std::uint32_t userBreakpointId = 0;
        const bool isUserBreakpoint = is_user_breakpoint_hit(exceptionAddress, &userBreakpointId);

        if (isUserBreakpoint)
        {
            const auto restoreResult = restore_breakpoint_byte(exceptionAddress);
            if (restoreResult != STATUS_OK)
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            if (!decrement_instruction_pointer(event.dwThreadId, isWow64))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            ctx.currentState->store(VERTEX_DBG_STATE_BREAKPOINT_HIT, std::memory_order_release);

            {
                std::scoped_lock lock{*ctx.callbackMutex};
                if (ctx.callbacks->has_value())
                {
                    const auto& cb = ctx.callbacks->value();

                    if (cb.on_breakpoint_hit != nullptr)
                    {
                        DebugEvent debugEvent{};
                        debugEvent.type = VERTEX_DBG_EVENT_BREAKPOINT;
                        debugEvent.threadId = event.dwThreadId;
                        debugEvent.address = exceptionAddress - 1;
                        debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                        debugEvent.firstChance = dwFirstChance ? 1 : 0;
                        debugEvent.breakpointId = userBreakpointId;

                        cb.on_breakpoint_hit(&debugEvent, cb.user_data);
                    }

                    if (cb.on_state_changed != nullptr)
                    {
                        cb.on_state_changed(oldState, VERTEX_DBG_STATE_BREAKPOINT_HIT, cb.user_data);
                    }
                }
            }
        }
        else
        {
            const bool isPauseRequested = ctx.pauseRequested->exchange(false, std::memory_order_acq_rel);

            if (!isPauseRequested)
            {
                OutputDebugStringA("[Vertex] Breakpoint: not user BP, not pause requested, returning DBG_CONTINUE\n");
                return DBG_CONTINUE;
            }

            ctx.currentState->store(VERTEX_DBG_STATE_PAUSED, std::memory_order_release);

            {
                std::scoped_lock lock{*ctx.callbackMutex};
                if (ctx.callbacks->has_value())
                {
                    const auto& cb = ctx.callbacks->value();

                    if (cb.on_single_step != nullptr)
                    {
                        DebugEvent debugEvent{};
                        debugEvent.type = VERTEX_DBG_EVENT_SINGLE_STEP;
                        debugEvent.threadId = event.dwThreadId;
                        debugEvent.address = exceptionAddress;
                        debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                        debugEvent.firstChance = dwFirstChance ? 1 : 0;

                        cb.on_single_step(&debugEvent, cb.user_data);
                    }

                    if (cb.on_state_changed != nullptr)
                    {
                        cb.on_state_changed(oldState, VERTEX_DBG_STATE_PAUSED, cb.user_data);
                    }
                }
            }
        }

        shouldWaitForCommand = true;
        const auto cmd = wait_for_command(ctx, stopToken);

        if (isUserBreakpoint && cmd == DebugCommand::Continue)
        {
            set_breakpoint_step_over(exceptionAddress - 1);

            if (!set_trap_flag(event.dwThreadId, isWow64, true))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            const auto prevState = ctx.currentState->load(std::memory_order_acquire);
            ctx.currentState->store(VERTEX_DBG_STATE_RUNNING, std::memory_order_release);

            {
                std::scoped_lock lock{*ctx.callbackMutex};
                if (ctx.callbacks->has_value() && ctx.callbacks->value().on_state_changed != nullptr)
                {
                    ctx.callbacks->value().on_state_changed(prevState, VERTEX_DBG_STATE_RUNNING, ctx.callbacks->value().user_data);
                }
            }

            return DBG_CONTINUE;
        }

        switch (cmd)
        {
        case DebugCommand::Continue:
            return process_continue_command(ctx);
        case DebugCommand::StepInto:
            if (isUserBreakpoint)
            {
                set_breakpoint_step_over(exceptionAddress - 1);
            }
            return process_step_into_command(ctx, event.dwThreadId, isWow64);
        case DebugCommand::StepOver:
            if (isUserBreakpoint)
            {
                set_breakpoint_step_over(exceptionAddress - 1);
            }
            return process_step_over_command(ctx, event.dwThreadId, isWow64);
        case DebugCommand::StepOut:
            if (isUserBreakpoint)
            {
                set_breakpoint_step_over(exceptionAddress - 1);
            }
            return process_step_out_command(ctx, event.dwThreadId, isWow64);
        case DebugCommand::RunToAddress:
            if (isUserBreakpoint)
            {
                set_breakpoint_step_over(exceptionAddress - 1);

                if (!set_trap_flag(event.dwThreadId, isWow64, true))
                {
                    return DBG_EXCEPTION_NOT_HANDLED;
                }

                const auto prevState = ctx.currentState->load(std::memory_order_acquire);
                ctx.currentState->store(VERTEX_DBG_STATE_RUNNING, std::memory_order_release);

                {
                    std::scoped_lock lock{*ctx.callbackMutex};
                    if (ctx.callbacks->has_value() && ctx.callbacks->value().on_state_changed != nullptr)
                    {
                        ctx.callbacks->value().on_state_changed(prevState, VERTEX_DBG_STATE_RUNNING,
                                                                ctx.callbacks->value().user_data);
                    }
                }

                return DBG_CONTINUE;
            }
            return process_run_to_address_command(ctx);
        default:
            return DBG_CONTINUE;
        }
    }
}
