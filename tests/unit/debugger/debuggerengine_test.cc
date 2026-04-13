#include <gtest/gtest.h>
#include <vertex/debugger/debuggerengine.hh>
#include <vertex/runtime/plugin.hh>
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <chrono>
#include <expected>
#include <memory>
#include <thread>

using namespace Vertex::Debugger;
using namespace Vertex::Testing::Mocks;
using namespace Vertex::Thread;
using namespace testing;

namespace
{
    StatusCode VERTEX_API stub_set_callbacks(const DebuggerCallbacks*) { return STATUS_OK; }
    StatusCode VERTEX_API stub_set_callbacks_fail(const DebuggerCallbacks*) { return STATUS_ERROR_GENERAL; }
    StatusCode VERTEX_API stub_tick(uint32_t) { return STATUS_DEBUG_TICK_NO_EVENT; }
    StatusCode VERTEX_API stub_attach() { return STATUS_OK; }
    StatusCode VERTEX_API stub_attach_fail() { return STATUS_ERROR_DEBUGGER_ATTACH_FAILED; }
    StatusCode VERTEX_API stub_detach() { return STATUS_OK; }
    StatusCode VERTEX_API stub_detach_fail() { return STATUS_ERROR_GENERAL; }
    StatusCode VERTEX_API stub_set_callbacks_clear_fail(const DebuggerCallbacks* cb)
    {
        if (cb == nullptr) { return STATUS_ERROR_GENERAL; }
        return STATUS_OK;
    }
    StatusCode VERTEX_API stub_continue(uint8_t) { return STATUS_OK; }
    StatusCode VERTEX_API stub_continue_fail(uint8_t) { return STATUS_ERROR_GENERAL; }
    StatusCode VERTEX_API stub_pause() { return STATUS_OK; }
    StatusCode VERTEX_API stub_pause_fail() { return STATUS_ERROR_GENERAL; }
    StatusCode VERTEX_API stub_step(::StepMode) { return STATUS_OK; }
    StatusCode VERTEX_API stub_step_fail(::StepMode) { return STATUS_ERROR_GENERAL; }
    StatusCode VERTEX_API stub_run_to_address(uint64_t) { return STATUS_OK; }
    StatusCode VERTEX_API stub_run_to_address_fail(uint64_t) { return STATUS_ERROR_GENERAL; }
    StatusCode VERTEX_API stub_tick_paused(uint32_t) { return STATUS_DEBUG_TICK_PAUSED; }
    StatusCode VERTEX_API stub_tick_processed(uint32_t) { return STATUS_DEBUG_TICK_PROCESSED; }
    StatusCode VERTEX_API stub_tick_exited(uint32_t) { return STATUS_DEBUG_TICK_PROCESS_EXITED; }
    StatusCode VERTEX_API stub_tick_detached(uint32_t) { return STATUS_DEBUG_TICK_DETACHED; }
    StatusCode VERTEX_API stub_tick_error(uint32_t) { return STATUS_DEBUG_TICK_ERROR; }

    struct TestCtx final
    {
        bool tickCalled {};
        bool detachCalled {};
        bool pauseCalled {};
        bool continueCalled {};
        bool stepCalled {};
        bool runToAddressCalled {};
        bool clearCallbacksCalled {};
        int attachCallCount {};
        int callOrder {};
        int attachOrder {};
        int tickOrder {};
        std::uint32_t capturedTimeout {};
        std::uint8_t capturedPassException {};
        ::StepMode capturedMode {};
        std::uint64_t capturedAddress {};
        DebuggerCallbacks capturedCallbacks {};
        bool callbacksCaptured {};
        void* capturedUserData {};
        StatusCode tickReturnCode {STATUS_DEBUG_TICK_NO_EVENT};
    };

    TestCtx g_ctx {};

    StatusCode VERTEX_API ctx_tick(uint32_t timeout)
    {
        g_ctx.tickCalled = true;
        g_ctx.capturedTimeout = timeout;
        g_ctx.tickOrder = ++g_ctx.callOrder;
        return g_ctx.tickReturnCode;
    }

