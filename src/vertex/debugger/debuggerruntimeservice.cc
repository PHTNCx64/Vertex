//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerruntimeservice.hh>

#include <vertex/debugger/debuggerengine.hh>

#include <cassert>
#include <utility>
#include <vector>

namespace Vertex::Debugger
{
    DebuggerRuntimeService::DebuggerRuntimeService(Thread::IThreadDispatcher& dispatcher,
                                                     Log::ILog& log)
        : m_dispatcher {dispatcher}
        , m_log {log}
    {
        const auto handle = m_dispatcher.schedule_recurring_persistent(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            Thread::RecurringPolicy::FixedDelay,
            REAP_INTERVAL,
            [this] { return reap_timeouts(); },
            Thread::RecurringFailurePolicy::Continue);

        if (handle.has_value())
        {
            m_timeoutReaperHandle = *handle;
            m_reaperActive = true;
            return;
        }

        m_log.log_error(
            "DebuggerRuntimeService: persistent timeout reaper schedule failed");
    }

    DebuggerRuntimeService::~DebuggerRuntimeService()
    {
        shutdown();
    }

    Runtime::CommandId
    DebuggerRuntimeService::send_command(service::Command command,
                                          std::chrono::milliseconds timeout)
    {
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        const auto id = m_nextCommandId.fetch_add(1, std::memory_order_relaxed);

        {
            const auto now = std::chrono::steady_clock::now();
            PendingResult pending{.id = id, .deadline = now + timeout, .evictAfter = {}, .channel = std::make_shared<Runtime::ResultChannel<service::CommandResult>>(), .completed = false};
            std::scoped_lock lock {m_pendingMutex};
            if (m_shuttingDown.load(std::memory_order_acquire))
            {
                return Runtime::INVALID_COMMAND_ID;
            }
            m_pending.emplace(id, std::move(pending));
        }

        if (auto* engine = m_engine.load(std::memory_order_acquire); engine != nullptr)
        {
            engine->enqueue_service_command(id, std::move(command));
        }
        return id;
    }

    void DebuggerRuntimeService::subscribe_result(Runtime::CommandId id, ResultCallback callback)
    {
        if (id == Runtime::INVALID_COMMAND_ID || !callback)
        {
            return;
        }

        ResultChannelPtr channel {};
        {
            std::scoped_lock lock {m_pendingMutex};
            channel = find_channel_locked(id);
        }

        if (!channel)
        {
            service::CommandResult timeout {.id = id, .code = STATUS_TIMEOUT};
            callback(timeout);
            return;
        }

        channel->on_result(
            [cb = std::move(callback)](const service::CommandResult& result)
            {
                cb(result);
            });
    }

    service::CommandResult
    DebuggerRuntimeService::await_result(Runtime::CommandId id, std::chrono::milliseconds timeout)
    {
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return service::CommandResult {.id = id, .code = STATUS_ERROR_INVALID_PARAMETER};
        }

        {
            std::scoped_lock lock {m_uiThreadMutex};
            if (m_uiThreadGuardEnabled && std::this_thread::get_id() == m_uiThreadId)
            {
                m_log.log_error(
                    "DebuggerRuntimeService::await_result called from UI thread; use subscribe_result instead");
                assert(false && "await_result forbidden on UI thread");
                return service::CommandResult {.id = id, .code = STATUS_ERROR_INVALID_STATE};
            }
        }

        ResultChannelPtr channel {};
        {
            std::scoped_lock lock {m_pendingMutex};
            channel = find_channel_locked(id);
        }

        if (!channel)
        {
            return service::CommandResult {.id = id, .code = STATUS_TIMEOUT};
        }

        if (!channel->wait_for(timeout))
        {
            return service::CommandResult {.id = id, .code = STATUS_TIMEOUT};
        }

