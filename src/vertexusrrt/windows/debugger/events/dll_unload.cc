//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    DWORD handle_unload_dll(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_module_unloaded != nullptr)
            {
                const auto& cb = ctx.callbacks->value();
                ModuleEvent moduleEvent{};
                moduleEvent.baseAddress = reinterpret_cast<std::uint64_t>(event.u.UnloadDll.lpBaseOfDll);
                moduleEvent.isMainModule = 0;
                cb.on_module_unloaded(&moduleEvent, cb.user_data);
            }
        }
        return DBG_CONTINUE;
    }
}