    StatusCode VERTEX_API ctx_attach()
    {
        g_ctx.attachOrder = ++g_ctx.callOrder;
        g_ctx.attachCallCount++;
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_attach_counting()
    {
        g_ctx.attachCallCount++;
        return STATUS_ERROR_DEBUGGER_ATTACH_FAILED;
    }

    StatusCode VERTEX_API ctx_detach()
    {
        g_ctx.detachCalled = true;
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_continue(uint8_t passException)
    {
        g_ctx.continueCalled = true;
        g_ctx.capturedPassException = passException;
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_pause()
    {
        g_ctx.pauseCalled = true;
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_step(::StepMode mode)
    {
        g_ctx.stepCalled = true;
        g_ctx.capturedMode = mode;
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_run_to_address(uint64_t address)
    {
        g_ctx.runToAddressCalled = true;
        g_ctx.capturedAddress = address;
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_set_callbacks_capture(const DebuggerCallbacks* callbacks)
    {
        if (callbacks != nullptr)
        {
            g_ctx.capturedCallbacks = *callbacks;
            g_ctx.callbacksCaptured = true;
            g_ctx.capturedUserData = callbacks->user_data;
        }
        else
        {
            g_ctx.clearCallbacksCalled = true;
        }
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_set_callbacks_detect_clear(const DebuggerCallbacks* callbacks)
    {
        if (callbacks == nullptr)
        {
            g_ctx.clearCallbacksCalled = true;
        }
        return STATUS_OK;
    }

    StatusCode VERTEX_API ctx_tick_with_breakpoint_callback(uint32_t)
    {
        if (g_ctx.callbacksCaptured && g_ctx.capturedCallbacks.on_breakpoint_hit != nullptr)
        {
            ::DebugEvent event {};
            event.address = 0x401000;
            event.threadId = 42;
            g_ctx.capturedCallbacks.on_breakpoint_hit(&event, g_ctx.capturedCallbacks.user_data);
        }
        return STATUS_DEBUG_TICK_PAUSED;
    }

    StatusCode VERTEX_API ctx_tick_with_single_step_callback(uint32_t)
    {
        if (g_ctx.callbacksCaptured && g_ctx.capturedCallbacks.on_single_step != nullptr)
        {
            ::DebugEvent event {};
            event.address = 0x402000;
            event.threadId = 99;
            g_ctx.capturedCallbacks.on_single_step(&event, g_ctx.capturedCallbacks.user_data);
        }
        return STATUS_DEBUG_TICK_PAUSED;
    }

    StatusCode VERTEX_API ctx_tick_with_watchpoint_callback(uint32_t)
    {
        if (g_ctx.callbacksCaptured && g_ctx.capturedCallbacks.on_watchpoint_hit != nullptr)
        {
            WatchpointEvent event {};
            event.accessAddress = 0x7FFC00;
            event.threadId = 7;
            g_ctx.capturedCallbacks.on_watchpoint_hit(&event, g_ctx.capturedCallbacks.user_data);
        }
        return STATUS_DEBUG_TICK_PAUSED;
    }
}

class DebuggerEngineTest : public ::testing::Test
{
protected:
    NiceMock<MockILoader> m_loader {};
    NiceMock<MockILog> m_logger {};
    NiceMock<MockIThreadDispatcher> m_dispatcher {};
    Vertex::Runtime::Plugin m_plugin {m_logger};
    std::function<StatusCode()> m_capturedPumpTask {};

    void SetUp() override
    {
        g_ctx = {};

        m_plugin.internal_vertex_debugger_set_callbacks = stub_set_callbacks;
        m_plugin.internal_vertex_debugger_tick = stub_tick;
        m_plugin.internal_vertex_debugger_attach = stub_attach;
        m_plugin.internal_vertex_debugger_detach = stub_detach;
        m_plugin.internal_vertex_debugger_continue = stub_continue;
        m_plugin.internal_vertex_debugger_pause = stub_pause;
        m_plugin.internal_vertex_debugger_step = stub_step;
        m_plugin.internal_vertex_debugger_run_to_address = stub_run_to_address;

        ON_CALL(m_dispatcher, dispatch(_, _))
            .WillByDefault(Invoke(
                [](ThreadChannel, std::packaged_task<StatusCode()>&& task)
                    -> std::expected<std::future<StatusCode>, StatusCode>
                {
                    auto future = task.get_future();
                    task();
                    return future;
                }));
    }

    void expect_plugin_available()
    {
        ON_CALL(m_loader, has_plugin_loaded())
            .WillByDefault(Return(STATUS_OK));
        ON_CALL(m_loader, get_active_plugin())
            .WillByDefault(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>{m_plugin}));
    }

    void expect_plugin_unavailable()
    {
        ON_CALL(m_loader, has_plugin_loaded())
            .WillByDefault(Return(STATUS_ERROR_PLUGIN_NOT_LOADED));
    }

    void expect_schedule_recurring_succeeds()
    {
        ON_CALL(m_dispatcher, schedule_recurring(_, _, _, _, _, _))
            .WillByDefault(Return(RecurringTaskHandle{1, 1}));
    }

    void expect_schedule_recurring_captures_task()
    {
        ON_CALL(m_dispatcher, schedule_recurring(_, _, _, _, _, _))
            .WillByDefault(Invoke(
                [this](ThreadChannel, DispatchPriority, RecurringPolicy,
                       std::chrono::milliseconds, std::function<StatusCode()> task,
                       RecurringFailurePolicy) -> std::expected<RecurringTaskHandle, StatusCode>
                {
                    m_capturedPumpTask = std::move(task);
                    return RecurringTaskHandle{1, 1};
                }));
    }

    void expect_cancel_recurring_succeeds()
    {
        ON_CALL(m_dispatcher, cancel_recurring(_))
            .WillByDefault(Return(STATUS_OK));
    }

    void expect_dispatch_runs_task()
    {
        ON_CALL(m_dispatcher, dispatch(_, _))
            .WillByDefault(Invoke(
                [](ThreadChannel, std::packaged_task<StatusCode()>&& task)
                    -> std::expected<std::future<StatusCode>, StatusCode>
                {
                    auto future = task.get_future();
                    task();
                    return future;
                }));
    }

    void expect_not_single_threaded()
    {
        ON_CALL(m_dispatcher, is_single_threaded())
            .WillByDefault(Return(false));
    }

    StatusCode start_engine(DebuggerEngine& engine)
    {
        expect_plugin_available();
        expect_not_single_threaded();
        return engine.start();
    }

    void stop_engine_cleanly(DebuggerEngine& engine)
    {
        expect_cancel_recurring_succeeds();
        expect_dispatch_runs_task();
        std::ignore = engine.stop();
    }
};

// ===============================================================================================================
// Construction and Destruction
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Construction_InitialState_IsIdle)
{
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.state, EngineState::Idle);
    EXPECT_EQ(snapshot.currentAddress, 0u);
    EXPECT_EQ(snapshot.currentThreadId, 0u);
    EXPECT_EQ(snapshot.generation, 0u);
}

TEST_F(DebuggerEngineTest, Construction_GenerationIsZero)
{
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    EXPECT_EQ(engine.get_generation(), 0u);
}

// ===============================================================================================================
// start()
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Start_NoPlugin_ReturnsPluginNotLoaded)
{
    expect_plugin_unavailable();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    EXPECT_EQ(engine.start(), STATUS_ERROR_PLUGIN_NOT_LOADED);
}

TEST_F(DebuggerEngineTest, Start_SetCallbacksFails_ReturnsError)
{
    m_plugin.internal_vertex_debugger_set_callbacks = stub_set_callbacks_fail;
    expect_plugin_available();
    expect_not_single_threaded();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    EXPECT_EQ(engine.start(), STATUS_ERROR_GENERAL);
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Idle);
}

TEST_F(DebuggerEngineTest, Start_ScheduleRecurringFails_ReturnsError)
{
    expect_plugin_available();
    expect_not_single_threaded();
    ON_CALL(m_dispatcher, schedule_recurring(_, _, _, _, _, _))
        .WillByDefault(Return(std::unexpected{STATUS_ERROR_THREAD_IS_BUSY}));

    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    EXPECT_EQ(engine.start(), STATUS_ERROR_THREAD_IS_BUSY);
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Idle);
}

TEST_F(DebuggerEngineTest, Start_Success_TransitionsToDetached)
{
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    EXPECT_EQ(engine.get_generation(), 1u);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Start_AlreadyRunning_ReturnsError)
{
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    EXPECT_EQ(engine.start(), STATUS_ERROR_DEBUGGER_ALREADY_RUNNING);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// stop()
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Stop_NotRunning_ReturnsError)
{
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    EXPECT_EQ(engine.stop(), STATUS_ERROR_DEBUGGER_NOT_RUNNING);
}

TEST_F(DebuggerEngineTest, Stop_Running_TransitionsToStopped)
{
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    expect_cancel_recurring_succeeds();
    expect_dispatch_runs_task();
    EXPECT_EQ(engine.stop(), STATUS_OK);
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);
}

