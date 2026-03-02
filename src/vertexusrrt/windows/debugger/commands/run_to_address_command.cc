//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    DWORD process_run_to_address_command(TickState& state)
    {
        const std::uint64_t targetAddr = state.pendingTargetAddress;
        state.pendingTargetAddress = 0;

        if (targetAddr != 0)
        {
            if (!set_temp_breakpoint(targetAddr))
            {
                return DBG_EXCEPTION_NOT_HANDLED;
            }
        }

        return DBG_CONTINUE;
    }
}
