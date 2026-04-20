//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/command.hh>
#include <vertex/scripting/scripting_command.hh>
#include <vertex/scripting/scripting_event.hh>

#include <chrono>
#include <functional>

namespace Vertex::Scripting
{
    class IScriptingRuntimeService
    {
      public:
        using EventCallback = std::move_only_function<void(const ScriptingEvent&) const>;
        using ResultCallback = std::move_only_function<void(const engine::CommandResult&) const>;

        static constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT{std::chrono::seconds{30}};

        IScriptingRuntimeService() = default;
        virtual ~IScriptingRuntimeService() = default;

        IScriptingRuntimeService(const IScriptingRuntimeService&) = delete;
        IScriptingRuntimeService& operator=(const IScriptingRuntimeService&) = delete;
        IScriptingRuntimeService(IScriptingRuntimeService&&) = delete;
        IScriptingRuntimeService& operator=(IScriptingRuntimeService&&) = delete;

        [[nodiscard]] virtual Runtime::CommandId
        send_command(engine::ScriptingCommand command,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        virtual void subscribe_result(Runtime::CommandId id, ResultCallback callback) = 0;

        [[nodiscard]] virtual engine::CommandResult
        await_result(Runtime::CommandId id,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        [[nodiscard]] virtual Runtime::SubscriptionId
        subscribe(ScriptingEventKindMask mask, EventCallback callback) = 0;

        virtual void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept = 0;

        virtual void on_scripting_event(ScriptingEvent event) = 0;
        virtual void on_scripting_command_result(engine::CommandResult result) = 0;

        virtual void shutdown() = 0;
    };
}