TEST_F(DebuggerEngineTest, Stop_DispatchFails_ReturnsError)
{
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    expect_cancel_recurring_succeeds();
    ON_CALL(m_dispatcher, dispatch(_, _))
        .WillByDefault(Invoke(
            [](ThreadChannel, std::packaged_task<StatusCode()>&&)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                return std::unexpected {STATUS_ERROR_THREAD_IS_BUSY};
            }));

    EXPECT_EQ(engine.stop(), STATUS_ERROR_THREAD_IS_BUSY);
}

// ===============================================================================================================
// send_command()
// ===============================================================================================================

TEST_F(DebuggerEngineTest, SendCommand_EnqueuesWithoutBlocking)
{
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.send_command(engine::CmdAttach{});
    engine.send_command(engine::CmdPause{});
    engine.send_command(engine::CmdShutdown{});
}

// ===============================================================================================================
// get_snapshot() / get_generation()
// ===============================================================================================================

TEST_F(DebuggerEngineTest, GetSnapshot_AfterStart_ReflectsDetached)
{
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.state, EngineState::Detached);
    EXPECT_EQ(snapshot.generation, 1u);
    EXPECT_EQ(engine.get_generation(), 1u);

    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// set_event_callback() / set_tick_timeout()
// ===============================================================================================================

TEST_F(DebuggerEngineTest, SetEventCallback_CanBeCalledBeforeStart)
{
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    bool called {};
    engine.set_event_callback(
        [&called](DirtyFlags, const EngineSnapshot&) { called = true; });
    EXPECT_FALSE(called);
}

TEST_F(DebuggerEngineTest, SetTickTimeout_StoresValues)
{
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.set_tick_timeout(200, 1000);
}

