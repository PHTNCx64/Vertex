//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <sdk/debugger.h>
#include <vertex/runtime/iloader.hh>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <variant>

namespace Vertex::Debugger
{
    class DebuggerWorker;

    struct CallbackContext
    {
        std::atomic<DebuggerWorker*> worker {};
        std::atomic<bool> valid {};
    };

    class CallbackContextRegistry final
    {
    public:
        [[nodiscard]] static CallbackContextRegistry& instance();

        void register_context(void* key, std::weak_ptr<CallbackContext> context);
        void unregister_context(void* key);
        [[nodiscard]] std::shared_ptr<CallbackContext> lookup(void* key);

        CallbackContextRegistry(const CallbackContextRegistry&) = delete;
        CallbackContextRegistry& operator=(const CallbackContextRegistry&) = delete;

    private:
        CallbackContextRegistry() = default;

        mutable std::shared_mutex m_mutex {};
        std::unordered_map<void*, std::weak_ptr<CallbackContext>> m_registry {};
    };

    struct CmdAttach {};
    struct CmdDetach {};
    struct CmdContinue { std::uint8_t passException {}; };
    struct CmdPause {};
    struct CmdStepInto {};
    struct CmdStepOver {};
    struct CmdStepOut {};
    struct CmdRunToAddress { std::uint64_t address {}; };
    struct CmdShutdown {};

    using DebuggerCommand = std::variant<
        CmdAttach,
        CmdDetach,
        CmdContinue,
        CmdPause,
        CmdStepInto,
        CmdStepOver,
        CmdStepOut,
        CmdRunToAddress,
        CmdShutdown
    >;

    struct DebuggerSnapshot
    {
        DebuggerState state {DebuggerState::Detached};
        std::uint64_t currentAddress {};
        std::uint32_t currentThreadId {};
    };

    struct EvtStateChanged { DebuggerSnapshot snapshot; };
    struct EvtLog { std::string message; };
    struct EvtError { StatusCode code; std::string message; };
    struct EvtAttachFailed { StatusCode code; std::string message; };

    struct EvtBreakpointHit
    {
        std::uint32_t breakpointId {};
        std::uint32_t threadId {};
        std::uint64_t address {};
    };

    struct EvtWatchpointHit
    {
        std::uint32_t watchpointId {};
        std::uint32_t threadId {};
        std::uint64_t accessorAddress {};
    };

    using DebuggerEvent = std::variant<EvtStateChanged, EvtLog, EvtError, EvtAttachFailed, EvtBreakpointHit, EvtWatchpointHit>;
    using DebuggerEventCallback = std::function<void(const DebuggerEvent&)>;

    class DebuggerWorker final
    {
    public:
        explicit DebuggerWorker(Runtime::ILoader& loaderService, Thread::IThreadDispatcher& dispatcher);
        ~DebuggerWorker();

        DebuggerWorker(const DebuggerWorker&) = delete;
        DebuggerWorker& operator=(const DebuggerWorker&) = delete;
        DebuggerWorker(DebuggerWorker&&) = delete;
        DebuggerWorker& operator=(DebuggerWorker&&) = delete;

        [[nodiscard]] StatusCode start();
        [[nodiscard]] StatusCode stop();

        void send_command(DebuggerCommand cmd);
        void set_event_callback(DebuggerEventCallback callback);

        [[nodiscard]] DebuggerState get_state() const noexcept;
        [[nodiscard]] bool is_running() const noexcept;

        void handle_attached(std::uint32_t processId);
        void handle_detached(std::uint32_t processId);
        void handle_state_changed(::DebuggerState oldState, ::DebuggerState newState);
        void handle_breakpoint_hit(const ::DebugEvent* event);
        void handle_single_step(const ::DebugEvent* event);
        void handle_exception(const ::DebugEvent* event);
        void handle_watchpoint_hit(const ::WatchpointEvent* event) const;
        void handle_error(StatusCode code, const char* message) const;
        void increment_callback_count();
        void decrement_callback_count();

    private:
        void post_event(DebuggerEvent evt) const;
        void post_state_changed() const;
        void post_log(std::string_view message) const;
        void post_error(StatusCode code, std::string_view message) const;

        [[nodiscard]] Runtime::Plugin* get_plugin() const;

        [[nodiscard]] bool is_valid_command_for_state(const DebuggerCommand& cmd) const;
        [[nodiscard]] StatusCode execute_command(Runtime::Plugin* plugin, const DebuggerCommand& cmd);
        void wait_for_callbacks_to_drain();

        Runtime::ILoader& m_loaderService;
        Thread::IThreadDispatcher& m_dispatcher;

        std::atomic<bool> m_isRunning {};
        std::atomic<DebuggerState> m_state {DebuggerState::Detached};
        std::atomic<bool> m_attached {};
        std::atomic<bool> m_stopping {};

        std::atomic<std::uint64_t> m_currentAddress {};
        std::atomic<std::uint32_t> m_currentThreadId {};

        std::atomic<std::uint32_t> m_callbacksInFlight {};
        mutable std::mutex m_drainMutex {};
        std::condition_variable m_drainCondition {};

        DebuggerEventCallback m_eventCallback {};
        mutable std::mutex m_callbackMutex {};

        std::shared_ptr<CallbackContext> m_callbackContext {};
    };

}
