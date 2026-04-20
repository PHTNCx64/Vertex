//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scripting/scriptingruntimeservice.hh>

#include <fmt/format.h>

#include <utility>
#include <vector>

namespace Vertex::Scripting
{
    ScriptingRuntimeService::ScriptingRuntimeService(IAngelScript& scriptEngine,
                                                       Thread::IThreadDispatcher& dispatcher,
                                                       Log::ILog& log)
        : m_engine{scriptEngine}
        , m_dispatcher{dispatcher}
        , m_log{log}
    {
        const auto handle = m_dispatcher.schedule_recurring_persistent(
            Thread::ThreadChannel::Script,
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

        m_log.log_error(fmt::format("ScriptingRuntimeService: persistent timeout reaper schedule failed (status={})",
                                     static_cast<int>(handle.error())));
    }

    ScriptingRuntimeService::~ScriptingRuntimeService()
    {
        shutdown();
    }

    Runtime::CommandId ScriptingRuntimeService::allocate_command_id()
    {
        return m_nextCommandId.fetch_add(1, std::memory_order_relaxed);
    }

    ScriptingRuntimeService::ResultChannelPtr
    ScriptingRuntimeService::register_pending(Runtime::CommandId id,
                                                std::chrono::milliseconds timeout,
                                                std::string moduleName)
    {
        const auto now = std::chrono::steady_clock::now();
        auto channel = std::make_shared<Runtime::ResultChannel<engine::CommandResult>>();
        PendingResult pending{.id = id,
                              .deadline = now + timeout,
                              .evictAfter = {},
                              .channel = channel,
                              .moduleName = std::move(moduleName),
                              .completed = false};

        std::scoped_lock lock{m_pendingMutex};
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return {};
        }
        m_pending.emplace(id, std::move(pending));
        return channel;
    }

    void ScriptingRuntimeService::synthesise_rejection(Runtime::CommandId id, StatusCode code, std::string moduleName)
    {
        post_result(id, code, std::move(moduleName));
    }

    void ScriptingRuntimeService::post_result(Runtime::CommandId id, StatusCode code, std::string moduleName)
    {
        engine::CommandResult result{.id = id, .code = code, .moduleName = std::move(moduleName)};
        on_scripting_command_result(std::move(result));
    }

    Runtime::CommandId
    ScriptingRuntimeService::send_command(engine::ScriptingCommand command, std::chrono::milliseconds timeout)
    {
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        return std::visit(
            [&]<class T>(T&& cmd) -> Runtime::CommandId
            {
                using D = std::decay_t<T>;
                if constexpr (std::is_same_v<D, engine::CmdExecute>)
                {
                    return dispatch_execute(std::forward<T>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<D, engine::CmdStop>)
                {
                    return dispatch_stop(std::forward<T>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<D, engine::CmdReload>)
                {
                    return dispatch_reload(std::forward<T>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<D, engine::CmdEvaluate>)
                {
                    return dispatch_evaluate(std::forward<T>(cmd), timeout);
                }
                else
                {
                    static_assert(!sizeof(D*), "unhandled scripting command");
                }
            },
            std::move(command));
    }

    Runtime::CommandId
    ScriptingRuntimeService::dispatch_execute(engine::CmdExecute command, std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        const auto moduleName = command.moduleName;
        if (!register_pending(id, timeout, moduleName))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        {
            std::scoped_lock lock{m_activeMutex};
            if (m_activeByModule.contains(moduleName))
            {
                synthesise_rejection(id, STATUS_ERROR_THREAD_IS_BUSY, moduleName);
                return id;
            }
            m_activeByModule[moduleName] = id;
        }

        post_result(id, STATUS_OK, moduleName);
        return id;
    }

    Runtime::CommandId
    ScriptingRuntimeService::dispatch_stop(engine::CmdStop command, std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        const auto moduleName = command.moduleName;
        if (!register_pending(id, timeout, moduleName))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        Runtime::CommandId activeId{Runtime::INVALID_COMMAND_ID};
        std::string activeModule{};
        {
            std::scoped_lock lock{m_activeMutex};
            auto it = m_activeByModule.find(moduleName);
            if (it == m_activeByModule.end())
            {
                synthesise_rejection(id, STATUS_ERROR_GENERAL_NOT_FOUND, moduleName);
                return id;
            }
            activeId = it->second;
            activeModule = it->first;
            m_activeByModule.erase(it);
        }

        ScriptingEvent complete{
            .kind = ScriptingEventKind::ScriptComplete,
            .detail = ScriptCompleteInfo{.moduleName = activeModule, .code = STATUS_CANCELED}};
        m_fanout.fire(complete);

        on_scripting_command_result(
            engine::CommandResult{.id = activeId, .code = STATUS_CANCELED, .moduleName = activeModule});

        post_result(id, STATUS_OK, moduleName);
        return id;
    }

    Runtime::CommandId
    ScriptingRuntimeService::dispatch_reload(engine::CmdReload command, std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        const auto moduleName = command.moduleName;
        if (!register_pending(id, timeout, moduleName))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        ScriptingEvent event{
            .kind = ScriptingEventKind::ModuleLoaded,
            .detail = ModuleInfo{.moduleName = moduleName, .path = {}}};
        m_fanout.fire(event);

        post_result(id, STATUS_OK, moduleName);
        return id;
    }

    Runtime::CommandId
    ScriptingRuntimeService::dispatch_evaluate(engine::CmdEvaluate command, std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        const auto moduleName = command.moduleName;
        if (!register_pending(id, timeout, moduleName))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        post_result(id, STATUS_ERROR_NOT_IMPLEMENTED, moduleName);
        return id;
    }

    void ScriptingRuntimeService::subscribe_result(Runtime::CommandId id, ResultCallback callback)
    {
        if (id == Runtime::INVALID_COMMAND_ID || !callback) return;

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            channel = find_channel_locked(id);
        }

        if (!channel)
        {
            engine::CommandResult timeout{.id = id, .code = STATUS_TIMEOUT};
            callback(timeout);
            return;
        }

        channel->on_result([cb = std::move(callback)](const engine::CommandResult& result) { cb(result); });
    }

    engine::CommandResult
    ScriptingRuntimeService::await_result(Runtime::CommandId id, std::chrono::milliseconds timeout)
    {
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return engine::CommandResult{.id = id, .code = STATUS_ERROR_INVALID_PARAMETER};
        }

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            channel = find_channel_locked(id);
        }

        if (!channel)
        {
            return engine::CommandResult{.id = id, .code = STATUS_TIMEOUT};
        }

        if (!channel->wait_for(timeout))
        {
            return engine::CommandResult{.id = id, .code = STATUS_TIMEOUT};
        }

        auto result = channel->copy_result();
        if (result.has_value())
        {
            return std::move(*result);
        }
        return engine::CommandResult{.id = id, .code = STATUS_TIMEOUT};
    }

