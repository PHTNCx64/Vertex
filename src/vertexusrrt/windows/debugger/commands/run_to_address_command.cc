//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    DWORD process_run_to_address_command(const DebugLoopContext& ctx)
    {
        const std::uint64_t targetAddr = ctx.targetAddress->load(std::memory_order_acquire);

        if (targetAddr != 0)
        {
            if (!set_temp_breakpoint(targetAddr))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }
        }

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

        return DBG_CONTINUE;
    }
}