// ===============================================================================================================
// DirtyFlags operators
// ===============================================================================================================

TEST_F(DebuggerEngineTest, DirtyFlags_BitwiseOr)
{
    const auto combined = DirtyFlags::State | DirtyFlags::Modules;
    EXPECT_EQ(static_cast<std::uint32_t>(combined), 0x3u);
}

TEST_F(DebuggerEngineTest, DirtyFlags_BitwiseOrAssign)
{
    auto flags = DirtyFlags::None;
    flags |= DirtyFlags::Threads;
    flags |= DirtyFlags::Breakpoints;
    EXPECT_EQ(static_cast<std::uint32_t>(flags), 0xCu);
}

TEST_F(DebuggerEngineTest, DirtyFlags_BitwiseAnd)
{
    EXPECT_EQ(DirtyFlags::All & DirtyFlags::Registers, DirtyFlags::Registers);
}

TEST_F(DebuggerEngineTest, DirtyFlags_NoneIsZero)
{
    EXPECT_EQ(static_cast<std::uint32_t>(DirtyFlags::None), 0u);
}

// ===============================================================================================================
// CmdShutdown — single authority for cleanup
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdShutdown_TransitionsToStopped)
{
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    expect_cancel_recurring_succeeds();
    expect_dispatch_runs_task();
    ASSERT_EQ(engine.stop(), STATUS_OK);

    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.state, EngineState::Stopped);
    EXPECT_GE(snapshot.generation, 2u);
}

TEST_F(DebuggerEngineTest, CmdShutdown_ClearsCallbacks)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_detect_clear;

    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    expect_cancel_recurring_succeeds();
    expect_dispatch_runs_task();
    ASSERT_EQ(engine.stop(), STATUS_OK);
    EXPECT_TRUE(g_ctx.clearCallbacksCalled);
}

// ===============================================================================================================
// tick_once() — via captured recurring task
// ===============================================================================================================

TEST_F(DebuggerEngineTest, TickOnce_DetachedState_DoesNotCallTick)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.tickCalled)
        << "tick_once must not call vertex_debugger_tick when state is Detached";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_RunningState_CallsTick)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;
    m_capturedPumpTask();

    EXPECT_TRUE(g_ctx.tickCalled)
        << "tick_once must call vertex_debugger_tick when state is Running";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_StoppedState_DoesNotCallTick)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();

    g_ctx = {};
    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.tickCalled)
        << "tick_once must not call vertex_debugger_tick when state is Stopped";
}

