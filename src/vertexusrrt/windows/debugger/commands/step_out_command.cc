//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    DWORD process_step_out_command(TickState& state, const DWORD threadId)
    {
        const auto stepOutTarget = get_step_out_target(threadId, state.isWow64);

        if (stepOutTarget.has_value())
        {
            if (!set_temp_breakpoint(stepOutTarget.value()))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            return DBG_CONTINUE;
        }

        if (!set_trap_flag(threadId, state.isWow64, true))
        {
            return DBG_EXCEPTION_NOT_HANDLED;
        }

        return DBG_CONTINUE;
    }
}
