//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    DWORD handle_output_string(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_output_string != nullptr)
            {
                const auto& cb = ctx.callbacks->value();
                OutputStringEvent outputEvent{};
                outputEvent.threadId = event.dwThreadId;
                cb.on_output_string(&outputEvent, cb.user_data);
            }
        }
        return DBG_CONTINUE;
    }
}
