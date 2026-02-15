//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <sdk/api.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <unordered_map>

namespace debugger
{
    struct SoftwareBreakpointData
    {
        std::uint32_t id{};
        std::uint64_t address{};
        ::BreakpointType type{};
        ::BreakpointState state{VERTEX_BP_STATE_ENABLED};
        std::uint8_t originalByte{};
        std::uint32_t hitCount{};
        bool temporary{false};
    };

    struct HardwareBreakpointData
    {
        std::uint32_t id{};
        std::uint64_t address{};
        ::BreakpointType type{};
        ::BreakpointState state{VERTEX_BP_STATE_ENABLED};
        std::uint8_t size{1};
        std::uint8_t registerIndex{};
        std::uint32_t hitCount{};
    };

    struct WatchpointData
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

    struct BreakpointManager
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

    struct DebugLoopContext final
    {
        std::atomic<bool>* stopRequested{};
        std::atomic<::DebuggerState>* currentState{};
        std::atomic<std::uint32_t>* attachedProcessId{};
        std::atomic<std::uint32_t>* pendingAttachProcessId{};
        std::atomic<std::uint32_t>* currentThreadId{};
        std::atomic<bool>* passException{};
        std::optional<::DebuggerCallbacks>* callbacks{};
        std::mutex* callbackMutex{};

        std::atomic<DebugCommand>* pendingCommand{};
        std::atomic<std::uint64_t>* targetAddress{};
        std::condition_variable* commandSignal{};
        std::mutex* commandMutex{};
        std::atomic<bool>* isWow64Process{};
        std::atomic<bool>* initialBreakpointPending{};
        std::atomic<bool>* pauseRequested{};
    };

    void run_debug_loop(const DebugLoopContext& ctx, const std::stop_token& stopToken);
}