TEST_F(DebuggerEngineTest, TickOnce_DrainsCommandsBeforeTick)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_attach = ctx_attach;
    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    EXPECT_GT(g_ctx.tickOrder, g_ctx.attachOrder)
        << "Commands must be drained before vertex_debugger_tick is called";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_ActiveTimeout_PassedToTick)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.set_tick_timeout(200, 1000);
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedTimeout, 200u)
        << "Running state should use activeTimeoutMs";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_ParkedTimeout_PassedWhenPaused)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.set_tick_timeout(100, 500);
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedTimeout, 500u)
        << "Paused state should use parkedTimeoutMs";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_ParkedTimeout_PassedWhenExited)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.set_tick_timeout(100, 500);
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_exited;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Exited);

    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedTimeout, 500u)
        << "Exited state should use parkedTimeoutMs";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_SingleThreadDependent_ClampsTimeout)
{
    expect_schedule_recurring_captures_task();

    ON_CALL(m_dispatcher, is_single_threaded()).WillByDefault(Return(true));
    m_plugin.get_plugin_info().featureCapability = VERTEX_FEATURE_DEBUGGER_DEPENDENT;

    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;

    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.set_tick_timeout(200, 1000);
    expect_plugin_available();
    ASSERT_EQ(engine.start(), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    EXPECT_LE(g_ctx.capturedTimeout, 50u)
        << "Single-threaded dependent mode must clamp timeout to singleThreadTickClampMs";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_MultiThread_DoesNotClampTimeout)
{
    expect_schedule_recurring_captures_task();

    m_plugin.internal_vertex_debugger_tick = ctx_tick;
    g_ctx.tickReturnCode = STATUS_DEBUG_TICK_NO_EVENT;

    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    engine.set_tick_timeout(200, 1000);
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedTimeout, 200u)
        << "Multi-threaded mode should not clamp active timeout";
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// tick_once() — tick result processing
// ===============================================================================================================

TEST_F(DebuggerEngineTest, TickResult_NoEvent_StaysRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_Processed_StaysRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_processed;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_Paused_TransitionsToPaused)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Paused);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_ProcessExited_TransitionsToExited)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_exited;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Exited);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_Detached_TransitionsToDetached)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_detached;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_Error_TransitionsToDetached)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_error;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_ProcessExited_BumpsGeneration)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    const auto genBefore = engine.get_generation();

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_exited;
    m_capturedPumpTask();

    EXPECT_GT(engine.get_generation(), genBefore + 1)
        << "Exited should bump generation multiple times (attach + exited transition)";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickResult_Detached_BumpsGeneration)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    const auto genBefore = engine.get_generation();

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_detached;
    m_capturedPumpTask();

    EXPECT_GT(engine.get_generation(), genBefore + 1)
        << "Detached should bump generation for cache invalidation";
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdAttach
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdAttach_FromDetached_TransitionsToRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdAttach_FromRunning_Ignored)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    const auto genBefore = engine.get_generation();
    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    EXPECT_EQ(engine.get_generation(), genBefore)
        << "Duplicate attach should be ignored without state change";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdAttach_PluginFails_StaysDetached)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_attach = stub_attach_fail;

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdAttach_BumpsGeneration)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    const auto genBefore = engine.get_generation();

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_GT(engine.get_generation(), genBefore)
        << "Attach should bump generation (DirtyFlags::All)";
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdDetach
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdDetach_FromRunning_TransitionsToDetached)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdDetach{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdDetach_FromPaused_TransitionsToDetached)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdDetach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdDetach_FromDetached_NoOp)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_detach = ctx_detach;

    const auto genBefore = engine.get_generation();
    engine.send_command(engine::CmdDetach{});
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.detachCalled)
        << "Detach from Detached state should be a no-op";
    EXPECT_EQ(engine.get_generation(), genBefore);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdDetach_CallsPluginDetach)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_detach = ctx_detach;

    engine.send_command(engine::CmdDetach{});
    m_capturedPumpTask();

    EXPECT_TRUE(g_ctx.detachCalled);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdContinue
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdContinue_FromPaused_TransitionsToRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdContinue{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdContinue_FromRunning_Ignored)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_continue = ctx_continue;

    engine.send_command(engine::CmdContinue{});
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.continueCalled)
        << "Continue from Running must be ignored";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdContinue_PassesExceptionFlag)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_continue = ctx_continue;

    engine.send_command(engine::CmdContinue{.passException = 1});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedPassException, 1u);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdPause
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdPause_FromRunning_CallsPluginPause)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_pause = ctx_pause;

    engine.send_command(engine::CmdPause{});
    m_capturedPumpTask();

    EXPECT_TRUE(g_ctx.pauseCalled);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdPause_FromPaused_Ignored)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    m_plugin.internal_vertex_debugger_pause = ctx_pause;

    engine.send_command(engine::CmdPause{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.pauseCalled)
        << "Pause from Paused must be ignored";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdPause_DoesNotTransitionImmediately)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdPause{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running)
        << "Pause does not transition state; that happens when tick returns PAUSED";
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdStepInto / CmdStepOver / CmdStepOut
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdStepInto_FromPaused_TransitionsToRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdStepInto{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepOver_FromPaused_TransitionsToRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdStepOver{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepOut_FromPaused_TransitionsToRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdStepOut{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepInto_FromRunning_Ignored)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_step = ctx_step;

    engine.send_command(engine::CmdStepInto{});
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.stepCalled)
        << "Step from Running must be ignored";
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepInto_CallsPluginWithCorrectMode)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_step = ctx_step;

    engine.send_command(engine::CmdStepInto{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedMode, VERTEX_STEP_INTO);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepOver_CallsPluginWithCorrectMode)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_step = ctx_step;

    engine.send_command(engine::CmdStepOver{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedMode, VERTEX_STEP_OVER);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepOut_CallsPluginWithCorrectMode)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_step = ctx_step;

    engine.send_command(engine::CmdStepOut{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedMode, VERTEX_STEP_OUT);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdRunToAddress
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdRunToAddress_FromPaused_TransitionsToRunning)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdRunToAddress{.address = 0xDEADBEEF});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdRunToAddress_PassesAddress)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    m_plugin.internal_vertex_debugger_run_to_address = ctx_run_to_address;

    engine.send_command(engine::CmdRunToAddress{.address = 0x00007FF600001000});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    EXPECT_EQ(g_ctx.capturedAddress, 0x00007FF600001000u);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdRunToAddress_FromRunning_Ignored)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_run_to_address = ctx_run_to_address;

    engine.send_command(engine::CmdRunToAddress{.address = 0x1234});
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.runToAddressCalled)
        << "RunToAddress from Running must be ignored";
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Command execution — CmdShutdown (via tick_once drain)
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdShutdown_FromRunning_DetachesAndTransitionsToStopped)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_detach = ctx_detach;

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();

    EXPECT_TRUE(g_ctx.detachCalled)
        << "Shutdown from Running must call plugin detach";
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);
}

TEST_F(DebuggerEngineTest, CmdShutdown_FromDetached_SkipsDetach)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_detach = ctx_detach;

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();

    EXPECT_FALSE(g_ctx.detachCalled)
        << "Shutdown from Detached must not call plugin detach";
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);
}

