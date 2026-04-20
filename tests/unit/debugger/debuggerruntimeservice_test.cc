//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vertex/debugger/debuggerruntimeservice.hh>

#include <mocks/MockILog.hh>
#include <mocks/MockIThreadDispatcher.hh>

#include <chrono>
#include <expected>
#include <memory>
#include <thread>
#include <utility>

namespace dbg = Vertex::Debugger;
namespace service = Vertex::Debugger::service;

using Vertex::Testing::Mocks::MockILog;
using Vertex::Testing::Mocks::MockIThreadDispatcher;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

class DebuggerRuntimeServiceTest : public ::testing::Test
{
protected:
    MockIThreadDispatcher m_dispatcher {};
    MockILog m_log {};
    std::function<StatusCode()> m_reaperTask {};
    Vertex::Thread::RecurringTaskHandle m_reaperHandle {.id = 42, .epoch = 1};

    void SetUp() override
    {
        using namespace ::testing;
        EXPECT_CALL(m_dispatcher, schedule_recurring_persistent(
            Vertex::Thread::ThreadChannel::Debugger, _, _, _, _, _))
            .WillOnce(DoAll(SaveArg<4>(&m_reaperTask), Return(m_reaperHandle)));
        EXPECT_CALL(m_dispatcher, cancel_recurring(m_reaperHandle))
            .WillRepeatedly(Return(STATUS_OK));
        EXPECT_CALL(m_log, log_warn(_)).WillRepeatedly(Return(STATUS_OK));
        EXPECT_CALL(m_log, log_error(_)).WillRepeatedly(Return(STATUS_OK));
        EXPECT_CALL(m_log, log_info(_)).WillRepeatedly(Return(STATUS_OK));
    }

    void fire_reap_now()
    {
        ASSERT_TRUE(m_reaperTask);
        (void)m_reaperTask();
    }
};

TEST_F(DebuggerRuntimeServiceTest, ConstructorSchedulesReaper)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    EXPECT_TRUE(m_reaperTask);
}

TEST_F(DebuggerRuntimeServiceTest, SendCommandAllocatesMonotonicIds)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto a = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    const auto b = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    EXPECT_NE(a, Vertex::Runtime::INVALID_COMMAND_ID);
    EXPECT_EQ(b, a + 1);
}

TEST_F(DebuggerRuntimeServiceTest, SendCommandReturnsInvalidAfterShutdown)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    svc.shutdown();
    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    EXPECT_EQ(id, Vertex::Runtime::INVALID_COMMAND_ID);
}

TEST_F(DebuggerRuntimeServiceTest, OnResultUnblocksAwaitResult)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto id = svc.send_command(service::CmdReadRegisters {.threadId = 1},
                                      std::chrono::seconds {5});
    ASSERT_NE(id, Vertex::Runtime::INVALID_COMMAND_ID);

    std::thread producer {[&]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds {20});
        service::CommandResult result {
            .id = id,
            .code = STATUS_OK,
            .payload = service::RegisterSnapshotPayload {.registers = {}, .engineGeneration = 7}
        };
        svc.on_engine_command_result(std::move(result));
    }};

    const auto result = svc.await_result(id, std::chrono::seconds {2});
    producer.join();
    EXPECT_EQ(result.id, id);
    EXPECT_EQ(result.code, STATUS_OK);
    ASSERT_TRUE(std::holds_alternative<service::RegisterSnapshotPayload>(result.payload));
    EXPECT_EQ(std::get<service::RegisterSnapshotPayload>(result.payload).engineGeneration, 7u);
}

TEST_F(DebuggerRuntimeServiceTest, SubscribeResultSyncCallbackWhenAlreadyCompleted)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    svc.on_engine_command_result(service::CommandResult {.id = id, .code = STATUS_OK});

    bool fired = false;
    svc.subscribe_result(id, [&](const service::CommandResult& result)
    {
        fired = true;
        EXPECT_EQ(result.id, id);
        EXPECT_EQ(result.code, STATUS_OK);
    });
    EXPECT_TRUE(fired);
}

TEST_F(DebuggerRuntimeServiceTest, SubscribeResultAsyncCallbackWhenPending)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});

    std::atomic<bool> fired {false};
    svc.subscribe_result(id, [&](const service::CommandResult& result)
    {
        EXPECT_EQ(result.id, id);
        EXPECT_EQ(result.code, STATUS_OK);
        fired.store(true);
    });

    EXPECT_FALSE(fired.load());
    svc.on_engine_command_result(service::CommandResult {.id = id, .code = STATUS_OK});
    EXPECT_TRUE(fired.load());
}

