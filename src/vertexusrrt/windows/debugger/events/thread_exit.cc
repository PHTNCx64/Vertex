//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    DWORD handle_exit_thread(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        release_thread_handle(event.dwThreadId);
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_thread_exited != nullptr)
            {
                const auto& cb = ctx.callbacks->value();
                ThreadEvent threadEvent{};
                threadEvent.threadId = event.dwThreadId;
                threadEvent.entryPoint = 0;
                threadEvent.stackBase = 0;
                threadEvent.exitCode = static_cast<std::int32_t>(event.u.ExitThread.dwExitCode);
                cb.on_thread_exited(&threadEvent, cb.user_data);
            }
        }
        return DBG_CONTINUE;
    }
}
