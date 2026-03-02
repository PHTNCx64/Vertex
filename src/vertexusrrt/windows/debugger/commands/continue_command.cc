//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    DWORD process_continue_command(TickState& state)
    {
        if (state.passException)
        {
            state.passException = false;
            return DBG_EXCEPTION_NOT_HANDLED;
        }
        return DBG_CONTINUE;
    }
}
