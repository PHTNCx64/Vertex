//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <sdk/debugger.h>
#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <variant>

#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace Vertex::Debugger
{
    enum class EngineState : std::uint8_t
    {
        Idle,
        Detached,
        Running,
        Paused,
        Exited,
        Stopped
    };

    struct EngineSnapshot final
    {
        EngineState state {};
        std::uint64_t currentAddress {};
        std::uint32_t currentThreadId {};
        std::uint64_t generation {};
        std::uint32_t exceptionCode {};
        bool exceptionFirstChance {};
        bool hasException {};
    };

    enum class DirtyFlags : std::uint32_t
    {
        None            = 0,
        State           = 1 << 0,
        Modules         = 1 << 1,
        Threads         = 1 << 2,
        Breakpoints     = 1 << 3,
        Registers       = 1 << 4,
        CallStack       = 1 << 5,
        Watchpoints     = 1 << 6,
        ImportsExports  = 1 << 7,
        Disassembly     = 1 << 8,
        All             = 0xFFFFFFFF
    };

    [[nodiscard]] constexpr DirtyFlags operator|(DirtyFlags lhs, DirtyFlags rhs) noexcept
    {
        return static_cast<DirtyFlags>(
            static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
    }

    constexpr DirtyFlags& operator|=(DirtyFlags& lhs, DirtyFlags rhs) noexcept
    {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr DirtyFlags operator&(DirtyFlags lhs, DirtyFlags rhs) noexcept
    {
        return static_cast<DirtyFlags>(
            static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
    }

    namespace engine
    {
        struct CmdAttach {};
        struct CmdDetach {};
        struct CmdContinue { std::uint8_t passException {}; };
        struct CmdPause {};
        struct CmdStepInto {};
        struct CmdStepOver {};
        struct CmdStepOut {};
        struct CmdRunToAddress { std::uint64_t address {}; };
        struct CmdShutdown {};
    }

    using EngineCommand = std::variant<
        engine::CmdAttach,
        engine::CmdDetach,
        engine::CmdContinue,
        engine::CmdPause,
        engine::CmdStepInto,
        engine::CmdStepOver,
        engine::CmdStepOut,
        engine::CmdRunToAddress,
        engine::CmdShutdown
    >;

    using EngineEventCallback = std::function<void(DirtyFlags flags, const EngineSnapshot& snapshot)>;

    struct EngineError final
    {
        std::string_view operation {};
        StatusCode code {};
        EngineState stateAtError {};
    };

    class DebuggerEngine final
    {
    public:
        explicit DebuggerEngine(Runtime::ILoader& loader, Thread::IThreadDispatcher& dispatcher);
        ~DebuggerEngine();

        DebuggerEngine(const DebuggerEngine&) = delete;
        DebuggerEngine& operator=(const DebuggerEngine&) = delete;
        DebuggerEngine(DebuggerEngine&&) = delete;
        DebuggerEngine& operator=(DebuggerEngine&&) = delete;

        [[nodiscard]] StatusCode start();
        [[nodiscard]] StatusCode stop();

        void send_command(EngineCommand cmd);

        [[nodiscard]] EngineSnapshot get_snapshot() const;
        [[nodiscard]] std::uint64_t get_generation() const noexcept;

        void set_event_callback(EngineEventCallback callback);
        void set_tick_timeout(std::uint32_t activeMs, std::uint32_t parkedMs);

        [[nodiscard]] std::optional<EngineError> consume_last_error();

    private:
        [[nodiscard]] StatusCode tick_once();

        void drain_commands();
        void drain_all_commands();
        void execute_command(Runtime::Plugin* plugin, const EngineCommand& cmd);
        void process_tick_result(StatusCode tickResult);
        [[nodiscard]] bool transition_state(EngineState newState);
        void flush_events();
        void post_error(std::string_view operation, StatusCode code);

        [[nodiscard]] Runtime::Plugin* get_plugin() const;
        [[nodiscard]] static bool is_parked_state(EngineState state) noexcept;

        static void on_breakpoint_hit(const ::DebugEvent* event, void* userData);
        static void on_single_step(const ::DebugEvent* event, void* userData);
        static void on_exception(const ::DebugEvent* event, void* userData);
        static void on_watchpoint_hit(const ::WatchpointEvent* event, void* userData);
        static void on_thread_created(const ::ThreadEvent* event, void* userData);
        static void on_thread_exited(const ::ThreadEvent* event, void* userData);
        static void on_module_loaded(const ::ModuleEvent* event, void* userData);
        static void on_module_unloaded(const ::ModuleEvent* event, void* userData);
        static void on_process_exited(std::int32_t exitCode, void* userData);
        static void on_output_string(const ::OutputStringEvent* event, void* userData);
        static void on_error(const ::DebuggerError* error, void* userData);

        Runtime::ILoader& m_loader;
        Thread::IThreadDispatcher& m_dispatcher;

        std::atomic<bool> m_running {};
        std::uint32_t m_singleThreadTickClampMs {50};

        std::atomic<EngineState> m_state {EngineState::Idle};
        std::atomic<std::uint64_t> m_generation {};
        mutable std::mutex m_snapshotMutex {};
        EngineSnapshot m_snapshot {};

        moodycamel::ConcurrentQueue<EngineCommand> m_commandQueue {};

        std::atomic<std::uint32_t> m_dirtyFlags {};
        std::atomic<std::uint32_t> m_pendingUiFlags {};
        std::atomic<bool> m_uiFlushPending {};

        std::uint32_t m_activeTimeoutMs {100};
        std::uint32_t m_parkedTimeoutMs {500};

        EngineEventCallback m_eventCallback {};
        std::mutex m_callbackMutex {};
        std::optional<EngineError> m_lastError {};

        Thread::RecurringTaskHandle m_pumpHandle {};
        std::future<StatusCode> m_stopFuture {};
        bool m_isSingleThreadMode {};
        bool m_isDebuggerDependent {};
    };
}
