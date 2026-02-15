//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugloopcontext.hh>

namespace debugger
{
    namespace
    {
        BreakpointManager g_breakpointManager{};
    }

    BreakpointManager& get_breakpoint_manager()
    {
        return g_breakpointManager;
    }
}