        auto result = channel->copy_result();
        if (result.has_value())
        {
            return std::move(*result);
        }
        return service::CommandResult {.id = id, .code = STATUS_TIMEOUT};
    }

    Runtime::SubscriptionId
    DebuggerRuntimeService::subscribe(EngineEventKindMask mask, EventCallback callback)
    {
        return m_fanout.subscribe(mask, std::move(callback));
    }

    void DebuggerRuntimeService::unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept
    {
        m_fanout.unsubscribe(subscriptionId);
    }

    std::optional<RegisterSet>
    DebuggerRuntimeService::snapshot_registers(std::uint32_t threadId, std::chrono::milliseconds timeout)
    {
        const auto id = send_command(service::CmdReadRegisters {.threadId = threadId}, timeout);
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return std::nullopt;
        }
        const auto result = await_result(id, timeout);
        if (result.code != STATUS_OK)
        {
            return std::nullopt;
        }
        if (const auto* payload = std::get_if<service::RegisterSnapshotPayload>(&result.payload))
        {
            return payload->registers;
        }
        return std::nullopt;
    }

    std::vector<StackFrame>
    DebuggerRuntimeService::snapshot_call_stack(std::uint32_t threadId, std::chrono::milliseconds timeout)
    {
        const auto id = send_command(service::CmdReadCallStack {.threadId = threadId}, timeout);
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return {};
        }
        const auto result = await_result(id, timeout);
        if (result.code != STATUS_OK)
        {
            return {};
        }
        if (const auto* payload = std::get_if<service::CallStackSnapshotPayload>(&result.payload))
        {
            return payload->frames;
        }
        return {};
    }

    std::optional<DisassemblyLine>
    DebuggerRuntimeService::disassemble_one(std::uint64_t pc, std::chrono::milliseconds timeout)
    {
        const auto id = send_command(
            service::CmdDisassemble {.address = pc, .instructionCount = 1}, timeout);
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return std::nullopt;
        }
        const auto result = await_result(id, timeout);
        if (result.code != STATUS_OK)
        {
            return std::nullopt;
        }
        if (const auto* payload = std::get_if<service::DisassemblyPayload>(&result.payload))
        {
            if (!payload->lines.empty())
            {
                return payload->lines.front();
            }
        }
        return std::nullopt;
    }

    void DebuggerRuntimeService::shutdown()
    {
        bool expected = false;
        if (!m_shuttingDown.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        if (m_reaperActive)
        {
            const auto code = m_dispatcher.cancel_recurring(m_timeoutReaperHandle);
            if (code != STATUS_OK)
            {
                m_log.log_warn("DebuggerRuntimeService: cancel_recurring failed");
            }
            m_reaperActive = false;
        }

        m_fanout.shutdown();
        drain_pending_on_shutdown();
    }

    void DebuggerRuntimeService::set_ui_thread(std::thread::id uiThread) noexcept
    {
        std::scoped_lock lock {m_uiThreadMutex};
        m_uiThreadId = uiThread;
        m_uiThreadGuardEnabled = true;
    }

    void DebuggerRuntimeService::clear_ui_thread() noexcept
    {
        std::scoped_lock lock {m_uiThreadMutex};
        m_uiThreadGuardEnabled = false;
        m_uiThreadId = std::thread::id {};
    }

    void DebuggerRuntimeService::on_engine_event(EngineEvent event)
    {
        m_fanout.fire(event);
    }

    void DebuggerRuntimeService::attach_engine(DebuggerEngine* engine) noexcept
    {
        m_engine.store(engine, std::memory_order_release);
    }

    void DebuggerRuntimeService::on_engine_command_result(service::CommandResult result)
    {
        const auto id = result.id;
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return;
        }

        ResultChannelPtr channel {};
        {
            std::scoped_lock lock {m_pendingMutex};
            const auto it = m_pending.find(id);
            if (it == m_pending.end() || it->second.completed)
            {
                return;
            }
            complete_locked(it->second, result);
            channel = it->second.channel;
        }

        if (channel)
        {
            (void)channel->post(std::move(result));
        }
    }

    DebuggerRuntimeService::ResultChannelPtr
    DebuggerRuntimeService::find_channel_locked(Runtime::CommandId id) const
    {
        const auto it = m_pending.find(id);
        if (it == m_pending.end())
        {
            return {};
        }
        return it->second.channel;
    }

    void DebuggerRuntimeService::complete_locked(PendingResult& pending,
                                                  service::CommandResult /*result*/)
    {
        pending.completed = true;
        pending.evictAfter = std::chrono::steady_clock::now() + COMPLETED_GRACE;
    }

    StatusCode DebuggerRuntimeService::reap_timeouts()
    {
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return STATUS_OK;
        }

        const auto now = std::chrono::steady_clock::now();
        std::vector<std::pair<ResultChannelPtr, service::CommandResult>> firedTimeouts {};
        std::vector<Runtime::CommandId> evicted {};

        {
            std::scoped_lock lock {m_pendingMutex};
            for (auto it = m_pending.begin(); it != m_pending.end();)
            {
                auto& entry = it->second;
                if (entry.completed)
                {
                    if (now >= entry.evictAfter)
                    {
                        evicted.emplace_back(entry.id);
                        it = m_pending.erase(it);
                        continue;
                    }
                }
                else if (now >= entry.deadline)
                {
                    service::CommandResult timeout {.id = entry.id, .code = STATUS_TIMEOUT};
                    complete_locked(entry, timeout);
                    firedTimeouts.emplace_back(entry.channel, std::move(timeout));
                }
                ++it;
            }
        }

        for (auto& [channel, result] : firedTimeouts)
        {
            if (channel)
            {
                (void)channel->post(std::move(result));
            }
        }
        (void)evicted;
        return STATUS_OK;
    }

    void DebuggerRuntimeService::drain_pending_on_shutdown()
    {
        std::vector<std::pair<ResultChannelPtr, service::CommandResult>> firedShutdowns {};

        {
            std::scoped_lock lock {m_pendingMutex};
            for (auto& [id, entry] : m_pending)
            {
                if (!entry.completed)
                {
                    service::CommandResult shutdown {.id = id, .code = STATUS_SHUTDOWN};
                    complete_locked(entry, shutdown);
                    firedShutdowns.emplace_back(entry.channel, std::move(shutdown));
                }
            }
            m_pending.clear();
        }

        for (auto& [channel, result] : firedShutdowns)
        {
            if (channel)
            {
                (void)channel->post(std::move(result));
            }
        }
    }
}