TEST_F(DebuggerRuntimeServiceTest, SubscribeResultForUnknownIdFiresTimeout)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    bool fired = false;
    svc.subscribe_result(999u, [&](const service::CommandResult& result)
    {
        fired = true;
        EXPECT_EQ(result.code, STATUS_TIMEOUT);
    });
    EXPECT_TRUE(fired);
}

TEST_F(DebuggerRuntimeServiceTest, TimeoutReaperFiresSTATUS_TIMEOUT)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto id = svc.send_command(service::CmdPause {}, std::chrono::milliseconds {1});
    ASSERT_NE(id, Vertex::Runtime::INVALID_COMMAND_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds {5});

    fire_reap_now();

    const auto result = svc.await_result(id, std::chrono::milliseconds {50});
    EXPECT_EQ(result.id, id);
    EXPECT_EQ(result.code, STATUS_TIMEOUT);
}

TEST_F(DebuggerRuntimeServiceTest, ShutdownFiresSTATUS_SHUTDOWNForPending)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    ASSERT_NE(id, Vertex::Runtime::INVALID_COMMAND_ID);

    std::atomic<bool> fired {false};
    service::CommandResult capturedResult {};
    svc.subscribe_result(id, [&](const service::CommandResult& result)
    {
        capturedResult = result;
        fired.store(true);
    });

    svc.shutdown();
    EXPECT_TRUE(fired.load());
    EXPECT_EQ(capturedResult.id, id);
    EXPECT_EQ(capturedResult.code, STATUS_SHUTDOWN);
}

TEST_F(DebuggerRuntimeServiceTest, FanoutSubscribeReceivesOnEngineEvent)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};

    std::atomic<int> hits {0};
    const auto subId = svc.subscribe(
        dbg::mask_of(dbg::EngineEventKind::StateChanged),
        [&](const dbg::EngineEvent&) { ++hits; });
    EXPECT_NE(subId, Vertex::Runtime::INVALID_SUBSCRIPTION_ID);

    dbg::EngineEvent evt {
        .kind = dbg::EngineEventKind::StateChanged,
        .detail = dbg::StateChangedInfo {}
    };
    svc.on_engine_event(evt);
    EXPECT_EQ(hits.load(), 1);

    dbg::EngineEvent evt2 {
        .kind = dbg::EngineEventKind::BreakpointHit,
        .detail = {}
    };
    svc.on_engine_event(evt2);
    EXPECT_EQ(hits.load(), 1);

    svc.unsubscribe(subId);
    svc.on_engine_event(evt);
    EXPECT_EQ(hits.load(), 1);
}

TEST_F(DebuggerRuntimeServiceTest, AwaitResultForUnknownIdReturnsTIMEOUT)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto result = svc.await_result(12345u, std::chrono::milliseconds {10});
    EXPECT_EQ(result.code, STATUS_TIMEOUT);
}

TEST_F(DebuggerRuntimeServiceTest, AwaitResultTimesOutIfNoResult)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    const auto result = svc.await_result(id, std::chrono::milliseconds {20});
    EXPECT_EQ(result.id, id);
    EXPECT_EQ(result.code, STATUS_TIMEOUT);
}

#ifdef NDEBUG
TEST_F(DebuggerRuntimeServiceTest, AwaitResultFromUIThreadReturnsInvalidState)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    svc.set_ui_thread(std::this_thread::get_id());

    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    const auto result = svc.await_result(id, std::chrono::milliseconds {50});
    EXPECT_EQ(result.code, STATUS_ERROR_INVALID_STATE);
}
#endif

TEST_F(DebuggerRuntimeServiceTest, AwaitResultFromNonUIThreadAllowed)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    svc.set_ui_thread(std::this_thread::get_id());

    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});

    std::thread worker {[&]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds {20});
        svc.on_engine_command_result(service::CommandResult {.id = id, .code = STATUS_OK});
    }};

    service::CommandResult result {};
    std::thread awaiter {[&]
    {
        result = svc.await_result(id, std::chrono::seconds {2});
    }};
    awaiter.join();
    worker.join();
    EXPECT_EQ(result.code, STATUS_OK);
}

TEST_F(DebuggerRuntimeServiceTest, ClearUiThreadReEnablesAwaitFromCtorThread)
{
    dbg::DebuggerRuntimeService svc {m_dispatcher, m_log};
    svc.set_ui_thread(std::this_thread::get_id());
    svc.clear_ui_thread();

    const auto id = svc.send_command(service::CmdPause {}, std::chrono::seconds {5});
    const auto result = svc.await_result(id, std::chrono::milliseconds {20});
    EXPECT_EQ(result.code, STATUS_TIMEOUT);
}

