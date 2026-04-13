//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <sdk/api.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace debugger
{
    struct SoftwareBreakpointData final
    {
        std::uint32_t id{};
        std::uint64_t address{};
        ::BreakpointType type{};
        ::BreakpointState state{VERTEX_BP_STATE_ENABLED};
        std::uint8_t originalByte{};
        std::uint32_t hitCount{};
        bool temporary{false};
        ::BreakpointCondition condition{};
    };

    struct HardwareBreakpointData final
    {
        std::uint32_t id{};
        std::uint64_t address{};
        ::BreakpointType type{};
        ::BreakpointState state{VERTEX_BP_STATE_ENABLED};
        std::uint8_t size{1};
        std::uint8_t registerIndex{};
        std::uint32_t hitCount{};
        ::BreakpointCondition condition{};
    };

    struct WatchpointData final
    {
        std::uint32_t id{};
        std::uint64_t address{};
        std::uint32_t size{};
        ::WatchpointType type{};
        bool enabled{true};
        bool temporarilyDisabled{false};
        std::uint8_t registerIndex{};
        std::uint32_t hitCount{};
    };

    struct BreakpointManager final
    {
        std::mutex mutex{};
        std::unordered_map<std::uint32_t, SoftwareBreakpointData> softwareBreakpoints{};
        std::unordered_map<std::uint32_t, HardwareBreakpointData> hardwareBreakpoints{};
        std::unordered_map<std::uint32_t, WatchpointData> watchpoints{};
        std::atomic<std::uint32_t> nextBreakpointId{1};
        std::atomic<std::uint32_t> nextWatchpointId{1};
        std::array<bool, 4> hwRegisterUsed{};
    };

    BreakpointManager& get_breakpoint_manager();
    void reset_breakpoint_manager();
    StatusCode apply_all_hw_breakpoints_to_thread(std::uint32_t threadId);

    [[nodiscard]] constexpr ::BreakpointType convert_watchpoint_type_to_breakpoint(::WatchpointType type)
    {
        switch (type)
        {
        case VERTEX_WP_READ:
            return VERTEX_BP_READ;
        case VERTEX_WP_WRITE:
            return VERTEX_BP_WRITE;
        case VERTEX_WP_READWRITE:
            return VERTEX_BP_READWRITE;
        case VERTEX_WP_EXECUTE:
            return VERTEX_BP_EXECUTE;
        default:
            return VERTEX_BP_EXECUTE;
        }
    }

    enum class DebugCommand : std::uint8_t
    {
        None,
        Continue,
        StepInto,
        StepOver,
        StepOut,
        RunToAddress
    };

    enum class PauseReason : std::uint8_t
    {
        None,
        UserBreakpoint,
        HardwareBreakpoint,
        TempBreakpoint,
        Watchpoint,
        SingleStep,
        PauseRequested,
        Exception
    };
}
