//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

#include <format>

namespace debugger
{
    DWORD handle_exception_single_step(const DebugLoopContext& ctx, const DEBUG_EVENT& event,
                                        const std::stop_token& stopToken, bool& shouldWaitForCommand)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;
        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        const bool isWow64 = ctx.isWow64Process->load(std::memory_order_acquire);
        const auto exceptionAddress = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);

        auto logMsg = std::format("[Vertex] SingleStep: addr=0x{:016X} oldState={}\n",
                                  exceptionAddress, static_cast<int>(oldState));
        OutputDebugStringA(logMsg.c_str());

        std::uint64_t bpAddress{};
        if (is_stepping_over_breakpoint(&bpAddress))
        {
            clear_breakpoint_step_over();
            std::ignore = reapply_breakpoint_byte(bpAddress);

            if (is_temp_breakpoint_hit(exceptionAddress))
            {
                if (!remove_temp_breakpoint())
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
                            debugEvent.address = exceptionAddress;
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

        std::uint32_t watchpointId{};
        if (is_stepping_over_watchpoint(event.dwThreadId, &watchpointId))
        {
            clear_watchpoint_step_over(event.dwThreadId);
            std::ignore = re_enable_watchpoint_on_all_threads(watchpointId);

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
}