TEST_F(DebuggerEngineTest, CmdShutdown_ClearsCallbacksOnPlugin)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_detect_clear;

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();

    EXPECT_TRUE(g_ctx.clearCallbacksCalled)
        << "Shutdown must call set_callbacks(nullptr) to clear callbacks";
}

// ===============================================================================================================
// Command burst limit
// ===============================================================================================================

TEST_F(DebuggerEngineTest, DrainCommands_BurstLimit_DoesNotExceed32)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_attach = ctx_attach_counting;

    for (int i {}; i < 64; ++i)
    {
        engine.send_command(engine::CmdAttach{});
    }

    m_capturedPumpTask();

    EXPECT_LE(g_ctx.attachCallCount, 32)
        << "drain_commands must respect MAX_COMMAND_BURST (32)";

    m_capturedPumpTask();
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Generation tracking
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Generation_IncrementsOnEachStateTransition)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    const auto gen0 = engine.get_generation();

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    const auto gen1 = engine.get_generation();
    EXPECT_GT(gen1, gen0);

    engine.send_command(engine::CmdContinue{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();

    const auto gen2 = engine.get_generation();
    EXPECT_GT(gen2, gen1);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Generation_SnapshotMatchesAtomicGeneration)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.generation, engine.get_generation());
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Callback handlers — dirty flag setting
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Callback_BreakpointHit_UpdatesSnapshotAndDirtyFlags)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);
    ASSERT_TRUE(g_ctx.callbacksCaptured);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = ctx_tick_with_breakpoint_callback;
    m_capturedPumpTask();

    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.currentAddress, 0x401000u);
    EXPECT_EQ(snapshot.currentThreadId, 42u);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Callback_SingleStep_UpdatesSnapshotAndDirtyFlags)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);
    ASSERT_TRUE(g_ctx.callbacksCaptured);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = ctx_tick_with_single_step_callback;
    m_capturedPumpTask();

    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.currentAddress, 0x402000u);
    EXPECT_EQ(snapshot.currentThreadId, 99u);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Callback_NullEvent_DoesNotCrash)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(g_ctx.callbacksCaptured);

    g_ctx.capturedCallbacks.on_breakpoint_hit(nullptr, g_ctx.capturedCallbacks.user_data);
    g_ctx.capturedCallbacks.on_single_step(nullptr, g_ctx.capturedCallbacks.user_data);
    g_ctx.capturedCallbacks.on_exception(nullptr, g_ctx.capturedCallbacks.user_data);
    g_ctx.capturedCallbacks.on_watchpoint_hit(nullptr, g_ctx.capturedCallbacks.user_data);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Callback_NullUserData_DoesNotCrash)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;
    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(g_ctx.callbacksCaptured);

    ::DebugEvent event {};
    g_ctx.capturedCallbacks.on_breakpoint_hit(&event, nullptr);
    g_ctx.capturedCallbacks.on_single_step(&event, nullptr);
    g_ctx.capturedCallbacks.on_exception(&event, nullptr);

    WatchpointEvent watchEvent {};
    g_ctx.capturedCallbacks.on_watchpoint_hit(&watchEvent, nullptr);

    ThreadEvent threadEvent {};
    g_ctx.capturedCallbacks.on_thread_created(&threadEvent, nullptr);
    g_ctx.capturedCallbacks.on_thread_exited(&threadEvent, nullptr);

    ModuleEvent moduleEvent {};
    g_ctx.capturedCallbacks.on_module_loaded(&moduleEvent, nullptr);
    g_ctx.capturedCallbacks.on_module_unloaded(&moduleEvent, nullptr);

    g_ctx.capturedCallbacks.on_process_exited(0, nullptr);

    OutputStringEvent outputEvent {};
    g_ctx.capturedCallbacks.on_output_string(&outputEvent, nullptr);

    DebuggerError errorEvent {};
    g_ctx.capturedCallbacks.on_error(&errorEvent, nullptr);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Callback_WatchpointHit_UpdatesSnapshotAndDirtyFlags)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);
    ASSERT_TRUE(g_ctx.callbacksCaptured);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = ctx_tick_with_watchpoint_callback;
    m_capturedPumpTask();

    const auto snapshot = engine.get_snapshot();
    EXPECT_EQ(snapshot.currentAddress, 0x7FFC00u);
    EXPECT_EQ(snapshot.currentThreadId, 7u);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Callback registration plumbing
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Start_RegistersCallbacksWithPlugin)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;

    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    ASSERT_TRUE(g_ctx.callbacksCaptured);
    EXPECT_NE(g_ctx.capturedCallbacks.on_breakpoint_hit, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_single_step, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_exception, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_watchpoint_hit, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_thread_created, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_thread_exited, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_module_loaded, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_module_unloaded, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_process_exited, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_output_string, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.on_error, nullptr);
    EXPECT_NE(g_ctx.capturedCallbacks.user_data, nullptr);

    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, Start_UserDataPointsToEngine)
{
    m_plugin.internal_vertex_debugger_set_callbacks = ctx_set_callbacks_capture;

    expect_schedule_recurring_succeeds();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    EXPECT_EQ(g_ctx.capturedUserData, static_cast<void*>(&engine));
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Recurring pump scheduling
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Start_SchedulesPumpOnDebuggerChannel)
{
    ThreadChannel capturedChannel {};
    DispatchPriority capturedPriority {};
    RecurringPolicy capturedPolicy {};

    ON_CALL(m_dispatcher, schedule_recurring(_, _, _, _, _, _))
        .WillByDefault(Invoke(
            [&capturedChannel, &capturedPriority, &capturedPolicy](
                ThreadChannel channel, DispatchPriority priority, RecurringPolicy policy,
                std::chrono::milliseconds, std::function<StatusCode()>,
                RecurringFailurePolicy) -> std::expected<RecurringTaskHandle, StatusCode>
            {
                capturedChannel = channel;
                capturedPriority = priority;
                capturedPolicy = policy;
                return RecurringTaskHandle{1, 1};
            }));

    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    EXPECT_EQ(capturedChannel, ThreadChannel::Debugger);
    EXPECT_EQ(capturedPriority, DispatchPriority::High);
    EXPECT_EQ(capturedPolicy, RecurringPolicy::AsSoonAsPossible);

    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Full state machine cycle
// ===============================================================================================================

TEST_F(DebuggerEngineTest, StateMachine_FullCycle_IdleToStoppedThroughAllStates)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Idle);

    ASSERT_EQ(start_engine(engine), STATUS_OK);
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdContinue{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdDetach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);
}

