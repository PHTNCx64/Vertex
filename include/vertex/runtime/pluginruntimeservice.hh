//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/log/ilog.hh>
#include <vertex/runtime/command.hh>
#include <vertex/runtime/fanout.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/runtime/ipluginruntimeservice.hh>
#include <vertex/runtime/plugin_command.hh>
#include <vertex/runtime/plugin_event.hh>
#include <vertex/runtime/result_channel.hh>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Vertex::Runtime::PluginRuntime
{
    class PluginRuntimeService final : public IPluginRuntimeService
    {
      public:
        static constexpr std::chrono::milliseconds COMPLETED_GRACE{1000};

        PluginRuntimeService(Runtime::ILoader& loader, Log::ILog& log);
        ~PluginRuntimeService() override;

        [[nodiscard]] Runtime::CommandId
        send_command(engine::PluginCommand command, std::chrono::milliseconds timeout) override;

        void subscribe_result(Runtime::CommandId id, ResultCallback callback) override;

        [[nodiscard]] engine::CommandResult
        await_result(Runtime::CommandId id, std::chrono::milliseconds timeout) override;

        [[nodiscard]] Runtime::SubscriptionId
        subscribe(PluginEventKindMask mask, EventCallback callback) override;

        void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept override;

        [[nodiscard]] std::vector<PluginSnapshotEntry> snapshot_plugins() const override;
        [[nodiscard]] std::optional<PluginSnapshotEntry> snapshot_active() const override;

        void shutdown() override;

      private:
        using ResultChannelPtr = std::shared_ptr<Runtime::ResultChannel<engine::CommandResult>>;

        struct PendingResult final
        {
            Runtime::CommandId id{Runtime::INVALID_COMMAND_ID};
            ResultChannelPtr channel{};
            bool completed{false};
        };

        [[nodiscard]] Runtime::CommandId allocate_command_id();
        [[nodiscard]] ResultChannelPtr register_pending(Runtime::CommandId id);
        void post_result(Runtime::CommandId id, StatusCode code, std::size_t index, std::string filename);

        Runtime::CommandId dispatch_load(engine::CmdLoad cmd);
        Runtime::CommandId dispatch_unload(engine::CmdUnload cmd);
        Runtime::CommandId dispatch_activate(engine::CmdActivate cmd);
        Runtime::CommandId dispatch_deactivate(engine::CmdDeactivate cmd);

        void fire_plugin_state(std::size_t index, bool loaded, std::string filename);
        void fire_plugin_load_error(std::filesystem::path path, StatusCode code, std::string description);
        void fire_active_changed(std::size_t index, std::string filename);

        Runtime::ILoader& m_loader;
        Log::ILog& m_log;

        Runtime::Fanout<PluginEventKind, PluginEvent> m_fanout{};

        mutable std::mutex m_pendingMutex{};
        std::unordered_map<Runtime::CommandId, PendingResult> m_pending{};

        std::atomic<Runtime::CommandId> m_nextCommandId{1};
        std::atomic<bool> m_shuttingDown{false};

        mutable std::mutex m_activeIndexMutex{};
        std::optional<std::size_t> m_activeIndex{};
    };
}
