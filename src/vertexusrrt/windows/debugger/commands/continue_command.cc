//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    DWORD process_continue_command(const DebugLoopContext& ctx)
    {
        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        ctx.currentState->store(VERTEX_DBG_STATE_RUNNING, std::memory_order_release);

        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_state_changed != nullptr)
            {
                ctx.callbacks->value().on_state_changed(oldState, VERTEX_DBG_STATE_RUNNING,
                                                        ctx.callbacks->value().user_data);
            }
        }

        if (ctx.passException->load(std::memory_order_acquire))
        {
            ctx.passException->store(false, std::memory_order_release);
            return DBG_EXCEPTION_NOT_HANDLED;
        }
        return DBG_CONTINUE;
    }
}
