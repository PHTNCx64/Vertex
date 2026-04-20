//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/runtime/subscription_guard.hh>

#include <utility>
#include <vector>

namespace dbg = Vertex::Debugger;
namespace service = Vertex::Debugger::service;

namespace
{
    class FakeRuntimeService final : public dbg::IDebuggerRuntimeService
    {
    public:
        Vertex::Runtime::CommandId
        send_command(service::Command ,
                     std::chrono::milliseconds ) override
        {
            return ++m_nextId;
        }

        void subscribe_result(Vertex::Runtime::CommandId , ResultCallback ) override
        {
            ++resultSubscribes;
        }

        service::CommandResult
        await_result(Vertex::Runtime::CommandId id,
                     std::chrono::milliseconds ) override
        {
            return service::CommandResult {.id = id, .code = STATUS_OK};
        }

        Vertex::Runtime::SubscriptionId
        subscribe(dbg::EngineEventKindMask , EventCallback ) override
        {
            return ++m_nextSub;
        }

        void unsubscribe(Vertex::Runtime::SubscriptionId id) noexcept override
        {
            unsubscribed.push_back(id);
        }

        std::optional<dbg::RegisterSet>
        snapshot_registers(std::uint32_t , std::chrono::milliseconds ) override
        {
            return dbg::RegisterSet {};
        }

        std::vector<dbg::StackFrame>
        snapshot_call_stack(std::uint32_t , std::chrono::milliseconds ) override
        {
            return {};
        }

        std::optional<dbg::DisassemblyLine>
        disassemble_one(std::uint64_t , std::chrono::milliseconds ) override
        {
            return std::nullopt;
        }

        void shutdown() override
        {
            didShutdown = true;
        }

        void on_engine_event(dbg::EngineEvent ) override
        {
            ++engineEvents;
        }

        void on_engine_command_result(service::CommandResult ) override
        {
            ++engineCommandResults;
        }

        void attach_engine(dbg::DebuggerEngine* ) noexcept override
        {
            ++attachEngineCalls;
        }

        int engineEvents {0};
        int engineCommandResults {0};
        int attachEngineCalls {0};

        int resultSubscribes {0};
        bool didShutdown {false};
        std::vector<Vertex::Runtime::SubscriptionId> unsubscribed {};

    private:
        Vertex::Runtime::CommandId m_nextId {0};
        Vertex::Runtime::SubscriptionId m_nextSub {0};
    };
}

TEST(IDebuggerRuntimeServiceTest, FakeImplementsInterface)
{
    FakeRuntimeService svc {};
    dbg::IDebuggerRuntimeService& iface = svc;

    const auto cmdId = iface.send_command(service::CmdPause {});
    EXPECT_EQ(cmdId, 1u);

    iface.subscribe_result(cmdId, [](const service::CommandResult&) {});
    EXPECT_EQ(svc.resultSubscribes, 1);

    const auto result = iface.await_result(cmdId);
    EXPECT_EQ(result.id, cmdId);
    EXPECT_EQ(result.code, STATUS_OK);

    const auto subId = iface.subscribe(
        dbg::mask_of(dbg::EngineEventKind::StateChanged),
        [](const dbg::EngineEvent&) {});
    EXPECT_EQ(subId, 1u);

    iface.unsubscribe(subId);
    EXPECT_EQ(svc.unsubscribed.size(), 1u);

    iface.shutdown();
    EXPECT_TRUE(svc.didShutdown);
}

TEST(IDebuggerRuntimeServiceTest, SubscriptionGuardUnsubscribes)
{
    FakeRuntimeService svc {};
    {
        Vertex::Runtime::SubscriptionGuard<dbg::IDebuggerRuntimeService> guard {
            svc,
            svc.subscribe(dbg::mask_of(dbg::EngineEventKind::WatchpointHit),
                          [](const dbg::EngineEvent&) {})
        };
        EXPECT_TRUE(static_cast<bool>(guard));
    }
    ASSERT_EQ(svc.unsubscribed.size(), 1u);
    EXPECT_EQ(svc.unsubscribed[0], 1u);
}

TEST(IDebuggerRuntimeServiceTest, DefaultTimeoutsExposed)
{
    EXPECT_EQ(dbg::IDebuggerRuntimeService::DEFAULT_COMMAND_TIMEOUT.count(), 5000);
    EXPECT_EQ(dbg::IDebuggerRuntimeService::DEFAULT_SNAPSHOT_TIMEOUT.count(), 2000);
}

TEST(IDebuggerRuntimeServiceTest, SnapshotHelpersReturnOptionalAndVector)
{
    FakeRuntimeService svc {};
    dbg::IDebuggerRuntimeService& iface = svc;
    EXPECT_TRUE(iface.snapshot_registers(0).has_value());
    EXPECT_TRUE(iface.snapshot_call_stack(0).empty());
    EXPECT_FALSE(iface.disassemble_one(0).has_value());
}
