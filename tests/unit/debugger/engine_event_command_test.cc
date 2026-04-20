//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/debugger/engine_command.hh>
#include <vertex/debugger/engine_event.hh>

#include <cstdint>
#include <utility>
#include <variant>

namespace dbg = Vertex::Debugger;
namespace service = Vertex::Debugger::service;
using dbg::EngineEvent;
using dbg::EngineEventKind;
using dbg::mask_of;
using dbg::StateChangedInfo;
using dbg::WatchpointHitInfo;

TEST(EngineEventTest, KindMaskBitsAreDistinct)
{
    static_assert(mask_of(EngineEventKind::None) == 0u);
    static_assert(mask_of(EngineEventKind::StateChanged) == 1u);
    static_assert(mask_of(EngineEventKind::BreakpointHit) == 2u);
    static_assert(mask_of(EngineEventKind::WatchpointHit) == 4u);

    const auto combined = EngineEventKind::StateChanged | EngineEventKind::WatchpointHit;
    EXPECT_EQ(mask_of(combined), 1u | 4u);
}

TEST(EngineEventTest, StateChangedDetail)
{
    EngineEvent event {};
    event.kind = EngineEventKind::StateChanged;
    event.detail = StateChangedInfo {
        .previous = dbg::DebuggerState::Running,
        .current = dbg::DebuggerState::Paused,
        .pid = 1234u
    };

    ASSERT_TRUE(std::holds_alternative<StateChangedInfo>(event.detail));
    const auto& info = std::get<StateChangedInfo>(event.detail);
    EXPECT_EQ(info.previous, dbg::DebuggerState::Running);
    EXPECT_EQ(info.current, dbg::DebuggerState::Paused);
    ASSERT_TRUE(info.pid.has_value());
    EXPECT_EQ(*info.pid, 1234u);
}

TEST(EngineEventTest, WatchpointHitDetail)
{
    EngineEvent event {};
    event.kind = EngineEventKind::WatchpointHit;
    event.detail = WatchpointHitInfo {
        .watchpointId = 7,
        .threadId = 42,
        .accessAddress = 0xDEADBEEF,
        .instructionAddress = 0xCAFEBABE,
        .accessType = dbg::WatchpointType::Write,
        .accessSize = 4
    };
    ASSERT_TRUE(std::holds_alternative<WatchpointHitInfo>(event.detail));
    EXPECT_EQ(std::get<WatchpointHitInfo>(event.detail).watchpointId, 7u);
}

TEST(EngineCommandTest, VariantAcceptsEachCommand)
{
    service::Command cmd = service::CmdAttach {.pid = 42};
    EXPECT_EQ(std::get<service::CmdAttach>(cmd).pid, 42u);

    cmd = service::CmdStep {.mode = dbg::StepMode::StepOver};
    EXPECT_EQ(std::get<service::CmdStep>(cmd).mode, dbg::StepMode::StepOver);

    cmd = service::CmdAddWatchpoint {.address = 0x1000, .size = 4, .type = dbg::WatchpointType::ReadWrite};
    EXPECT_EQ(std::get<service::CmdAddWatchpoint>(cmd).address, 0x1000u);

    cmd = service::CmdWriteMemory {.address = 0x2000, .bytes = {1, 2, 3}};
    EXPECT_EQ(std::get<service::CmdWriteMemory>(cmd).bytes.size(), 3u);
}

TEST(EngineCommandTest, CommandResultPayloadDefaultsToMonostate)
{
    service::CommandResult result {};
    EXPECT_EQ(result.id, Vertex::Runtime::INVALID_COMMAND_ID);
    EXPECT_EQ(result.code, STATUS_OK);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result.payload));
}

TEST(EngineCommandTest, CommandResultCarriesPayload)
{
    service::CommandResult result {
        .id = 7,
        .code = STATUS_OK,
        .payload = service::AddWatchpointResultPayload {.watchpointId = 99}
    };
    ASSERT_TRUE(std::holds_alternative<service::AddWatchpointResultPayload>(result.payload));
    EXPECT_EQ(std::get<service::AddWatchpointResultPayload>(result.payload).watchpointId, 99u);
}

TEST(EngineCommandTest, TimeoutResultCode)
{
    service::CommandResult result {.id = 1, .code = STATUS_TIMEOUT};
    EXPECT_EQ(result.code, STATUS_TIMEOUT);
}

TEST(EngineCommandTest, ShutdownResultCode)
{
    service::CommandResult result {.id = 1, .code = STATUS_SHUTDOWN};
    EXPECT_EQ(result.code, STATUS_SHUTDOWN);
}
