//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
namespace debugger
{
    TickEventResult handle_unload_dll(TickState& state, const DEBUG_EVENT& event)
    {
        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value() && state.callbacks.value().on_module_unloaded != nullptr)
            {
                const auto& cb = state.callbacks.value();
                ModuleEvent moduleEvent{};
                moduleEvent.baseAddress = reinterpret_cast<std::uint64_t>(event.u.UnloadDll.lpBaseOfDll);
                moduleEvent.isMainModule = 0;
                cb.on_module_unloaded(&moduleEvent, cb.user_data);
            }
        }
        return TickEventResult{.continueStatus = DBG_CONTINUE};
    }
}
