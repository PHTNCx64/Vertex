//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>

#include <Windows.h>

namespace debugger
{
    DWORD handle_create_thread(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        cache_thread_handle(event.dwThreadId);
        apply_all_hw_breakpoints_to_thread(event.dwThreadId);

        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_thread_created != nullptr)
            {
                const auto& cb = ctx.callbacks->value();
                const auto& info = event.u.CreateThread;
                ThreadEvent threadEvent{};
                threadEvent.threadId = event.dwThreadId;
                threadEvent.entryPoint = reinterpret_cast<std::uint64_t>(info.lpStartAddress);
                threadEvent.stackBase = reinterpret_cast<std::uint64_t>(info.lpThreadLocalBase);
                threadEvent.exitCode = 0;
                cb.on_thread_created(&threadEvent, cb.user_data);
            }
        }
        return DBG_CONTINUE;
    }
}
