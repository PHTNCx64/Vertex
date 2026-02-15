//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    DWORD process_step_into_command(const DebugLoopContext& ctx, const DWORD threadId, const bool isWow64)
    {
        if (!set_trap_flag(threadId, isWow64, true))
        {
            return DBG_EXCEPTION_NOT_HANDLED;
        }

        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        ctx.currentState->store(VERTEX_DBG_STATE_STEPPING, std::memory_order_release);

        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_state_changed != nullptr)
            {
                ctx.callbacks->value().on_state_changed(oldState, VERTEX_DBG_STATE_STEPPING, ctx.callbacks->value().user_data);
            }
        }

        return DBG_CONTINUE;
    }
}
