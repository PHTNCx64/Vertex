//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerworker.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>

#include <wx/app.h>
#include <format>

namespace Vertex::Debugger
{
    void DebuggerWorker::handle_attached(const uint32_t processId)
    {
        m_attached.store(true, std::memory_order_release);
        post_log(std::format("Debugger attached to process {}", processId));
    }

    void DebuggerWorker::handle_detached(const uint32_t processId)
    {
        m_attached.store(false, std::memory_order_release);
        m_currentAddress.store(0, std::memory_order_release);
        m_currentThreadId.store(0, std::memory_order_release);
        post_log(std::format("Debugger detached from process {}", processId));
    }

    void DebuggerWorker::handle_state_changed([[maybe_unused]] const ::DebuggerState oldState, const ::DebuggerState newState)
    {
        DebuggerState internalState{};
        switch (newState)
        {
        case VERTEX_DBG_STATE_DETACHED: internalState = DebuggerState::Detached; break;
        case VERTEX_DBG_STATE_ATTACHED: internalState = DebuggerState::Attached; break;
        case VERTEX_DBG_STATE_RUNNING: internalState = DebuggerState::Running; break;
        case VERTEX_DBG_STATE_PAUSED: internalState = DebuggerState::Paused; break;
        case VERTEX_DBG_STATE_STEPPING: internalState = DebuggerState::Stepping; break;
        case VERTEX_DBG_STATE_BREAKPOINT_HIT: internalState = DebuggerState::BreakpointHit; break;
        case VERTEX_DBG_STATE_EXCEPTION: internalState = DebuggerState::Exception; break;
        default:
            post_error(StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE,
                       std::format("Unknown debugger state received from plugin: {}", static_cast<int>(newState)));
            return;
        }

        if (m_state.load(std::memory_order_acquire) == internalState)
        {
            return;
        }

        m_state.store(internalState, std::memory_order_release);

        const bool isPausedState = internalState == DebuggerState::Paused || internalState == DebuggerState::BreakpointHit || internalState == DebuggerState::Stepping || internalState == DebuggerState::Exception;

        if (isPausedState && m_currentThreadId.load(std::memory_order_acquire) == 0 && m_attached.load(std::memory_order_acquire))
        {
            const Runtime::Plugin* plugin = get_plugin();
            if (plugin != nullptr)
            {
                std::uint32_t tid{};
                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_get_current_thread, &tid);
                if (Runtime::status_ok(result) && tid != 0)
                {
                    m_currentThreadId.store(tid, std::memory_order_release);
                }
                else if (!Runtime::status_ok(result))
                {
                    post_error(Runtime::get_status(result), "Failed to get current thread ID");
                }
            }
        }

        post_state_changed();
    }

    void DebuggerWorker::handle_error(const StatusCode code, const char* message) const
    {
        post_error(code, message);
    }

    void DebuggerWorker::handle_breakpoint_hit(const ::DebugEvent* event)
    {
        m_currentAddress.store(event->address, std::memory_order_release);
        m_currentThreadId.store(event->threadId, std::memory_order_release);

        EvtBreakpointHit evt{};
        evt.breakpointId = event->breakpointId;
        evt.threadId = event->threadId;
        evt.address = event->address;
        post_event(evt);
    }

    void DebuggerWorker::handle_single_step(const ::DebugEvent* event)
    {
        m_currentAddress.store(event->address, std::memory_order_release);
        m_currentThreadId.store(event->threadId, std::memory_order_release);
    }

    void DebuggerWorker::handle_exception(const ::DebugEvent* event)
    {
        m_currentAddress.store(event->address, std::memory_order_release);
        m_currentThreadId.store(event->threadId, std::memory_order_release);
    }

    void DebuggerWorker::handle_watchpoint_hit(const ::WatchpointEvent* event) const
    {
        EvtWatchpointHit evt{};
        evt.watchpointId = event->breakpointId;
        evt.threadId = event->threadId;
        evt.accessorAddress = event->accessAddress;
        post_event(evt);
    }

    void DebuggerWorker::post_event(DebuggerEvent evt) const
    {
        std::scoped_lock lock(m_callbackMutex);
        if (m_eventCallback && wxTheApp)
        {
            auto callback = m_eventCallback;
            wxTheApp->CallAfter(
              [callback, evt = std::move(evt)]()
              {
                  callback(evt);
              });
        }
    }

    void DebuggerWorker::post_state_changed() const
    {
        DebuggerSnapshot snapshot{};
        snapshot.state = m_state.load(std::memory_order_acquire);
        snapshot.currentAddress = m_currentAddress.load(std::memory_order_acquire);
        snapshot.currentThreadId = m_currentThreadId.load(std::memory_order_acquire);
        post_event(EvtStateChanged{snapshot});
    }

    void DebuggerWorker::post_log(const std::string_view message) const
    {
        post_event(EvtLog{std::string(message)});
    }

    void DebuggerWorker::post_error(const StatusCode code, const std::string_view message) const
    {
        post_event(EvtError{code, std::string(message)});
    }
}
