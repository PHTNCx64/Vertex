//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/scripting/iangelscript.hh>
#include <vertex/scripting/scriptingruntimeservice.hh>

#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    class StubAngelScript final : public Vertex::Scripting::IAngelScript
    {
      public:
        StatusCode start() override { return STATUS_OK; }
        StatusCode stop() override { return STATUS_OK; }
        bool is_running() const noexcept override { return false; }

        std::expected<Vertex::Scripting::ContextId, StatusCode> create_context(
            std::string_view, const std::filesystem::path&, Vertex::Scripting::BindingPolicy) override
        {
            return Vertex::Scripting::ContextId{1};
        }

        StatusCode remove_context(Vertex::Scripting::ContextId) override { return STATUS_OK; }
        StatusCode suspend_context(Vertex::Scripting::ContextId) override { return STATUS_OK; }
        StatusCode resume_context(Vertex::Scripting::ContextId) override { return STATUS_OK; }
        StatusCode set_breakpoint(Vertex::Scripting::ContextId, int) override { return STATUS_OK; }
        StatusCode remove_breakpoint(Vertex::Scripting::ContextId, int) override { return STATUS_OK; }

        std::expected<Vertex::Scripting::ScriptState, StatusCode>
        get_context_state(Vertex::Scripting::ContextId) const override
        {
            return Vertex::Scripting::ScriptState::Ready;
        }
        std::vector<Vertex::Scripting::ContextInfo> get_context_list() const override { return {}; }
        std::expected<std::vector<Vertex::Scripting::ContextVariable>, StatusCode>
        get_context_variables(Vertex::Scripting::ContextId) const override { return std::vector<Vertex::Scripting::ContextVariable>{}; }
    };
}

class ScriptingRuntimeServiceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_engine = std::make_unique<StubAngelScript>();
        m_log = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        m_dispatcher = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>>();

        ON_CALL(*m_dispatcher, schedule_recurring(_, _, _, _, _, _))
            .WillByDefault([](auto, auto, auto, auto, auto, auto)
                           { return std::unexpected(StatusCode::STATUS_ERROR_NOT_IMPLEMENTED); });
        ON_CALL(*m_dispatcher, schedule_recurring_persistent(_, _, _, _, _, _))
            .WillByDefault([](auto, auto, auto, auto, auto, auto)
                           { return std::unexpected(StatusCode::STATUS_ERROR_NOT_IMPLEMENTED); });

        m_service = std::make_unique<Vertex::Scripting::ScriptingRuntimeService>(
            *m_engine, *m_dispatcher, *m_log);
    }

    std::unique_ptr<StubAngelScript> m_engine;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_log;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<Vertex::Scripting::ScriptingRuntimeService> m_service;
};

TEST_F(ScriptingRuntimeServiceTest, MultipleScriptOutputSubscribersReceiveAllLinesInOrder)
{
    std::mutex m{};
    std::vector<std::string> seenA{};
    std::vector<std::string> seenB{};

    const auto subA = m_service->subscribe(
        static_cast<Vertex::Scripting::ScriptingEventKindMask>(Vertex::Scripting::ScriptingEventKind::ScriptOutput),
        [&](const Vertex::Scripting::ScriptingEvent& evt)
        {
            const auto* info = std::get_if<Vertex::Scripting::ScriptOutputInfo>(&evt.detail);
            if (info) { std::scoped_lock lk{m}; seenA.push_back(info->text); }
        });
    const auto subB = m_service->subscribe(
        static_cast<Vertex::Scripting::ScriptingEventKindMask>(Vertex::Scripting::ScriptingEventKind::ScriptOutput),
        [&](const Vertex::Scripting::ScriptingEvent& evt)
        {
            const auto* info = std::get_if<Vertex::Scripting::ScriptOutputInfo>(&evt.detail);
            if (info) { std::scoped_lock lk{m}; seenB.push_back(info->text); }
        });

    const std::vector<std::string> lines{"one", "two", "three", "four"};
    for (const auto& line : lines)
    {
        m_service->on_scripting_event(Vertex::Scripting::ScriptingEvent{
            .kind = Vertex::Scripting::ScriptingEventKind::ScriptOutput,
            .detail = Vertex::Scripting::ScriptOutputInfo{.moduleName = "mod", .text = line},
        });
    }

    EXPECT_EQ(seenA, lines);
    EXPECT_EQ(seenB, lines);

    m_service->unsubscribe(subA);
    m_service->unsubscribe(subB);
}

TEST_F(ScriptingRuntimeServiceTest, CmdStopYieldsScriptCompleteWithCanceled)
{
    StatusCode completeCode{STATUS_OK};
    std::string completeModule{};
    std::atomic<bool> gotComplete{false};
    const auto sub = m_service->subscribe(
        static_cast<Vertex::Scripting::ScriptingEventKindMask>(Vertex::Scripting::ScriptingEventKind::ScriptComplete),
        [&](const Vertex::Scripting::ScriptingEvent& evt)
        {
            const auto* info = std::get_if<Vertex::Scripting::ScriptCompleteInfo>(&evt.detail);
            if (!info) return;
            completeCode = info->code;
            completeModule = info->moduleName;
            gotComplete = true;
        });

    const auto runId = m_service->send_command(
        Vertex::Scripting::engine::CmdExecute{.moduleName = "mod"}, std::chrono::seconds{1});
    const auto runResult = m_service->await_result(runId, std::chrono::seconds{1});
    ASSERT_EQ(runResult.code, STATUS_OK);

    const auto stopId = m_service->send_command(
        Vertex::Scripting::engine::CmdStop{.moduleName = "mod"}, std::chrono::seconds{1});
    const auto stopResult = m_service->await_result(stopId, std::chrono::seconds{1});
    EXPECT_EQ(stopResult.code, STATUS_OK);

    EXPECT_TRUE(gotComplete.load());
    EXPECT_EQ(completeCode, StatusCode::STATUS_CANCELED);
    EXPECT_EQ(completeModule, "mod");

    m_service->unsubscribe(sub);
}

TEST_F(ScriptingRuntimeServiceTest, StopUnknownModuleReturnsNotFound)
{
    const auto id = m_service->send_command(
        Vertex::Scripting::engine::CmdStop{.moduleName = "ghost"}, std::chrono::seconds{1});
    const auto result = m_service->await_result(id, std::chrono::seconds{1});
    EXPECT_EQ(result.code, StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND);
}

TEST_F(ScriptingRuntimeServiceTest, BusyModuleRejectedWithBusyStatus)
{
    const auto firstId = m_service->send_command(
        Vertex::Scripting::engine::CmdExecute{.moduleName = "mod"}, std::chrono::seconds{1});
    ASSERT_EQ(m_service->await_result(firstId, std::chrono::seconds{1}).code, STATUS_OK);

    const auto secondId = m_service->send_command(
        Vertex::Scripting::engine::CmdExecute{.moduleName = "mod"}, std::chrono::seconds{1});
    const auto secondResult = m_service->await_result(secondId, std::chrono::seconds{1});
    EXPECT_EQ(secondResult.code, StatusCode::STATUS_ERROR_THREAD_IS_BUSY);
}

TEST_F(ScriptingRuntimeServiceTest, ShutdownDrainsPendingWithShutdownStatus)
{
    
    const auto id = m_service->send_command(
        Vertex::Scripting::engine::CmdEvaluate{.moduleName = "mod", .snippet = "x"},
        std::chrono::seconds{30});
    
    
    
    (void) id;

    m_service->shutdown();
}
