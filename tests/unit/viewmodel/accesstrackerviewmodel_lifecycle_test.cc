//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/debugger/engine_command.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/model/accesstrackermodel.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <variant>

namespace
{
    namespace dbg = Vertex::Debugger;
    namespace service = Vertex::Debugger::service;
    using Vertex::Model::AccessTrackerModel;
    using Vertex::Model::StartTrackingResult;
    using Vertex::ViewModel::AccessTrackerViewModel;
    using Vertex::ViewModel::TrackingStatus;

    class StressFakeRuntime final : public dbg::IDebuggerRuntimeService
    {
    public:
        Vertex::Runtime::CommandId
        send_command(service::Command command, std::chrono::milliseconds) override
        {
            const auto id = m_nextCmdId.fetch_add(1, std::memory_order_relaxed);
            std::scoped_lock lock {m_mutex};
            m_pendingCommands.emplace(id, std::move(command));
            return id;
        }

        void subscribe_result(Vertex::Runtime::CommandId id, ResultCallback cb) override
        {
            service::Command command {};
            {
                std::scoped_lock lock {m_mutex};
                const auto it = m_pendingCommands.find(id);
                if (it == m_pendingCommands.end())
                {
                    cb(service::CommandResult {.id = id, .code = STATUS_TIMEOUT});
                    return;
                }
                command = std::move(it->second);
                m_pendingCommands.erase(it);
            }

            service::CommandResult result {.id = id, .code = STATUS_OK};
            if (std::holds_alternative<service::CmdAddWatchpoint>(command))
            {
                const auto wpId = m_nextWatchpointId.fetch_add(1, std::memory_order_relaxed);
                result.payload = service::AddWatchpointResultPayload {.watchpointId = wpId};
            }
            cb(result);
        }

        service::CommandResult
        await_result(Vertex::Runtime::CommandId id, std::chrono::milliseconds) override
        {
            return service::CommandResult {.id = id, .code = STATUS_ERROR_NOT_IMPLEMENTED};
        }

        Vertex::Runtime::SubscriptionId
        subscribe(dbg::EngineEventKindMask mask, EventCallback cb) override
        {
            const auto id = m_nextSubId.fetch_add(1, std::memory_order_relaxed);
            std::scoped_lock lock {m_mutex};
            m_subscriptions.emplace(id, Subscriber {.mask = mask, .callback = std::move(cb)});
            return id;
        }

        void unsubscribe(Vertex::Runtime::SubscriptionId id) noexcept override
        {
            std::scoped_lock lock {m_mutex};
            m_subscriptions.erase(id);
        }

        std::optional<dbg::RegisterSet>
        snapshot_registers(std::uint32_t, std::chrono::milliseconds) override { return std::nullopt; }

        std::vector<dbg::StackFrame>
        snapshot_call_stack(std::uint32_t, std::chrono::milliseconds) override { return {}; }

        std::optional<dbg::DisassemblyLine>
        disassemble_one(std::uint64_t, std::chrono::milliseconds) override { return std::nullopt; }

        void shutdown() override {}

        void on_engine_event(dbg::EngineEvent event) override
        {
            std::vector<EventCallback*> callbacks {};
            {
                std::scoped_lock lock {m_mutex};
                for (auto& [id, sub] : m_subscriptions)
                {
                    if ((sub.mask & dbg::mask_of(event.kind)) != 0)
                    {
                        callbacks.push_back(&sub.callback);
                    }
                }
            }
            for (auto* cb : callbacks)
            {
                if (*cb) (*cb)(event);
            }
        }

        void on_engine_command_result(service::CommandResult) override {}

        void attach_engine(dbg::DebuggerEngine*) noexcept override {}

        [[nodiscard]] std::size_t active_subscription_count() const
        {
            std::scoped_lock lock {m_mutex};
            return m_subscriptions.size();
        }

        [[nodiscard]] std::size_t pending_command_count() const
        {
            std::scoped_lock lock {m_mutex};
            return m_pendingCommands.size();
        }

    private:
        struct Subscriber final
        {
            dbg::EngineEventKindMask mask {};
            EventCallback callback {};
        };

