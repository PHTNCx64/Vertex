//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/debuggertypes.hh>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

namespace Vertex::Debugger
{
    enum class EngineEventKind : std::uint32_t
    {
        None            = 0,
        StateChanged    = 1u << 0,
        BreakpointHit   = 1u << 1,
        WatchpointHit   = 1u << 2,
        ExceptionRaised = 1u << 3,
        ThreadStarted   = 1u << 4,
        ThreadExited    = 1u << 5,
        ModuleLoaded    = 1u << 6,
        ModuleUnloaded  = 1u << 7,
        OutputLog       = 1u << 8,
    };

    using EngineEventKindMask = std::underlying_type_t<EngineEventKind>;

    [[nodiscard]] constexpr EngineEventKind operator|(EngineEventKind lhs, EngineEventKind rhs) noexcept
    {
        return static_cast<EngineEventKind>(
            static_cast<EngineEventKindMask>(lhs) | static_cast<EngineEventKindMask>(rhs));
    }

    [[nodiscard]] constexpr EngineEventKindMask mask_of(EngineEventKind kind) noexcept
    {
        return static_cast<EngineEventKindMask>(kind);
    }

    struct StateChangedInfo final
    {
        DebuggerState previous {DebuggerState::Detached};
        DebuggerState current {DebuggerState::Detached};
        std::optional<std::uint32_t> pid {};
    };

    struct EngineEvent final
    {
        EngineEventKind kind {EngineEventKind::None};
        std::variant<
            std::monostate,
            StateChangedInfo,
            Breakpoint,
            WatchpointHitInfo,
            ExceptionData,
            ThreadInfo,
            ModuleInfo,
            LogEntry> detail {};
    };
}
