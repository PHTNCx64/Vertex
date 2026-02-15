//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    DWORD handle_load_dll(const DebugLoopContext& ctx, const DEBUG_EVENT& event)
    {
        {
            std::scoped_lock lock{*ctx.callbackMutex};
            if (ctx.callbacks->has_value() && ctx.callbacks->value().on_module_loaded != nullptr)
            {
                const auto& cb = ctx.callbacks->value();
                const auto& info = event.u.LoadDll;
                ModuleEvent moduleEvent{};
                moduleEvent.baseAddress = reinterpret_cast<std::uint64_t>(info.lpBaseOfDll);
                moduleEvent.isMainModule = 0;
                cb.on_module_loaded(&moduleEvent, cb.user_data);
            }
        }
        if (event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
        }
        return DBG_CONTINUE;
    }
}
