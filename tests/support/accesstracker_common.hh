//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <vertex/debugger/engine_command.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <sdk/statuscode.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace Vertex::Testing::AccessTracker
{
    namespace dbg = Vertex::Debugger;
    namespace service = Vertex::Debugger::service;

    struct QueuedCommand final
    {
        Vertex::Runtime::CommandId id {Vertex::Runtime::INVALID_COMMAND_ID};
        service::Command command {};
    };

    class ControlledFakeRuntime final : public dbg::IDebuggerRuntimeService
    {
    public:
        Vertex::Runtime::CommandId
        send_command(service::Command command, std::chrono::milliseconds) override
        {
            const auto id = m_nextCmdId.fetch_add(1, std::memory_order_relaxed);
            std::scoped_lock lock {m_mutex};

            if (std::holds_alternative<service::CmdAddWatchpoint>(command))
            {
                m_addWatchpointCallCount.fetch_add(1, std::memory_order_relaxed);
            }
            else if (std::holds_alternative<service::CmdRemoveWatchpoint>(command))
            {
                m_removeWatchpointCallCount.fetch_add(1, std::memory_order_relaxed);
            }

            m_pending.emplace(id, QueuedCommand {.id = id, .command = std::move(command)});
            return id;
        }

        void subscribe_result(Vertex::Runtime::CommandId id, ResultCallback cb) override
        {
            QueuedCommand pending {};
            bool autoComplete {};
            std::optional<StatusCode> nextAddStatus {};
            std::optional<StatusCode> nextRemoveStatus {};

            {
                std::scoped_lock lock {m_mutex};
                const auto it = m_pending.find(id);
                if (it == m_pending.end())
                {
                    cb(service::CommandResult {.id = id, .code = STATUS_TIMEOUT});
                    return;
                }
                if (!m_autoCompleteEnabled)
                {
                    m_resultCallbacks.emplace(id, std::move(cb));
                    return;
                }
                pending = std::move(it->second);
                m_pending.erase(it);
                autoComplete = true;
                if (!m_nextAddStatuses.empty())
                {
                    nextAddStatus = m_nextAddStatuses.front();
                    m_nextAddStatuses.pop_front();
                }
                if (!m_nextRemoveStatuses.empty())
                {
                    nextRemoveStatus = m_nextRemoveStatuses.front();
                    m_nextRemoveStatuses.pop_front();
                }
            }

            if (!autoComplete)
            {
                return;
            }

            cb(build_result(pending, nextAddStatus, nextRemoveStatus));
        }

        service::CommandResult
        await_result(Vertex::Runtime::CommandId id, std::chrono::milliseconds) override
        {
            return service::CommandResult {.id = id, .code = STATUS_ERROR_NOT_IMPLEMENTED};
        }

        Vertex::Runtime::SubscriptionId
        subscribe(dbg::EngineEventKindMask mask, EventCallback cb) override
        {
            const auto id = m_nextSubId.fetch_add(1, std::memory_order_relaxed);
            std::scoped_lock lock {m_mutex};
            m_subscriptions.emplace(id, Subscriber {.mask = mask, .callback = std::move(cb)});
            m_totalSubscribes.fetch_add(1, std::memory_order_relaxed);
            return id;
        }

        void unsubscribe(Vertex::Runtime::SubscriptionId id) noexcept override
        {
            std::scoped_lock lock {m_mutex};
            m_subscriptions.erase(id);
            m_totalUnsubscribes.fetch_add(1, std::memory_order_relaxed);
        }

        std::optional<dbg::RegisterSet>
        snapshot_registers(std::uint32_t, std::chrono::milliseconds) override
        {
            m_registerCalls.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }

        std::vector<dbg::StackFrame>
        snapshot_call_stack(std::uint32_t, std::chrono::milliseconds) override { return {}; }

        std::optional<dbg::DisassemblyLine>
        disassemble_one(std::uint64_t, std::chrono::milliseconds) override { return std::nullopt; }

        void shutdown() override {}

        void on_engine_event(dbg::EngineEvent event) override
        {
            std::vector<EventCallback*> callbacks {};
            {
                std::scoped_lock lock {m_mutex};
                for (auto& [id, sub] : m_subscriptions)
                {
                    if ((sub.mask & dbg::mask_of(event.kind)) != 0)
                    {
                        callbacks.push_back(&sub.callback);
                    }
                }
            }
            for (auto* cb : callbacks)
            {
                if (*cb)
                {
                    (*cb)(event);
                }
            }
        }

        void on_engine_command_result(service::CommandResult) override {}

        void attach_engine(dbg::DebuggerEngine*) noexcept override {}

        

        void set_auto_complete(const bool enabled)
        {
            std::scoped_lock lock {m_mutex};
            m_autoCompleteEnabled = enabled;
        }

        void push_next_add_status(StatusCode status)
        {
            std::scoped_lock lock {m_mutex};
            m_nextAddStatuses.push_back(status);
        }

        void push_next_remove_status(StatusCode status)
        {
            std::scoped_lock lock {m_mutex};
            m_nextRemoveStatuses.push_back(status);
        }

        void complete_command(Vertex::Runtime::CommandId id,
                              const StatusCode status = STATUS_OK)
        {
            QueuedCommand pending {};
            ResultCallback cb {};
            {
                std::scoped_lock lock {m_mutex};
                const auto it = m_pending.find(id);
                if (it == m_pending.end())
                {
                    return;
                }
                pending = std::move(it->second);
                m_pending.erase(it);

                const auto cbIt = m_resultCallbacks.find(id);
                if (cbIt == m_resultCallbacks.end())
                {
                    return;
                }
                cb = std::move(cbIt->second);
                m_resultCallbacks.erase(cbIt);
            }

            if (!cb)
            {
                return;
            }

            auto result = build_result(pending, std::nullopt, std::nullopt);
            result.code = status;
            if (status != STATUS_OK && std::holds_alternative<service::CmdAddWatchpoint>(pending.command))
            {
                result.payload = std::monostate {};
            }
            cb(result);
        }

        std::optional<Vertex::Runtime::CommandId> peek_oldest_pending()
        {
            std::scoped_lock lock {m_mutex};
            if (m_pending.empty())
            {
                return std::nullopt;
            }
            Vertex::Runtime::CommandId oldest {std::numeric_limits<std::uint64_t>::max()};
            for (const auto& [id, _] : m_pending)
            {
                oldest = std::min(oldest, id);
            }
            return oldest;
        }

        void fire_detached()
        {
            on_engine_event(dbg::EngineEvent {
                .kind = dbg::EngineEventKind::StateChanged,
                .detail = dbg::StateChangedInfo {
                    .previous = dbg::DebuggerState::Running,
                    .current = dbg::DebuggerState::Detached,
                    .pid = std::nullopt,
                },
            });
        }

        void fire_watchpoint_hit(std::uint32_t watchpointId,
                                  std::uint64_t pc,
                                  std::uint32_t threadId = 1,
                                  std::uint64_t accessAddress = 0x1000)
        {
            on_engine_event(dbg::EngineEvent {
                .kind = dbg::EngineEventKind::WatchpointHit,
                .detail = dbg::WatchpointHitInfo {
                    .watchpointId = watchpointId,
                    .threadId = threadId,
                    .accessAddress = accessAddress,
                    .instructionAddress = pc,
                    .accessType = dbg::WatchpointType::Write,
                    .accessSize = 4,
                },
            });
        }

        [[nodiscard]] std::size_t active_subscription_count() const
        {
            std::scoped_lock lock {m_mutex};
            return m_subscriptions.size();
        }

        [[nodiscard]] std::size_t pending_command_count() const
        {
            std::scoped_lock lock {m_mutex};
            return m_pending.size();
        }

        [[nodiscard]] std::uint64_t total_subscribes() const noexcept
        {
            return m_totalSubscribes.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint64_t total_unsubscribes() const noexcept
        {
            return m_totalUnsubscribes.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint64_t add_watchpoint_call_count() const noexcept
        {
            return m_addWatchpointCallCount.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint64_t remove_watchpoint_call_count() const noexcept
        {
            return m_removeWatchpointCallCount.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint64_t register_calls() const noexcept
        {
            return m_registerCalls.load(std::memory_order_relaxed);
        }

    private:
        struct Subscriber final
        {
            dbg::EngineEventKindMask mask {};
            EventCallback callback {};
        };

        service::CommandResult build_result(const QueuedCommand& pending,
                                             std::optional<StatusCode> nextAddStatus,
                                             std::optional<StatusCode> nextRemoveStatus)
        {
            service::CommandResult result {.id = pending.id, .code = STATUS_OK};
            if (std::holds_alternative<service::CmdAddWatchpoint>(pending.command))
            {
                if (nextAddStatus.has_value() && *nextAddStatus != STATUS_OK)
                {
                    result.code = *nextAddStatus;
                    return result;
                }
                const auto wpId = m_nextWatchpointId.fetch_add(1, std::memory_order_relaxed);
                result.payload = service::AddWatchpointResultPayload {.watchpointId = wpId};
            }
            else if (std::holds_alternative<service::CmdRemoveWatchpoint>(pending.command))
            {
                if (nextRemoveStatus.has_value())
                {
                    result.code = *nextRemoveStatus;
                }
            }
            return result;
        }

        mutable std::mutex m_mutex {};
        bool m_autoCompleteEnabled {true};
        std::unordered_map<Vertex::Runtime::CommandId, QueuedCommand> m_pending {};
        std::unordered_map<Vertex::Runtime::CommandId, ResultCallback> m_resultCallbacks {};
        std::unordered_map<Vertex::Runtime::SubscriptionId, Subscriber> m_subscriptions {};
        std::deque<StatusCode> m_nextAddStatuses {};
        std::deque<StatusCode> m_nextRemoveStatuses {};

        std::atomic<Vertex::Runtime::CommandId> m_nextCmdId {1};
        std::atomic<Vertex::Runtime::SubscriptionId> m_nextSubId {1};
        std::atomic<std::uint32_t> m_nextWatchpointId {100};
        std::atomic<std::uint64_t> m_totalSubscribes {};
        std::atomic<std::uint64_t> m_totalUnsubscribes {};
        std::atomic<std::uint64_t> m_addWatchpointCallCount {};
        std::atomic<std::uint64_t> m_removeWatchpointCallCount {};
        std::atomic<std::uint64_t> m_registerCalls {};
    };

    class DeferrableDispatcher final : public Vertex::Thread::IThreadDispatcher
    {
    public:
        std::expected<std::future<StatusCode>, StatusCode>
        dispatch(Vertex::Thread::ThreadChannel, std::packaged_task<StatusCode()>&& task) override
        {
            auto future = task.get_future();
            task();
            return future;
        }

        StatusCode
        dispatch_fire_and_forget(Vertex::Thread::ThreadChannel, std::packaged_task<StatusCode()>&& task) override
        {
            if (m_deferred.load(std::memory_order_acquire))
            {
                std::scoped_lock lock {m_mutex};
                m_deferredTasks.emplace_back(std::move(task));
                return STATUS_OK;
            }
            task();
            return STATUS_OK;
        }

        std::expected<Vertex::Thread::RecurringTaskHandle, StatusCode>
        schedule_recurring(Vertex::Thread::ThreadChannel, Vertex::Thread::DispatchPriority,
                           Vertex::Thread::RecurringPolicy, std::chrono::milliseconds,
                           std::function<StatusCode()>,
                           Vertex::Thread::RecurringFailurePolicy) override
        {
            return std::unexpected {STATUS_ERROR_NOT_IMPLEMENTED};
        }

        std::expected<Vertex::Thread::RecurringTaskHandle, StatusCode>
        schedule_recurring_persistent(Vertex::Thread::ThreadChannel, Vertex::Thread::DispatchPriority,
                                      Vertex::Thread::RecurringPolicy, std::chrono::milliseconds,
                                      std::function<StatusCode()>,
                                      Vertex::Thread::RecurringFailurePolicy) override
        {
            return std::unexpected {STATUS_ERROR_NOT_IMPLEMENTED};
        }

        StatusCode cancel_recurring(Vertex::Thread::RecurringTaskHandle) override { return STATUS_OK; }

        std::expected<std::future<StatusCode>, StatusCode>
        dispatch_with_priority(Vertex::Thread::ThreadChannel, Vertex::Thread::DispatchPriority,
                               std::packaged_task<StatusCode()>&& task) override
        {
            auto future = task.get_future();
            task();
            return future;
        }

        StatusCode configure(std::uint64_t) override { return STATUS_OK; }
        StatusCode start() override { return STATUS_OK; }
        StatusCode stop() override { return STATUS_OK; }
        bool is_single_threaded() const noexcept override { return true; }
        bool is_channel_busy(Vertex::Thread::ThreadChannel) const override { return false; }
        std::size_t pending_tasks(Vertex::Thread::ThreadChannel) const override { return 0; }

        StatusCode create_worker_pool(Vertex::Thread::ThreadChannel, std::size_t) override { return STATUS_OK; }
        StatusCode destroy_worker_pool(Vertex::Thread::ThreadChannel) override { return STATUS_OK; }
        StatusCode enqueue_on_worker(Vertex::Thread::ThreadChannel, std::size_t,
                                      std::packaged_task<StatusCode()>&&) override { return STATUS_OK; }

        void set_deferred(const bool deferred) noexcept
        {
            m_deferred.store(deferred, std::memory_order_release);
        }

        std::size_t flush_deferred()
        {
            std::vector<std::packaged_task<StatusCode()>> tasks {};
            {
                std::scoped_lock lock {m_mutex};
                tasks.swap(m_deferredTasks);
            }
            for (auto& task : tasks)
            {
                task();
            }
            return tasks.size();
        }

        std::size_t deferred_count() const
        {
            std::scoped_lock lock {m_mutex};
            return m_deferredTasks.size();
        }

    private:
        std::atomic<bool> m_deferred {false};
        mutable std::mutex m_mutex {};
        std::vector<std::packaged_task<StatusCode()>> m_deferredTasks {};
    };
}
