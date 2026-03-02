//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugloopcontext.hh>

#include <mutex>

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

    void reset_breakpoint_manager()
    {
        std::scoped_lock lock{g_breakpointManager.mutex};
        g_breakpointManager.softwareBreakpoints.clear();
        g_breakpointManager.hardwareBreakpoints.clear();
        g_breakpointManager.watchpoints.clear();
        g_breakpointManager.nextBreakpointId.store(1, std::memory_order_relaxed);
        g_breakpointManager.nextWatchpointId.store(1, std::memory_order_relaxed);
        g_breakpointManager.hwRegisterUsed.fill(false);
    }
}
