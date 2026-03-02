//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <Windows.h>

namespace debugger
{
    TickEventResult handle_create_thread(TickState& state, const DEBUG_EVENT& event)
    {
        cache_thread_handle(event.dwThreadId);
        apply_all_hw_breakpoints_to_thread(event.dwThreadId);

        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value() && state.callbacks.value().on_thread_created != nullptr)
            {
                const auto& cb = state.callbacks.value();
                const auto& info = event.u.CreateThread;
                ThreadEvent threadEvent{};
                threadEvent.threadId = event.dwThreadId;
                threadEvent.entryPoint = reinterpret_cast<std::uint64_t>(info.lpStartAddress);
                threadEvent.stackBase = 0;
                threadEvent.exitCode = 0;
                cb.on_thread_created(&threadEvent, cb.user_data);
            }
        }
        return TickEventResult{.continueStatus = DBG_CONTINUE};
    }
}