        mutable std::mutex m_mutex {};
        std::unordered_map<Vertex::Runtime::CommandId, service::Command> m_pendingCommands {};
        std::unordered_map<Vertex::Runtime::SubscriptionId, Subscriber> m_subscriptions {};
        std::atomic<Vertex::Runtime::CommandId> m_nextCmdId {1};
        std::atomic<Vertex::Runtime::SubscriptionId> m_nextSubId {1};
        std::atomic<std::uint32_t> m_nextWatchpointId {100};
    };
}

TEST(AccessTrackerViewModelLifecycleTest, HundredCyclesLeakNothing)
{
    StressFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    using ::testing::_;
    using ::testing::Return;
    ON_CALL(log, log_error(_)).WillByDefault(Return(STATUS_OK));
    ON_CALL(log, log_warn(_)).WillByDefault(Return(STATUS_OK));
    ON_CALL(log, log_info(_)).WillByDefault(Return(STATUS_OK));

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        auto model = std::make_unique<AccessTrackerModel>(runtime);
        Vertex::Testing::Mocks::MockIThreadDispatcher dispatcher {};
        AccessTrackerViewModel viewModel {std::move(model), dispatcher, log, "test"};

        const auto iter = static_cast<std::uint64_t>(iteration);
        viewModel.start_tracking(0x1000ULL + iter, 4);
        EXPECT_EQ(viewModel.get_state().status, TrackingStatus::Active) << "iteration " << iteration;

        runtime.on_engine_event(dbg::EngineEvent {
            .kind = dbg::EngineEventKind::WatchpointHit,
            .detail = dbg::WatchpointHitInfo {
                .watchpointId = viewModel.get_state().watchpointId,
                .threadId = 1,
                .accessAddress = 0x1000ULL + iter,
                .instructionAddress = 0x2000ULL + iter,
                .accessType = dbg::WatchpointType::Write,
                .accessSize = 4
            }
        });

        viewModel.stop_tracking();
        EXPECT_EQ(viewModel.get_state().status, TrackingStatus::Idle) << "iteration " << iteration;
    }

    EXPECT_EQ(runtime.active_subscription_count(), 0u);
    EXPECT_EQ(runtime.pending_command_count(), 0u);
}

TEST(AccessTrackerViewModelLifecycleTest, DetachDuringActiveClearsState)
{
    StressFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    using ::testing::_;
    using ::testing::Return;
    ON_CALL(log, log_error(_)).WillByDefault(Return(STATUS_OK));
    ON_CALL(log, log_warn(_)).WillByDefault(Return(STATUS_OK));
    ON_CALL(log, log_info(_)).WillByDefault(Return(STATUS_OK));

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    Vertex::Testing::Mocks::MockIThreadDispatcher dispatcher {};
    AccessTrackerViewModel viewModel {std::move(model), dispatcher, log, "test"};

    viewModel.start_tracking(0xDEADBEEF, 4);
    ASSERT_EQ(viewModel.get_state().status, TrackingStatus::Active);

    runtime.on_engine_event(dbg::EngineEvent {
        .kind = dbg::EngineEventKind::StateChanged,
        .detail = dbg::StateChangedInfo {
            .previous = dbg::DebuggerState::Running,
            .current = dbg::DebuggerState::Detached,
            .pid = std::nullopt
        }
    });

    EXPECT_EQ(viewModel.get_state().status, TrackingStatus::Idle);
}

TEST(AccessTrackerViewModelLifecycleTest, DtorAfterStartUnsubscribes)
{
    StressFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    using ::testing::_;
    using ::testing::Return;
    ON_CALL(log, log_error(_)).WillByDefault(Return(STATUS_OK));
    ON_CALL(log, log_warn(_)).WillByDefault(Return(STATUS_OK));
    ON_CALL(log, log_info(_)).WillByDefault(Return(STATUS_OK));

    {
        auto model = std::make_unique<AccessTrackerModel>(runtime);
        Vertex::Testing::Mocks::MockIThreadDispatcher dispatcher {};
        AccessTrackerViewModel viewModel {std::move(model), dispatcher, log, "test"};
        viewModel.start_tracking(0x1000, 4);
        ASSERT_EQ(runtime.active_subscription_count(), 2u);
    }

    EXPECT_EQ(runtime.active_subscription_count(), 0u);
}