TEST_F(DebuggerEngineTest, StateMachine_ReattachAfterDetach)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdDetach{});
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);

    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, StateMachine_ReattachAfterTickError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_error;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);

    m_plugin.internal_vertex_debugger_attach = stub_attach;
    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);

    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, StateMachine_ReattachAfterPluginDetach)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_detached;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);

    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Structured error events — invalid state preconditions
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdAttach_FromRunning_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "attach");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    EXPECT_EQ(error->stateAtError, EngineState::Running);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdContinue_FromRunning_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdContinue{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "continue");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdPause_FromPaused_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    engine.send_command(engine::CmdPause{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "pause");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepInto_FromRunning_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdStepInto{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "step_into");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepOver_FromRunning_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdStepOver{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "step_over");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepOut_FromRunning_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdStepOut{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "step_out");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdRunToAddress_FromRunning_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdRunToAddress{.address = 0x1234});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "run_to_address");
    EXPECT_EQ(error->code, STATUS_ERROR_INVALID_STATE);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Structured error events — plugin API failures
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdAttach_PluginFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_attach = stub_attach_fail;

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "attach");
    EXPECT_EQ(error->code, STATUS_ERROR_DEBUGGER_ATTACH_FAILED);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdContinue_PluginFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    m_plugin.internal_vertex_debugger_continue = stub_continue_fail;

    engine.send_command(engine::CmdContinue{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "continue");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdPause_PluginFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_pause = stub_pause_fail;

    engine.send_command(engine::CmdPause{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "pause");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdStepInto_PluginFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    m_plugin.internal_vertex_debugger_step = stub_step_fail;

    engine.send_command(engine::CmdStepInto{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "step_into");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdRunToAddress_PluginFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick_paused;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Paused);

    m_plugin.internal_vertex_debugger_run_to_address = stub_run_to_address_fail;

    engine.send_command(engine::CmdRunToAddress{.address = 0xDEAD});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "run_to_address");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Structured error events — consume clears last error
// ===============================================================================================================

TEST_F(DebuggerEngineTest, ConsumeLastError_ClearsAfterRead)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    ASSERT_TRUE(engine.consume_last_error().has_value());
    EXPECT_FALSE(engine.consume_last_error().has_value());
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, ConsumeLastError_NoErrorReturnsEmpty)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    EXPECT_FALSE(engine.consume_last_error().has_value());
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdDetach_FromDetached_NoError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdDetach{});
    m_capturedPumpTask();

    EXPECT_FALSE(engine.consume_last_error().has_value());
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, PostError_SetsDirtyFlags)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    engine.send_command(engine::CmdAttach{});
    m_capturedPumpTask();

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Fix 1: stop() drains entire queue to guarantee CmdShutdown executes
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Stop_DeepQueue_StillExecutesShutdown)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    for (int i {}; i < 64; ++i)
    {
        engine.send_command(engine::CmdPause{});
    }

    expect_cancel_recurring_succeeds();
    expect_dispatch_runs_task();
    ASSERT_EQ(engine.stop(), STATUS_OK);

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);
}

// ===============================================================================================================
// Fix 2: Structured errors for CmdDetach and CmdShutdown API failures
// ===============================================================================================================

TEST_F(DebuggerEngineTest, CmdDetach_PluginFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_detach = stub_detach_fail;

    engine.send_command(engine::CmdDetach{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Running);

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "detach");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, CmdShutdown_DetachFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_detach = stub_detach_fail;
    m_plugin.internal_vertex_debugger_set_callbacks = stub_set_callbacks;

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "shutdown_detach");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
}