    Runtime::SubscriptionId
    ScriptingRuntimeService::subscribe(ScriptingEventKindMask mask, EventCallback callback)
    {
        return m_fanout.subscribe(mask, std::move(callback));
    }

    void ScriptingRuntimeService::unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept
    {
        m_fanout.unsubscribe(subscriptionId);
    }

    void ScriptingRuntimeService::on_scripting_event(ScriptingEvent event)
    {
        if (event.kind == ScriptingEventKind::ScriptComplete)
        {
            const auto* info = std::get_if<ScriptCompleteInfo>(&event.detail);
            if (info)
            {
                std::scoped_lock lock{m_activeMutex};
                m_activeByModule.erase(info->moduleName);
            }
        }
        m_fanout.fire(event);
    }

    void ScriptingRuntimeService::on_scripting_command_result(engine::CommandResult result)
    {
        const auto id = result.id;
        if (id == Runtime::INVALID_COMMAND_ID) return;

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            const auto it = m_pending.find(id);
            if (it == m_pending.end() || it->second.completed) return;
            complete_locked(it->second);
            channel = it->second.channel;
        }

        if (channel)
        {
            (void) channel->post(std::move(result));
        }
    }

    void ScriptingRuntimeService::shutdown()
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
                m_log.log_warn("ScriptingRuntimeService: cancel_recurring failed");
            }
            m_reaperActive = false;
        }

        m_fanout.shutdown();
        drain_pending_on_shutdown();
    }

    ScriptingRuntimeService::ResultChannelPtr
    ScriptingRuntimeService::find_channel_locked(Runtime::CommandId id) const
    {
        const auto it = m_pending.find(id);
        if (it == m_pending.end()) return {};
        return it->second.channel;
    }

    void ScriptingRuntimeService::complete_locked(PendingResult& pending)
    {
        pending.completed = true;
        pending.evictAfter = std::chrono::steady_clock::now() + COMPLETED_GRACE;
    }

    StatusCode ScriptingRuntimeService::reap_timeouts()
    {
        if (m_shuttingDown.load(std::memory_order_acquire)) return STATUS_OK;

        const auto now = std::chrono::steady_clock::now();
        std::vector<std::pair<ResultChannelPtr, engine::CommandResult>> fired{};

        {
            std::scoped_lock lock{m_pendingMutex};
            for (auto it = m_pending.begin(); it != m_pending.end();)
            {
                auto& entry = it->second;
                if (entry.completed)
                {
                    if (now >= entry.evictAfter)
                    {
                        it = m_pending.erase(it);
                        continue;
                    }
                }
                else if (now >= entry.deadline)
                {
                    engine::CommandResult timeout{.id = entry.id, .code = STATUS_TIMEOUT, .moduleName = entry.moduleName};
                    complete_locked(entry);
                    fired.emplace_back(entry.channel, std::move(timeout));
                }
                ++it;
            }
        }

        for (auto& [channel, result] : fired)
        {
            if (channel) (void) channel->post(std::move(result));
        }
        return STATUS_OK;
    }

    void ScriptingRuntimeService::drain_pending_on_shutdown()
    {
        std::vector<std::pair<ResultChannelPtr, engine::CommandResult>> fired{};

        {
            std::scoped_lock lock{m_pendingMutex};
            for (auto& [id, entry] : m_pending)
            {
                if (!entry.completed)
                {
                    engine::CommandResult shutdownResult{.id = id, .code = STATUS_SHUTDOWN, .moduleName = entry.moduleName};
                    complete_locked(entry);
                    fired.emplace_back(entry.channel, std::move(shutdownResult));
                }
            }
            m_pending.clear();
        }

        for (auto& [channel, result] : fired)
        {
            if (channel) (void) channel->post(std::move(result));
        }
    }
}
