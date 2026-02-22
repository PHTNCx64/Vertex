//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerworker.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>

#include <chrono>

namespace Vertex::Debugger
{
    namespace
    {
        class CallbackGuard final
        {
          public:
            explicit CallbackGuard(void* userData)
            {
                m_context = CallbackContextRegistry::instance().lookup(userData);
                if (m_context && m_context->valid.load(std::memory_order_acquire))
                {
                    m_worker = m_context->worker.load(std::memory_order_acquire);
                    if (m_worker != nullptr)
                    {
                        m_worker->increment_callback_count();
                    }
                }
            }

            ~CallbackGuard()
            {
                if (m_worker != nullptr)
                {
                    m_worker->decrement_callback_count();
                }
            }

            CallbackGuard(const CallbackGuard&) = delete;
            CallbackGuard& operator=(const CallbackGuard&) = delete;

            [[nodiscard]] DebuggerWorker* get() const
            {
                return m_worker;
            }

            [[nodiscard]] explicit operator bool() const
            {
                return m_worker != nullptr;
            }

          private:
            std::shared_ptr<CallbackContext> m_context{};
            DebuggerWorker* m_worker{};
        };

        void on_attached(const uint32_t processId, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard)
            {
                guard.get()->handle_attached(processId);
            }
        }

        void on_detached(const uint32_t processId, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard)
            {
                guard.get()->handle_detached(processId);
            }
        }

        void on_state_changed(const ::DebuggerState oldState, const ::DebuggerState newState, void* userData)
        {
            CallbackGuard guard{userData};
            if (guard)
            {
                guard.get()->handle_state_changed(oldState, newState);
            }
        }

        void on_error(const ::DebuggerError* error, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard && error != nullptr)
            {
                guard.get()->handle_error(error->code, error->message);
            }
        }

        void on_breakpoint_hit(const ::DebugEvent* event, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard && event != nullptr)
            {
                guard.get()->handle_breakpoint_hit(event);
            }
        }

        void on_single_step(const ::DebugEvent* event, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard && event != nullptr)
            {
                guard.get()->handle_single_step(event);
            }
        }

        void on_exception(const ::DebugEvent* event, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard && event != nullptr)
            {
                guard.get()->handle_exception(event);
            }
        }

        void on_watchpoint_hit(const ::WatchpointEvent* event, void* userData)
        {
            const CallbackGuard guard{userData};
            if (guard && event != nullptr)
            {
                guard.get()->handle_watchpoint_hit(event);
            }
        }
    }

    DebuggerWorker::DebuggerWorker(Runtime::ILoader& loaderService, Thread::IThreadDispatcher& dispatcher)
        : m_loaderService(loaderService),
          m_dispatcher(dispatcher)
    {
    }

    DebuggerWorker::~DebuggerWorker()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            stop();
        }
    }

    StatusCode DebuggerWorker::start()
    {
        bool expected{};
        if (!m_isRunning.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        Runtime::Plugin* plugin = get_plugin();
        if (plugin == nullptr)
        {
            m_isRunning.store(false, std::memory_order_release);
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        if (plugin->internal_vertex_debugger_run == nullptr)
        {
            m_isRunning.store(false, std::memory_order_release);
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }

        m_callbackContext = std::make_shared<CallbackContext>();
        m_callbackContext->worker.store(this, std::memory_order_release);
        m_callbackContext->valid.store(true, std::memory_order_release);
        CallbackContextRegistry::instance().register_context(m_callbackContext.get(), m_callbackContext);

        ::DebuggerCallbacks callbacks{};
        callbacks.on_attached = on_attached;
        callbacks.on_detached = on_detached;
        callbacks.on_state_changed = on_state_changed;
        callbacks.on_breakpoint_hit = on_breakpoint_hit;
        callbacks.on_single_step = on_single_step;
        callbacks.on_exception = on_exception;
        callbacks.on_watchpoint_hit = on_watchpoint_hit;
        callbacks.on_error = on_error;
        callbacks.user_data = m_callbackContext.get();

        const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_run, &callbacks);
        if (!Runtime::status_ok(result))
        {
            const StatusCode status = Runtime::get_status(result);
            m_callbackContext->valid.store(false, std::memory_order_release);
            m_callbackContext->worker.store(nullptr, std::memory_order_release);
            CallbackContextRegistry::instance().unregister_context(m_callbackContext.get());
            m_callbackContext.reset();
            m_isRunning.store(false, std::memory_order_release);
            return status;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerWorker::stop()
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        m_stopping.store(true, std::memory_order_release);

        if (m_callbackContext != nullptr)
        {
            m_callbackContext->valid.store(false, std::memory_order_release);
            m_callbackContext->worker.store(nullptr, std::memory_order_release);
        }

        const Runtime::Plugin* plugin = get_plugin();
        if (plugin != nullptr)
        {
            if (m_attached.load(std::memory_order_acquire))
            {
                const auto detachResult = Runtime::safe_call(plugin->internal_vertex_debugger_detach);
                if (!Runtime::status_ok(detachResult))
                {
                    post_error(Runtime::get_status(detachResult), "Failed to detach during stop");
                }
            }

            const auto stopResult = Runtime::safe_call(plugin->internal_vertex_debugger_request_stop);
            if (!Runtime::status_ok(stopResult))
            {
                post_error(Runtime::get_status(stopResult), "Failed to request stop");
            }
        }

        wait_for_callbacks_to_drain();

        if (m_callbackContext != nullptr)
        {
            CallbackContextRegistry::instance().unregister_context(m_callbackContext.get());
            m_callbackContext.reset();
        }

        m_isRunning.store(false, std::memory_order_release);
        m_attached.store(false, std::memory_order_release);
        m_state.store(DebuggerState::Detached, std::memory_order_release);
        m_stopping.store(false, std::memory_order_release);
        m_currentAddress.store(0, std::memory_order_release);
        m_currentThreadId.store(0, std::memory_order_release);

        return StatusCode::STATUS_OK;
    }

    void DebuggerWorker::increment_callback_count()
    {
        m_callbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
    }

    void DebuggerWorker::decrement_callback_count()
    {
        if (m_callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            std::scoped_lock lock{m_drainMutex};
            m_drainCondition.notify_all();
        }
    }

    void DebuggerWorker::wait_for_callbacks_to_drain()
    {
        constexpr auto DRAIN_TIMEOUT = std::chrono::seconds{5};

        std::unique_lock lock{m_drainMutex};
        m_drainCondition.wait_for(lock, DRAIN_TIMEOUT,
                                  [this]
                                  {
                                      return m_callbacksInFlight.load(std::memory_order_acquire) == 0;
                                  });
    }

    void DebuggerWorker::set_event_callback(DebuggerEventCallback callback)
    {
        std::scoped_lock lock(m_callbackMutex);
        m_eventCallback = std::move(callback);
    }

    DebuggerState DebuggerWorker::get_state() const noexcept
    {
        return m_state.load(std::memory_order_acquire);
    }

    bool DebuggerWorker::is_running() const noexcept
    {
        return m_isRunning.load(std::memory_order_acquire);
    }

    Runtime::Plugin* DebuggerWorker::get_plugin() const
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return nullptr;
        }

        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return nullptr;
        }

        return &pluginOpt.value().get();
    }
}