TEST_F(DebuggerEngineTest, CmdShutdown_ClearCallbacksFails_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    m_plugin.internal_vertex_debugger_set_callbacks = stub_set_callbacks_clear_fail;

    engine.send_command(engine::CmdShutdown{});
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped);

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "shutdown_clear_callbacks");
    EXPECT_EQ(error->code, STATUS_ERROR_GENERAL);
}

// ===============================================================================================================
// Fix 3: TICK_ERROR invalidates all caches (DirtyFlags::All)
// ===============================================================================================================

TEST_F(DebuggerEngineTest, TickResult_Error_InvalidatesAllCaches)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    const auto genBefore = engine.get_generation();

    m_plugin.internal_vertex_debugger_tick = stub_tick_error;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    EXPECT_GT(engine.get_generation(), genBefore);
    stop_engine_cleanly(engine);
}

// ===============================================================================================================
// Destructor latency — never-started / failed-start must not block
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Destructor_NeverStarted_DoesNotBlock)
{
    const auto before = std::chrono::steady_clock::now();
    {
        DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    }
    const auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_LT(elapsed, std::chrono::milliseconds {100});
}

TEST_F(DebuggerEngineTest, Destructor_FailedStart_DoesNotBlock)
{
    expect_plugin_unavailable();
    const auto before = std::chrono::steady_clock::now();
    {
        DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
        ASSERT_EQ(engine.start(), STATUS_ERROR_PLUGIN_NOT_LOADED);
    }
    const auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_LT(elapsed, std::chrono::milliseconds {100});
}

// ===============================================================================================================
// stop() non-blocking contract
// ===============================================================================================================

TEST_F(DebuggerEngineTest, Stop_IsNonBlocking_CleanupDeferred)
{
    expect_schedule_recurring_succeeds();
    std::packaged_task<StatusCode()> capturedFinalTask {};

    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);

    expect_cancel_recurring_succeeds();
    ON_CALL(m_dispatcher, dispatch(_, _))
        .WillByDefault(Invoke(
            [&capturedFinalTask](ThreadChannel, std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                auto future = task.get_future();
                capturedFinalTask = std::move(task);
                return future;
            }));

    ASSERT_EQ(engine.stop(), STATUS_OK);
    EXPECT_NE(engine.get_snapshot().state, EngineState::Stopped)
        << "stop() must not block — state transition is deferred to async task";

    capturedFinalTask();
    EXPECT_EQ(engine.get_snapshot().state, EngineState::Stopped)
        << "State must reach Stopped once the dispatched cleanup runs";
}

TEST_F(DebuggerEngineTest, Destructor_AfterStop_WaitsForCompletion)
{
    expect_schedule_recurring_succeeds();
    std::packaged_task<StatusCode()> capturedFinalTask {};

    auto engine = std::make_unique<DebuggerEngine>(m_loader, m_dispatcher, m_logger);
    ASSERT_EQ(start_engine(*engine), STATUS_OK);

    expect_cancel_recurring_succeeds();
    ON_CALL(m_dispatcher, dispatch(_, _))
        .WillByDefault(Invoke(
            [&capturedFinalTask](ThreadChannel, std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                auto future = task.get_future();
                capturedFinalTask = std::move(task);
                return future;
            }));

    ASSERT_EQ(engine->stop(), STATUS_OK);

    std::thread completionThread(
        [&capturedFinalTask]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds {10});
            capturedFinalTask();
        });

    engine.reset();
    completionThread.join();
}

// ===============================================================================================================
// tick safe_call failure — structured error
// ===============================================================================================================

TEST_F(DebuggerEngineTest, TickOnce_SafeCallFailure_PostsError)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    m_plugin.internal_vertex_debugger_tick = nullptr;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);

    const auto error = engine.consume_last_error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->operation, "tick");
    EXPECT_EQ(error->code, STATUS_DEBUG_TICK_ERROR);

    stop_engine_cleanly(engine);
}

TEST_F(DebuggerEngineTest, TickOnce_SafeCallFailure_InvalidatesAllCaches)
{
    expect_schedule_recurring_captures_task();
    DebuggerEngine engine {m_loader, m_dispatcher, m_logger};
    ASSERT_EQ(start_engine(engine), STATUS_OK);
    ASSERT_TRUE(m_capturedPumpTask);

    engine.send_command(engine::CmdAttach{});
    m_plugin.internal_vertex_debugger_tick = stub_tick;
    m_capturedPumpTask();
    ASSERT_EQ(engine.get_snapshot().state, EngineState::Running);

    const auto genBefore = engine.get_generation();
    m_plugin.internal_vertex_debugger_tick = nullptr;
    m_capturedPumpTask();

    EXPECT_EQ(engine.get_snapshot().state, EngineState::Detached);
    EXPECT_GT(engine.get_generation(), genBefore);
    stop_engine_cleanly(engine);
}
