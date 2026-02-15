//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    DWORD handle_create_process(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        cache_thread_handle(event.dwThreadId);

        const std::uint32_t processId = event.dwProcessId;
        ctx.attachedProcessId->store(processId, std::memory_order_release);
        ctx.currentThreadId->store(event.dwThreadId, std::memory_order_release);
        const auto oldState = ctx.currentState->load(std::memory_order_acquire);
        ctx.currentState->store(VERTEX_DBG_STATE_ATTACHED, std::memory_order_release);
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value())
            {
                const auto& cb = ctx.callbacks->value();
                if (cb.on_attached != nullptr)
                {
                    cb.on_attached(processId, cb.user_data);
                }
                if (cb.on_state_changed != nullptr)
                {
                    cb.on_state_changed(oldState, VERTEX_DBG_STATE_ATTACHED, cb.user_data);
                }
                if (cb.on_module_loaded != nullptr)
                {
                    const auto& info = event.u.CreateProcessInfo;
                    ModuleEvent moduleEvent{};
                    moduleEvent.baseAddress = reinterpret_cast<std::uint64_t>(info.lpBaseOfImage);
                    moduleEvent.isMainModule = 1;
                    cb.on_module_loaded(&moduleEvent, cb.user_data);
                }
            }
        }
        ctx.currentState->store(VERTEX_DBG_STATE_RUNNING, std::memory_order_release);
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_state_changed != nullptr)
            {
                const auto& cb = ctx.callbacks->value();
                cb.on_state_changed(VERTEX_DBG_STATE_ATTACHED, VERTEX_DBG_STATE_RUNNING, cb.user_data);
            }
        }
        if (event.u.CreateProcessInfo.hFile != nullptr)
        {
            CloseHandle(event.u.CreateProcessInfo.hFile);
        }
        return DBG_CONTINUE;
    }
}
