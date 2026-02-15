//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

#include <format>

namespace debugger
{
    DWORD handle_exception_general(const DebugLoopContext& ctx, const DEBUG_EVENT& event,
                                    const std::stop_token& stopToken, bool& shouldWaitForCommand)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;
        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        const bool isWow64 = ctx.isWow64Process->load(std::memory_order_acquire);
        const auto exceptionAddress = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);

        const auto logMsg = std::format("[Vertex] GeneralException: code=0x{:08X} addr=0x{:016X} firstChance={}\n", ExceptionRecord.ExceptionCode, exceptionAddress, dwFirstChance);
        OutputDebugStringA(logMsg.c_str());

        if (dwFirstChance != 0)
        {
            return DBG_EXCEPTION_NOT_HANDLED;
        }

        ctx.currentState->store(VERTEX_DBG_STATE_EXCEPTION, std::memory_order_release);

        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value())
            {
                const auto& cb = ctx.callbacks->value();

                if (cb.on_exception != nullptr)
                {
                    DebugEvent debugEvent{};
                    debugEvent.type = VERTEX_DBG_EVENT_EXCEPTION;
                    debugEvent.threadId = event.dwThreadId;
                    debugEvent.address = exceptionAddress;
                    debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                    debugEvent.firstChance = 0;

                    cb.on_exception(&debugEvent, cb.user_data);
                }

                if (cb.on_state_changed != nullptr)
                {
                    cb.on_state_changed(oldState, VERTEX_DBG_STATE_EXCEPTION, cb.user_data);
                }
            }
        }

        shouldWaitForCommand = true;
        const auto cmd = wait_for_command(ctx, stopToken);

        switch (cmd)
        {
        case DebugCommand::Continue:
        {
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

            if (ctx.passException->load(std::memory_order_acquire))
            {
                ctx.passException->store(false, std::memory_order_release);
                return DBG_CONTINUE;
            }
            return DBG_EXCEPTION_NOT_HANDLED;
        }
        case DebugCommand::StepInto:
            return process_step_into_command(ctx, event.dwThreadId, isWow64);
        case DebugCommand::StepOver:
            return process_step_over_command(ctx, event.dwThreadId, isWow64);
        case DebugCommand::StepOut:
            return process_step_out_command(ctx, event.dwThreadId, isWow64);
        case DebugCommand::RunToAddress:
            return process_run_to_address_command(ctx);
        default:
            return DBG_EXCEPTION_NOT_HANDLED;
        }
    }
}
