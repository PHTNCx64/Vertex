//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

namespace Vertex::Event
{
    using EventId = int;

    static constexpr EventId PROCESS_OPEN_EVENT = 1;
    static constexpr EventId DATATYPE_EVENT = 2;
    static constexpr EventId VIEW_EVENT = 3;
    static constexpr EventId SETTINGS_CHANGED_EVENT = 4;
    static constexpr EventId VIEW_UPDATE_EVENT = 5;
    static constexpr EventId PROCESS_CLOSED_EVENT = 6;

    static constexpr EventId APPLICATION_SHUTDOWN_EVENT = 9;

    static constexpr EventId DEBUGGER_ATTACH_EVENT = 10;
    static constexpr EventId DEBUGGER_DETACH_EVENT = 11;
    static constexpr EventId DEBUGGER_BREAKPOINT_HIT_EVENT = 12;
    static constexpr EventId DEBUGGER_STEP_COMPLETE_EVENT = 13;
    static constexpr EventId DEBUGGER_MEMORY_CHANGED_EVENT = 14;
    static constexpr EventId DEBUGGER_REGISTER_CHANGED_EVENT = 15;
    static constexpr EventId DEBUGGER_STATE_CHANGED_EVENT = 16;
    static constexpr EventId DEBUGGER_MODULE_LOADED_EVENT = 17;
    static constexpr EventId DEBUGGER_MODULE_UNLOADED_EVENT = 18;
}
