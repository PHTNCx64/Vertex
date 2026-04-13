//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <windows.h>

namespace debugger
{
    DWORD process_step_over_command(TickState& state, const DWORD threadId)
    {
        const auto stepOverTarget = get_step_over_target(threadId);

        if (stepOverTarget.has_value())
        {
            if (!set_temp_breakpoint(stepOverTarget.value()))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }

            return DBG_CONTINUE;
        }

        if (!set_trap_flag(threadId, true))
        {
            return DBG_EXCEPTION_NOT_HANDLED;
        }

        return DBG_CONTINUE;
    }
}
