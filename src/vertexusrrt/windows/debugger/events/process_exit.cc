//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <windows.h>
namespace debugger
{
    TickEventResult handle_exit_process(TickState& state, const DEBUG_EVENT& event)
    {
        state.attachedProcessId = 0;
        state.currentThreadId = 0;
        state.stopRequested = true;
        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value())
            {
                const auto& cb = state.callbacks.value();
                if (cb.on_process_exited != nullptr)
                {
                    cb.on_process_exited(static_cast<std::int32_t>(event.u.ExitProcess.dwExitCode), cb.user_data);
                }
            }
        }
        return TickEventResult{.continueStatus = DBG_CONTINUE, .processExited = true};
    }
}
