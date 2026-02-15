//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    DWORD handle_exit_process(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        const std::uint32_t processId = ctx.attachedProcessId->load(std::memory_order_acquire);
        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        ctx.currentState->store(VERTEX_DBG_STATE_DETACHED, std::memory_order_release);
        ctx.attachedProcessId->store(0, std::memory_order_release);
        ctx.currentThreadId->store(0, std::memory_order_release);
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value())
            {
                const auto& cb = ctx.callbacks->value();
                if (cb.on_process_exited != nullptr)
                {
                    cb.on_process_exited(static_cast<std::int32_t>(event.u.ExitProcess.dwExitCode), cb.user_data);
                }
                if (cb.on_state_changed != nullptr)
                {
                    cb.on_state_changed(oldState, VERTEX_DBG_STATE_DETACHED, cb.user_data);
                }
                if (cb.on_detached != nullptr)
                {
                    cb.on_detached(processId, cb.user_data);
                }
            }
        }
        ctx.stopRequested->store(true, std::memory_order_release);
        return DBG_CONTINUE;
    }
}
