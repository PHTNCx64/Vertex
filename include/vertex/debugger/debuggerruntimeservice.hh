//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/engine_command.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/log/ilog.hh>
#include <vertex/runtime/command.hh>
#include <vertex/runtime/fanout.hh>
#include <vertex/runtime/result_channel.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Vertex::Debugger
{
    class DebuggerEngine;

    class DebuggerRuntimeService final : public IDebuggerRuntimeService
    {
    public:
        static constexpr std::chrono::milliseconds REAP_INTERVAL {100};
        static constexpr std::chrono::milliseconds COMPLETED_GRACE {1000};

        DebuggerRuntimeService(Thread::IThreadDispatcher& dispatcher, Log::ILog& log);
        ~DebuggerRuntimeService() override;

        DebuggerRuntimeService(const DebuggerRuntimeService&) = delete;
        DebuggerRuntimeService& operator=(const DebuggerRuntimeService&) = delete;
        DebuggerRuntimeService(DebuggerRuntimeService&&) = delete;
        DebuggerRuntimeService& operator=(DebuggerRuntimeService&&) = delete;

        [[nodiscard]] Runtime::CommandId
        send_command(service::Command command,
                     std::chrono::milliseconds timeout) override;

        void subscribe_result(Runtime::CommandId id, ResultCallback callback) override;

        [[nodiscard]] service::CommandResult
        await_result(Runtime::CommandId id, std::chrono::milliseconds timeout) override;

        [[nodiscard]] Runtime::SubscriptionId
        subscribe(EngineEventKindMask mask, EventCallback callback) override;

        void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept override;

        [[nodiscard]] std::optional<RegisterSet>
        snapshot_registers(std::uint32_t threadId, std::chrono::milliseconds timeout) override;

        [[nodiscard]] std::vector<StackFrame>
        snapshot_call_stack(std::uint32_t threadId, std::chrono::milliseconds timeout) override;

        [[nodiscard]] std::optional<DisassemblyLine>
        disassemble_one(std::uint64_t pc, std::chrono::milliseconds timeout) override;

        void shutdown() override;

        void on_engine_event(EngineEvent event) override;
        void on_engine_command_result(service::CommandResult result) override;

        void attach_engine(DebuggerEngine* engine) noexcept override;

        void set_ui_thread(std::thread::id uiThread) noexcept;
        void clear_ui_thread() noexcept;

    private:
        using ResultChannelPtr = std::shared_ptr<Runtime::ResultChannel<service::CommandResult>>;

        struct PendingResult final
        {
            Runtime::CommandId id {Runtime::INVALID_COMMAND_ID};
            std::chrono::steady_clock::time_point deadline {};
            std::chrono::steady_clock::time_point evictAfter {};
            ResultChannelPtr channel {};
            bool completed {false};
        };

        [[nodiscard]] ResultChannelPtr find_channel_locked(Runtime::CommandId id) const;
        void complete_locked(PendingResult& pending, service::CommandResult result);
        StatusCode reap_timeouts();
        void drain_pending_on_shutdown();

        Thread::IThreadDispatcher& m_dispatcher;
        Log::ILog& m_log;

        Runtime::Fanout<EngineEventKind, EngineEvent> m_fanout {};

        mutable std::mutex m_pendingMutex {};
        std::unordered_map<Runtime::CommandId, PendingResult> m_pending {};

        std::atomic<Runtime::CommandId> m_nextCommandId {1};
        std::atomic<bool> m_shuttingDown {false};

        mutable std::mutex m_uiThreadMutex {};
        bool m_uiThreadGuardEnabled {false};
        std::thread::id m_uiThreadId {};

        Thread::RecurringTaskHandle m_timeoutReaperHandle {};
        bool m_reaperActive {false};

        std::atomic<DebuggerEngine*> m_engine {nullptr};
    };
}
