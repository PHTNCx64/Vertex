//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/runtime/command.hh>

#include <cstdint>
#include <variant>
#include <vector>

namespace Vertex::Debugger::service
{
    struct CmdAttach final
    {
        std::uint32_t pid {};
    };

    struct CmdDetach final
    {
    };

    struct CmdContinue final
    {
    };

    struct CmdPause final
    {
    };

    struct CmdStep final
    {
        StepMode mode {StepMode::StepInto};
    };

    struct CmdAddBreakpoint final
    {
        Breakpoint breakpoint {};
    };

    struct CmdRemoveBreakpoint final
    {
        std::uint32_t id {};
    };

    struct CmdAddWatchpoint final
    {
        std::uint64_t address {};
        std::uint32_t size {};
        WatchpointType type {WatchpointType::ReadWrite};
    };

    struct CmdRemoveWatchpoint final
    {
        std::uint32_t id {};
    };

    struct CmdReadRegisters final
    {
        std::uint32_t threadId {};
    };

    struct CmdReadCallStack final
    {
        std::uint32_t threadId {};
    };

    struct CmdDisassemble final
    {
        std::uint64_t address {};
        std::uint32_t instructionCount {1};
    };

    struct CmdReadMemory final
    {
        std::uint64_t address {};
        std::uint32_t size {};
    };

    struct CmdWriteMemory final
    {
        std::uint64_t address {};
        std::vector<std::uint8_t> bytes {};
    };

    using Command = std::variant<
        CmdAttach,
        CmdDetach,
        CmdContinue,
        CmdPause,
        CmdStep,
        CmdAddBreakpoint,
        CmdRemoveBreakpoint,
        CmdAddWatchpoint,
        CmdRemoveWatchpoint,
        CmdReadRegisters,
        CmdReadCallStack,
        CmdDisassemble,
        CmdReadMemory,
        CmdWriteMemory>;

    struct AddWatchpointResultPayload final
    {
        std::uint32_t watchpointId {};
    };

    struct AddBreakpointResultPayload final
    {
        std::uint32_t breakpointId {};
    };

    struct RegisterSnapshotPayload final
    {
        RegisterSet registers {};
        std::uint64_t engineGeneration {};
    };

    struct CallStackSnapshotPayload final
    {
        std::vector<StackFrame> frames {};
        std::uint64_t engineGeneration {};
    };

    struct DisassemblyPayload final
    {
        std::vector<DisassemblyLine> lines {};
        std::uint64_t engineGeneration {};
    };

    struct MemoryReadPayload final
    {
        std::vector<std::uint8_t> bytes {};
        std::uint64_t engineGeneration {};
    };

    using CommandResultPayload = std::variant<
        std::monostate,
        AddWatchpointResultPayload,
        AddBreakpointResultPayload,
        RegisterSnapshotPayload,
        CallStackSnapshotPayload,
        DisassemblyPayload,
        MemoryReadPayload>;

    struct CommandResult final
    {
        Runtime::CommandId id {Runtime::INVALID_COMMAND_ID};
        StatusCode code {STATUS_OK};
        CommandResultPayload payload {};
    };
}
