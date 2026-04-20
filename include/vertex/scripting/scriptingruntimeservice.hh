//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/log/ilog.hh>
#include <vertex/runtime/command.hh>
#include <vertex/runtime/fanout.hh>
#include <vertex/runtime/result_channel.hh>
#include <vertex/scripting/iangelscript.hh>
#include <vertex/scripting/iscriptingruntimeservice.hh>
#include <vertex/scripting/scripting_command.hh>
#include <vertex/scripting/scripting_event.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace Vertex::Scripting
{
    class ScriptingRuntimeService final : public IScriptingRuntimeService
    {
      public:
        static constexpr std::chrono::milliseconds REAP_INTERVAL{100};
        static constexpr std::chrono::milliseconds COMPLETED_GRACE{1000};

        ScriptingRuntimeService(IAngelScript& engine,
                                 Thread::IThreadDispatcher& dispatcher,
                                 Log::ILog& log);
        ~ScriptingRuntimeService() override;

        [[nodiscard]] Runtime::CommandId
        send_command(engine::ScriptingCommand command,
                     std::chrono::milliseconds timeout) override;

        void subscribe_result(Runtime::CommandId id, ResultCallback callback) override;

        [[nodiscard]] engine::CommandResult
        await_result(Runtime::CommandId id, std::chrono::milliseconds timeout) override;

        [[nodiscard]] Runtime::SubscriptionId
        subscribe(ScriptingEventKindMask mask, EventCallback callback) override;

        void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept override;

        void on_scripting_event(ScriptingEvent event) override;
        void on_scripting_command_result(engine::CommandResult result) override;

        void shutdown() override;

      private:
        using ResultChannelPtr = std::shared_ptr<Runtime::ResultChannel<engine::CommandResult>>;

        struct PendingResult final
        {
            Runtime::CommandId id{Runtime::INVALID_COMMAND_ID};
            std::chrono::steady_clock::time_point deadline{};
            std::chrono::steady_clock::time_point evictAfter{};
            ResultChannelPtr channel{};
            std::string moduleName{};
            bool completed{false};
        };

        [[nodiscard]] Runtime::CommandId allocate_command_id();
        [[nodiscard]] ResultChannelPtr register_pending(Runtime::CommandId id,
                                                         std::chrono::milliseconds timeout,
                                                         std::string moduleName);
        void synthesise_rejection(Runtime::CommandId id, StatusCode code, std::string moduleName);
        void post_result(Runtime::CommandId id, StatusCode code, std::string moduleName);

        [[nodiscard]] ResultChannelPtr find_channel_locked(Runtime::CommandId id) const;
        void complete_locked(PendingResult& pending);
        StatusCode reap_timeouts();
        void drain_pending_on_shutdown();

        Runtime::CommandId dispatch_execute(engine::CmdExecute command, std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_stop(engine::CmdStop command, std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_reload(engine::CmdReload command, std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_evaluate(engine::CmdEvaluate command, std::chrono::milliseconds timeout);

        IAngelScript& m_engine;
        Thread::IThreadDispatcher& m_dispatcher;
        Log::ILog& m_log;

        Runtime::Fanout<ScriptingEventKind, ScriptingEvent> m_fanout{};

        mutable std::mutex m_pendingMutex{};
        std::unordered_map<Runtime::CommandId, PendingResult> m_pending{};

        mutable std::mutex m_activeMutex{};
        std::unordered_map<std::string, Runtime::CommandId> m_activeByModule{};

        std::atomic<Runtime::CommandId> m_nextCommandId{1};
        std::atomic<bool> m_shuttingDown{false};

        Thread::RecurringTaskHandle m_timeoutReaperHandle{};
        bool m_reaperActive{false};
    };
}
