//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/debugger/engine_command.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/runtime/command.hh>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Vertex::Debugger
{
    class DebuggerEngine;

    class IDebuggerRuntimeService
    {
    public:
        using EventCallback = std::move_only_function<void(const EngineEvent&) const>;
        using ResultCallback = std::move_only_function<void(const service::CommandResult&) const>;

        static constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT {5000};
        static constexpr std::chrono::milliseconds DEFAULT_SNAPSHOT_TIMEOUT {2000};

        IDebuggerRuntimeService() = default;
        virtual ~IDebuggerRuntimeService() = default;

        IDebuggerRuntimeService(const IDebuggerRuntimeService&) = delete;
        IDebuggerRuntimeService& operator=(const IDebuggerRuntimeService&) = delete;
        IDebuggerRuntimeService(IDebuggerRuntimeService&&) = delete;
        IDebuggerRuntimeService& operator=(IDebuggerRuntimeService&&) = delete;

        [[nodiscard]] virtual Runtime::CommandId
        send_command(service::Command command,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        virtual void subscribe_result(Runtime::CommandId id, ResultCallback callback) = 0;

        [[nodiscard]] virtual service::CommandResult
        await_result(Runtime::CommandId id,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        [[nodiscard]] virtual Runtime::SubscriptionId
        subscribe(EngineEventKindMask mask, EventCallback callback) = 0;

        virtual void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept = 0;

        [[nodiscard]] virtual std::optional<RegisterSet>
        snapshot_registers(std::uint32_t threadId,
                           std::chrono::milliseconds timeout = DEFAULT_SNAPSHOT_TIMEOUT) = 0;

        [[nodiscard]] virtual std::vector<StackFrame>
        snapshot_call_stack(std::uint32_t threadId,
                            std::chrono::milliseconds timeout = DEFAULT_SNAPSHOT_TIMEOUT) = 0;

        [[nodiscard]] virtual std::optional<DisassemblyLine>
        disassemble_one(std::uint64_t pc,
                        std::chrono::milliseconds timeout = DEFAULT_SNAPSHOT_TIMEOUT) = 0;

        virtual void shutdown() = 0;

        virtual void on_engine_event(EngineEvent event) = 0;
        virtual void on_engine_command_result(service::CommandResult result) = 0;

        virtual void attach_engine(DebuggerEngine* engine) noexcept = 0;
    };
}
