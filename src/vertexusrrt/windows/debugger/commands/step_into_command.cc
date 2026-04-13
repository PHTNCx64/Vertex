//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <windows.h>

namespace debugger
{
    DWORD process_step_into_command(TickState& state, const DWORD threadId)
    {
        if (!set_trap_flag(threadId, true))
        {
            return DBG_EXCEPTION_NOT_HANDLED;
        }

        return DBG_CONTINUE;
    }
}
