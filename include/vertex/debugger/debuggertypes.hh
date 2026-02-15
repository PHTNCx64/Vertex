//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <string>
#include <vector>
#include <optional>

namespace Vertex::Debugger
{
    enum class DebuggerState : std::uint8_t
    {
        Detached = 0,
        Attached,
        Running,
        Paused,
        Stepping,
        BreakpointHit,
        Exception
    };

    enum class StepMode : std::uint8_t
    {
        StepInto = 0,
        StepOver,
        StepOut
    };

    enum class BranchType : std::uint8_t
    {
        None = 0,
        UnconditionalJump,
        ConditionalJump,
        Call,
        Return,
        Loop,
        Interrupt
    };

    struct DisassemblyLine final
    {
        std::uint64_t address {};
        std::vector<std::uint8_t> bytes {};
        std::string mnemonic {};
        std::string operands {};
        std::string comment {};
        bool isCurrentInstruction {};
        bool hasBreakpoint {};
        bool isJumpTarget {};
        bool isCallTarget {};
        std::optional<std::uint64_t> branchTarget {};
        BranchType branchType {BranchType::None};
    };

    struct DisassemblyRange final
    {
        std::uint64_t startAddress {};
        std::uint64_t endAddress {};
        std::vector<DisassemblyLine> lines {};
    };

    enum class BreakpointType : std::uint8_t
    {
        Software = 0,
        Hardware,
        Memory,
        Conditional
    };

    enum class BreakpointState : std::uint8_t
    {
        Enabled = 0,
        Disabled,
        Pending,
        Error
    };

    struct Breakpoint final
    {
        std::uint32_t id {};
        std::uint64_t address {};
        BreakpointType type {BreakpointType::Software};
        BreakpointState state {BreakpointState::Enabled};
        std::string condition {};
        std::string moduleName {};
        std::uint32_t hitCount {};
        bool temporary {};
    };

    enum class WatchpointType : std::uint8_t
    {
        Read = 0,
        Write,
        ReadWrite,
        Execute
    };

    struct Watchpoint final
    {
        std::uint32_t id {};
        std::uint64_t address {};
        std::uint32_t size {};
        WatchpointType type {WatchpointType::ReadWrite};
        bool enabled {true};
        std::uint32_t hitCount {};
        std::uint64_t lastAccessorAddress {};
    };

    struct WatchpointHitInfo final
    {
        std::uint32_t watchpointId {};
        std::uint32_t threadId {};
        std::uint64_t accessAddress {};
        std::uint64_t instructionAddress {};
        WatchpointType accessType {};
        std::uint8_t accessSize {};
    };

    enum class RegisterCategory : std::uint8_t
    {
        General = 0,
        Segment,
        Flags,
        FloatingPoint,
        Vector,
        Debug,
        Control
    };

    struct Register final
    {
        std::string name {};
        RegisterCategory category {RegisterCategory::General};
        std::uint64_t value {};
        std::uint64_t previousValue {};
        std::uint8_t bitWidth {};
        bool modified {};
    };

    struct RegisterSet final
    {
        std::vector<Register> generalPurpose {};
        std::vector<Register> segment {};
        std::vector<Register> flags {};
        std::vector<Register> floatingPoint {};
        std::vector<Register> vector {};
        std::uint64_t instructionPointer {};
        std::uint64_t stackPointer {};
        std::uint64_t basePointer {};
    };

    struct StackFrame final
    {
        std::uint32_t frameIndex {};
        std::uint64_t returnAddress {};
        std::uint64_t framePointer {};
        std::uint64_t stackPointer {};
        std::string functionName {};
        std::string moduleName {};
        std::string sourceFile {};
        std::uint32_t sourceLine {};
    };

    struct CallStack final
    {
        std::vector<StackFrame> frames {};
        std::uint32_t currentFrameIndex {};
    };

    struct MemoryBlock final
    {
        std::uint64_t baseAddress {};
        std::vector<std::uint8_t> data {};
        std::vector<bool> readable {};
        std::vector<bool> modified {};
    };

    struct ImportEntry final
    {
        std::string moduleName {};
        std::string functionName {};
        std::uint64_t address {};
        std::uint64_t hint {};
        bool bound {};
    };

    struct ExportEntry final
    {
        std::string functionName {};
        std::uint64_t address {};
        std::uint32_t ordinal {};
        bool forwarded {};
        std::string forwardTarget {};
    };

    struct ModuleInfo final
    {
        std::string name {};
        std::string path {};
        std::uint64_t baseAddress {};
        std::uint64_t size {};
        std::vector<ImportEntry> imports {};
        std::vector<ExportEntry> exports {};
    };

    enum class ThreadState : std::uint8_t
    {
        Running = 0,
        Suspended,
        Waiting,
        Terminated
    };

    struct ThreadInfo final
    {
        std::uint32_t id {};
        std::string name {};
        ThreadState state {ThreadState::Running};
        std::uint64_t instructionPointer {};
        std::uint64_t stackPointer {};
        std::uint64_t entryPoint {};
        std::int32_t priority {};
        std::string priorityString {};
        bool isCurrent {};
    };

    enum class VariableType : std::uint8_t
    {
        Unknown = 0,
        Integer,
        Float,
        Double,
        Pointer,
        String,
        Array,
        Struct,
        Class,
        Enum,
        Boolean
    };

    struct WatchVariable final
    {
        std::uint32_t id {};
        std::string name {};
        std::string expression {};
        std::string value {};
        std::string typeName {};
        VariableType type {VariableType::Unknown};
        std::uint64_t address {};
        std::uint32_t size {};
        bool hasChildren {};
        bool isExpanded {};
        bool hasError {};
        std::string errorMessage {};
        std::vector<WatchVariable> children {};
    };

    struct LocalVariable final
    {
        std::string name {};
        std::string value {};
        std::string typeName {};
        VariableType type {VariableType::Unknown};
        std::uint64_t address {};
        std::uint32_t size {};
        std::uint32_t frameIndex {};
        bool hasChildren {};
        std::vector<LocalVariable> children {};
    };

    enum class LogLevel : std::uint8_t
    {
        Debug = 0,
        Info,
        Warning,
        Error,
        Output
    };

    struct LogEntry final
    {
        std::uint64_t timestamp {};
        LogLevel level {LogLevel::Info};
        std::string message {};
        std::uint32_t threadId {};
        std::string source {};
    };

    struct ExceptionData final
    {
        std::uint32_t code {};
        std::uint64_t address {};
        std::uint32_t threadId {};
        std::string description {};
        bool continuable {};
        bool firstChance {};
    };

    struct DebugEvent final
    {
        std::uint64_t address {};
        std::uint32_t threadId {};
        std::string description {};
        std::optional<std::uint32_t> breakpointId {};
    };
}
