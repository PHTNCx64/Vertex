//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerengine.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>
#include <vertex/thread/threadchannel.hh>

#include <sdk/feature.h>

#include <algorithm>
#include <chrono>

#include <wx/app.h>

namespace Vertex::Debugger
{
    static constexpr std::uint32_t MAX_COMMAND_BURST {32};

    DebuggerEngine::DebuggerEngine(Runtime::ILoader& loader, Thread::IThreadDispatcher& dispatcher)
        : m_loader(loader),
          m_dispatcher(dispatcher)
    {
    }

    DebuggerEngine::~DebuggerEngine()
    {
        if (m_running.load(std::memory_order_acquire))
        {
            [[maybe_unused]] const auto _ = stop();
        }

        if (m_stopFuture.valid())
        {
            static constexpr auto SHUTDOWN_TIMEOUT = std::chrono::seconds {5};
            m_stopFuture.wait_for(SHUTDOWN_TIMEOUT);
        }
    }

    StatusCode DebuggerEngine::start()
    {
        bool expected {};
        if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return STATUS_ERROR_DEBUGGER_ALREADY_RUNNING;
        }

        auto* plugin = get_plugin();
        if (plugin == nullptr)
        {
            m_running.store(false, std::memory_order_release);
            return STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        m_isSingleThreadMode = m_dispatcher.is_single_threaded();
        m_isDebuggerDependent = plugin->has_feature(VERTEX_FEATURE_DEBUGGER_DEPENDENT);

        ::DebuggerCallbacks callbacks {};
        callbacks.on_breakpoint_hit = on_breakpoint_hit;
        callbacks.on_single_step = on_single_step;
        callbacks.on_exception = on_exception;
        callbacks.on_watchpoint_hit = on_watchpoint_hit;
        callbacks.on_thread_created = on_thread_created;
        callbacks.on_thread_exited = on_thread_exited;
        callbacks.on_module_loaded = on_module_loaded;
        callbacks.on_module_unloaded = on_module_unloaded;
        callbacks.on_process_exited = on_process_exited;
        callbacks.on_output_string = on_output_string;
        callbacks.on_error = on_error;
        callbacks.user_data = this;

        const auto setResult = Runtime::safe_call(
            plugin->internal_vertex_debugger_set_callbacks, &callbacks);
        if (!Runtime::status_ok(setResult))
        {
            m_running.store(false, std::memory_order_release);
            return Runtime::get_status(setResult);
        }

        transition_state(EngineState::Detached);

        auto scheduleResult = m_dispatcher.schedule_recurring(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::High,
            Thread::RecurringPolicy::AsSoonAsPossible,
            std::chrono::milliseconds{0},
            [this]() -> StatusCode
            {
                return tick_once();
            });

        if (!scheduleResult.has_value())
        {
            [[maybe_unused]] const auto clearResult =
                Runtime::safe_call(plugin->internal_vertex_debugger_set_callbacks, nullptr);
            transition_state(EngineState::Idle);
            m_running.store(false, std::memory_order_release);
            return scheduleResult.error();
        }

        m_pumpHandle = *scheduleResult;
        return STATUS_OK;
    }

