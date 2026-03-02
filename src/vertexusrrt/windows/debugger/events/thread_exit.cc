//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    TickEventResult handle_exit_thread(TickState& state, const DEBUG_EVENT& event)
    {
        release_thread_handle(event.dwThreadId);
        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value() && state.callbacks.value().on_thread_exited != nullptr)
            {
                const auto& cb = state.callbacks.value();
                ThreadEvent threadEvent{};
                threadEvent.threadId = event.dwThreadId;
                threadEvent.entryPoint = 0;
                threadEvent.stackBase = 0;
                threadEvent.exitCode = static_cast<std::int32_t>(event.u.ExitThread.dwExitCode);
                cb.on_thread_exited(&threadEvent, cb.user_data);
            }
        }
        return TickEventResult{.continueStatus = DBG_CONTINUE};
    }
}
