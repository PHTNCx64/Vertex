//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/command.hh>
#include <vertex/runtime/plugin_command.hh>
#include <vertex/runtime/plugin_event.hh>

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Vertex::Runtime::PluginRuntime
{
    struct PluginSnapshotEntry final
    {
        std::size_t index{};
        bool loaded{};
        bool active{};
        std::string filename{};
    };

    class IPluginRuntimeService
    {
      public:
        using EventCallback = std::move_only_function<void(const PluginEvent&) const>;
        using ResultCallback = std::move_only_function<void(const engine::CommandResult&) const>;

        static constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT{std::chrono::seconds{30}};

        IPluginRuntimeService() = default;
        virtual ~IPluginRuntimeService() = default;

        IPluginRuntimeService(const IPluginRuntimeService&) = delete;
        IPluginRuntimeService& operator=(const IPluginRuntimeService&) = delete;
        IPluginRuntimeService(IPluginRuntimeService&&) = delete;
        IPluginRuntimeService& operator=(IPluginRuntimeService&&) = delete;

        [[nodiscard]] virtual Runtime::CommandId
        send_command(engine::PluginCommand command,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        virtual void subscribe_result(Runtime::CommandId id, ResultCallback callback) = 0;

        [[nodiscard]] virtual engine::CommandResult
        await_result(Runtime::CommandId id,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        [[nodiscard]] virtual Runtime::SubscriptionId
        subscribe(PluginEventKindMask mask, EventCallback callback) = 0;

        virtual void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept = 0;

        [[nodiscard]] virtual std::vector<PluginSnapshotEntry> snapshot_plugins() const = 0;
        [[nodiscard]] virtual std::optional<PluginSnapshotEntry> snapshot_active() const = 0;

        virtual void shutdown() = 0;
    };
}