    StatusCode DebuggerEngine::stop()
    {
        if (!m_running.load(std::memory_order_acquire))
        {
            return STATUS_ERROR_DEBUGGER_NOT_RUNNING;
        }

        m_commandQueue.enqueue(engine::CmdShutdown{});

        [[maybe_unused]] const auto cancelResult =
            m_dispatcher.cancel_recurring(m_pumpHandle);
        m_pumpHandle = {};

        std::packaged_task<StatusCode()> finalTask(
            [this]() -> StatusCode
            {
                drain_all_commands();
                flush_events();
                return STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.dispatch(
            Thread::ThreadChannel::Debugger, std::move(finalTask));
        if (!dispatchResult.has_value()) [[unlikely]]
        {
            m_running.store(false, std::memory_order_release);
            return dispatchResult.error();
        }

        m_stopFuture = std::move(*dispatchResult);
        m_running.store(false, std::memory_order_release);
        return STATUS_OK;
    }

    void DebuggerEngine::send_command(EngineCommand cmd)
    {
        m_commandQueue.enqueue(std::move(cmd));
    }

    EngineSnapshot DebuggerEngine::get_snapshot() const
    {
        std::scoped_lock lock {m_snapshotMutex};
        return m_snapshot;
    }

    std::uint64_t DebuggerEngine::get_generation() const noexcept
    {
        return m_generation.load(std::memory_order_acquire);
    }

    void DebuggerEngine::set_event_callback(EngineEventCallback callback)
    {
        std::scoped_lock lock {m_callbackMutex};
        m_eventCallback = std::move(callback);
    }

    void DebuggerEngine::set_tick_timeout(const std::uint32_t activeMs, const std::uint32_t parkedMs)
    {
        m_activeTimeoutMs = activeMs;
        m_parkedTimeoutMs = parkedMs;
    }

    std::optional<EngineError> DebuggerEngine::consume_last_error()
    {
        std::scoped_lock lock {m_snapshotMutex};
        auto error = std::move(m_lastError);
        m_lastError.reset();
        return error;
    }

    void DebuggerEngine::post_error(const std::string_view operation, const StatusCode code)
    {
        {
            std::scoped_lock lock {m_snapshotMutex};
            m_lastError = EngineError {
                .operation = operation,
                .code = code,
                .stateAtError = m_state.load(std::memory_order_acquire)
            };
        }
        m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
    }

    StatusCode DebuggerEngine::tick_once()
    {
        drain_commands();

        const auto currentState = m_state.load(std::memory_order_acquire);
        if (currentState == EngineState::Detached || currentState == EngineState::Stopped)
        {
            flush_events();
            return STATUS_OK;
        }

        auto* plugin = get_plugin();
        if (plugin == nullptr) [[unlikely]]
        {
            transition_state(EngineState::Detached);
            m_dirtyFlags.fetch_or(static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
            flush_events();
            return STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto timeout = is_parked_state(currentState) ? m_parkedTimeoutMs : m_activeTimeoutMs;
        if (m_isSingleThreadMode && m_isDebuggerDependent)
        {
            timeout = std::min(timeout, m_singleThreadTickClampMs);
        }

        const auto tickResult = Runtime::safe_call(plugin->internal_vertex_debugger_tick, timeout);
        if (tickResult.has_value())
        {
            process_tick_result(*tickResult);
        }
        else
        {
            transition_state(EngineState::Detached);
            m_dirtyFlags.fetch_or(
                static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
            post_error("tick", STATUS_DEBUG_TICK_ERROR);
        }

        flush_events();
        return STATUS_OK;
    }

    void DebuggerEngine::drain_commands()
    {
        auto* plugin = get_plugin();

        EngineCommand cmd {};
        for (std::uint32_t i {}; i < MAX_COMMAND_BURST && m_commandQueue.try_dequeue(cmd); ++i)
        {
            if (plugin == nullptr) [[unlikely]]
            {
                if (std::holds_alternative<engine::CmdShutdown>(cmd))
                {
                    transition_state(EngineState::Stopped);
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
                continue;
            }
            execute_command(plugin, cmd);
        }
    }

    void DebuggerEngine::drain_all_commands()
    {
        auto* plugin = get_plugin();

        EngineCommand cmd {};
        while (m_commandQueue.try_dequeue(cmd))
        {
            if (plugin == nullptr) [[unlikely]]
            {
                if (std::holds_alternative<engine::CmdShutdown>(cmd))
                {
                    transition_state(EngineState::Stopped);
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
                continue;
            }
            execute_command(plugin, cmd);
        }
    }

    void DebuggerEngine::execute_command(Runtime::Plugin* plugin, const EngineCommand& cmd)
    {
        std::visit(
            [this, plugin]<class T>(T&& arg)
            {
                using Cmd = std::decay_t<T>;

                if constexpr (std::is_same_v<Cmd, engine::CmdAttach>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Detached)
                    {
                        post_error("attach", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_attach);
                    if (Runtime::status_ok(result))
                    {
                        transition_state(EngineState::Running);
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("attach", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdDetach>)
                {
                    const auto currentState = m_state.load(std::memory_order_acquire);
                    if (currentState == EngineState::Detached || currentState == EngineState::Idle)
                    {
                        return;
                    }
                    const auto detachResult =
                        Runtime::safe_call(plugin->internal_vertex_debugger_detach);
                    if (!Runtime::status_ok(detachResult))
                    {
                        post_error("detach", Runtime::get_status(detachResult));
                    }
                    transition_state(EngineState::Detached);
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdContinue>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("continue", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_continue, arg.passException);
                    if (Runtime::status_ok(result))
                    {
                        transition_state(EngineState::Running);
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("continue", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdPause>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Running)
                    {
                        post_error("pause", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto pauseResult =
                        Runtime::safe_call(plugin->internal_vertex_debugger_pause);
                    if (!Runtime::status_ok(pauseResult))
                    {
                        post_error("pause", Runtime::get_status(pauseResult));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdStepInto>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("step_into", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_step, VERTEX_STEP_INTO);
                    if (Runtime::status_ok(result))
                    {
                        transition_state(EngineState::Running);
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("step_into", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdStepOver>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("step_over", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_step, VERTEX_STEP_OVER);
                    if (Runtime::status_ok(result))
                    {
                        transition_state(EngineState::Running);
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("step_over", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdStepOut>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("step_out", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_step, VERTEX_STEP_OUT);
                    if (Runtime::status_ok(result))
                    {
                        transition_state(EngineState::Running);
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("step_out", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdRunToAddress>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("run_to_address", STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_run_to_address, arg.address);
                    if (Runtime::status_ok(result))
                    {
                        transition_state(EngineState::Running);
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("run_to_address", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdShutdown>)
                {
                    const auto currentState = m_state.load(std::memory_order_acquire);
                    if (currentState != EngineState::Detached &&
                        currentState != EngineState::Idle &&
                        currentState != EngineState::Stopped)
                    {
                        const auto detachResult =
                            Runtime::safe_call(plugin->internal_vertex_debugger_detach);
                        if (!Runtime::status_ok(detachResult))
                        {
                            post_error("shutdown_detach", Runtime::get_status(detachResult));
                        }
                    }
                    const auto clearResult =
                        Runtime::safe_call(plugin->internal_vertex_debugger_set_callbacks, nullptr);
                    if (!Runtime::status_ok(clearResult))
                    {
                        post_error("shutdown_clear_callbacks", Runtime::get_status(clearResult));
                    }
                    transition_state(EngineState::Stopped);
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
            },
            cmd);
    }

    void DebuggerEngine::process_tick_result(const StatusCode tickResult)
    {
        switch (tickResult)
        {
            case STATUS_DEBUG_TICK_NO_EVENT:
            case STATUS_DEBUG_TICK_PROCESSED:
                break;

            case STATUS_DEBUG_TICK_PAUSED:
            {
                if (transition_state(EngineState::Paused))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
                break;
            }

            case STATUS_DEBUG_TICK_PROCESS_EXITED:
            {
                if (transition_state(EngineState::Exited))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                break;
            }

            case STATUS_DEBUG_TICK_DETACHED:
            {
                if (transition_state(EngineState::Detached))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                break;
            }

            case STATUS_DEBUG_TICK_ERROR:
            default:
            {
                if (transition_state(EngineState::Detached))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                break;
            }
        }
    }

    bool DebuggerEngine::transition_state(const EngineState newState)
    {
        const auto previous = m_state.exchange(newState, std::memory_order_acq_rel);
        if (previous == newState)
        {
            return false;
        }

        const auto gen = m_generation.fetch_add(1, std::memory_order_acq_rel) + 1;

        std::scoped_lock lock {m_snapshotMutex};
        m_snapshot.state = newState;
        m_snapshot.generation = gen;

        if (newState != EngineState::Paused)
        {
            m_snapshot.hasException = false;
            m_snapshot.exceptionCode = 0;
            m_snapshot.exceptionFirstChance = false;
        }

        return true;
    }

    void DebuggerEngine::flush_events()
    {
        const auto rawFlags = m_dirtyFlags.exchange(0, std::memory_order_acq_rel);
        if (rawFlags == 0)
        {
            return;
        }

        m_pendingUiFlags.fetch_or(rawFlags, std::memory_order_acq_rel);

        bool expected {};
        if (!m_uiFlushPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        std::scoped_lock lock {m_callbackMutex};
        if (m_eventCallback && wxTheApp)
        {
            auto callback = m_eventCallback;
            wxTheApp->CallAfter(
                [this, callback]()
                {
                    const auto accumulatedFlags = static_cast<DirtyFlags>(
                        m_pendingUiFlags.exchange(0, std::memory_order_acq_rel));

                    EngineSnapshot snapshot {};
                    {
                        std::scoped_lock snapshotLock {m_snapshotMutex};
                        snapshot = m_snapshot;
                    }

                    m_uiFlushPending.store(false, std::memory_order_release);
                    callback(accumulatedFlags, snapshot);
                });
        }
        else
        {
            m_uiFlushPending.store(false, std::memory_order_release);
        }
    }

    Runtime::Plugin* DebuggerEngine::get_plugin() const
    {
        if (m_loader.has_plugin_loaded() != STATUS_OK)
        {
            return nullptr;
        }

        const auto pluginOpt = m_loader.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return nullptr;
        }

        return &pluginOpt.value().get();
    }

    bool DebuggerEngine::is_parked_state(const EngineState state) noexcept
    {
        return state == EngineState::Paused || state == EngineState::Exited;
    }

    void DebuggerEngine::on_breakpoint_hit(const ::DebugEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->address;
            engine->m_snapshot.currentThreadId = event->threadId;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Breakpoints | DirtyFlags::State),
            std::memory_order_relaxed);
    }

    void DebuggerEngine::on_single_step(const ::DebugEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->address;
            engine->m_snapshot.currentThreadId = event->threadId;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State | DirtyFlags::Registers | DirtyFlags::CallStack),
            std::memory_order_relaxed);
    }

    void DebuggerEngine::on_exception(const ::DebugEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->address;
            engine->m_snapshot.currentThreadId = event->threadId;
            engine->m_snapshot.exceptionCode = event->exceptionCode;
            engine->m_snapshot.exceptionFirstChance = event->firstChance != 0;
            engine->m_snapshot.hasException = true;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_watchpoint_hit(const ::WatchpointEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->accessAddress;
            engine->m_snapshot.currentThreadId = event->threadId;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Watchpoints | DirtyFlags::State),
            std::memory_order_relaxed);
    }

    void DebuggerEngine::on_thread_created([[maybe_unused]] const ::ThreadEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Threads), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_thread_exited([[maybe_unused]] const ::ThreadEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Threads), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_module_loaded([[maybe_unused]] const ::ModuleEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Modules), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_module_unloaded([[maybe_unused]] const ::ModuleEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Modules), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_process_exited([[maybe_unused]] const std::int32_t exitCode, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_output_string([[maybe_unused]] const ::OutputStringEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_error([[maybe_unused]] const ::DebuggerError* error, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
    }
}
