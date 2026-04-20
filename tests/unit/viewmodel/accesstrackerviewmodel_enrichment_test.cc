//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/debugger/engine_command.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/model/accesstrackermodel.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include "../../mocks/MockILog.hh"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
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

    class EnrichmentFakeRuntime final : public dbg::IDebuggerRuntimeService
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
        snapshot_registers(std::uint32_t, std::chrono::milliseconds) override
        {
            m_registerCalls.fetch_add(1, std::memory_order_relaxed);
            dbg::RegisterSet regs {};
            regs.generalPurpose.push_back(dbg::Register {
                .name = "rax", .value = 0xCAFEBABEULL, .bitWidth = 64,
            });
            regs.instructionPointer = 0x2000;
            return regs;
        }

        std::vector<dbg::StackFrame>
        snapshot_call_stack(std::uint32_t, std::chrono::milliseconds) override
        {
            m_stackCalls.fetch_add(1, std::memory_order_relaxed);
            std::vector<dbg::StackFrame> frames {};
            frames.push_back(dbg::StackFrame {
                .frameIndex = 0,
                .functionName = "top_fn",
                .moduleName = "game.exe",
            });
            frames.push_back(dbg::StackFrame {
                .frameIndex = 1,
                .functionName = "caller_fn",
                .moduleName = "game.exe",
            });
            return frames;
        }

        std::optional<dbg::DisassemblyLine>
        disassemble_one(std::uint64_t pc, std::chrono::milliseconds) override
        {
            m_disasmCalls.fetch_add(1, std::memory_order_relaxed);
            dbg::DisassemblyLine line {};
            line.address = pc;
            line.mnemonic = "mov";
            line.operands = "[rcx], eax";
            return line;
        }

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

        [[nodiscard]] std::uint64_t register_calls() const
        {
            return m_registerCalls.load(std::memory_order_relaxed);
        }
        [[nodiscard]] std::uint64_t stack_calls() const
        {
            return m_stackCalls.load(std::memory_order_relaxed);
        }
        [[nodiscard]] std::uint64_t disasm_calls() const
        {
            return m_disasmCalls.load(std::memory_order_relaxed);
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
        std::atomic<std::uint64_t> m_registerCalls {0};
        std::atomic<std::uint64_t> m_stackCalls {0};
        std::atomic<std::uint64_t> m_disasmCalls {0};
    };

    class InlineDispatcher final : public Vertex::Thread::IThreadDispatcher
    {
    public:
        std::expected<std::future<StatusCode>, StatusCode>
        dispatch(Vertex::Thread::ThreadChannel, std::packaged_task<StatusCode()>&& task) override
        {
            auto future = task.get_future();
            task();
            return future;
        }

        StatusCode
        dispatch_fire_and_forget(Vertex::Thread::ThreadChannel, std::packaged_task<StatusCode()>&& task) override
        {
            if (m_deferred)
            {
                m_deferredTasks.emplace_back(std::move(task));
                return STATUS_OK;
            }
            task();
            return STATUS_OK;
        }

        std::expected<Vertex::Thread::RecurringTaskHandle, StatusCode>
        schedule_recurring(Vertex::Thread::ThreadChannel, Vertex::Thread::DispatchPriority,
                           Vertex::Thread::RecurringPolicy, std::chrono::milliseconds,
                           std::function<StatusCode()>,
                           Vertex::Thread::RecurringFailurePolicy) override
        {
            return std::unexpected {STATUS_ERROR_NOT_IMPLEMENTED};
        }

        std::expected<Vertex::Thread::RecurringTaskHandle, StatusCode>
        schedule_recurring_persistent(Vertex::Thread::ThreadChannel, Vertex::Thread::DispatchPriority,
                                      Vertex::Thread::RecurringPolicy, std::chrono::milliseconds,
                                      std::function<StatusCode()>,
                                      Vertex::Thread::RecurringFailurePolicy) override
        {
            return std::unexpected {STATUS_ERROR_NOT_IMPLEMENTED};
        }

        StatusCode cancel_recurring(Vertex::Thread::RecurringTaskHandle) override { return STATUS_OK; }

        std::expected<std::future<StatusCode>, StatusCode>
        dispatch_with_priority(Vertex::Thread::ThreadChannel, Vertex::Thread::DispatchPriority,
                               std::packaged_task<StatusCode()>&& task) override
        {
            auto future = task.get_future();
            task();
            return future;
        }

        StatusCode configure(std::uint64_t) override { return STATUS_OK; }
        StatusCode start() override { return STATUS_OK; }
        StatusCode stop() override { return STATUS_OK; }
        bool is_single_threaded() const noexcept override { return true; }
        bool is_channel_busy(Vertex::Thread::ThreadChannel) const override { return false; }
        std::size_t pending_tasks(Vertex::Thread::ThreadChannel) const override { return 0; }

        StatusCode create_worker_pool(Vertex::Thread::ThreadChannel, std::size_t) override { return STATUS_OK; }
        StatusCode destroy_worker_pool(Vertex::Thread::ThreadChannel) override { return STATUS_OK; }
        StatusCode enqueue_on_worker(Vertex::Thread::ThreadChannel, std::size_t,
                                      std::packaged_task<StatusCode()>&&) override { return STATUS_OK; }

        void set_deferred(const bool deferred) { m_deferred = deferred; }

        std::size_t flush_deferred()
        {
            std::vector<std::packaged_task<StatusCode()>> tasks {};
            tasks.swap(m_deferredTasks);
            for (auto& task : tasks)
            {
                task();
            }
            return tasks.size();
        }

        std::size_t deferred_count() const { return m_deferredTasks.size(); }

    private:
        bool m_deferred {false};
        std::vector<std::packaged_task<StatusCode()>> m_deferredTasks {};
    };

    void silence_log(Vertex::Testing::Mocks::MockILog& log)
    {
        using ::testing::_;
        using ::testing::Return;
        ON_CALL(log, log_error(_)).WillByDefault(Return(STATUS_OK));
        ON_CALL(log, log_warn(_)).WillByDefault(Return(STATUS_OK));
        ON_CALL(log, log_info(_)).WillByDefault(Return(STATUS_OK));
    }

    dbg::WatchpointHitInfo make_hit(std::uint32_t watchpointId,
                                    std::uint64_t pc,
                                    std::uint32_t threadId = 1)
    {
        return dbg::WatchpointHitInfo {
            .watchpointId = watchpointId,
            .threadId = threadId,
            .accessAddress = 0x1000,
            .instructionAddress = pc,
            .accessType = dbg::WatchpointType::Write,
            .accessSize = 4,
        };
    }
}

TEST(AccessTrackerEnrichmentTest, HitPopulatesRegistersStackMnemonic)
{
    EnrichmentFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    InlineDispatcher dispatcher {};

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "test"};

    vm.start_tracking(0x1000, 4);
    ASSERT_EQ(vm.get_state().status, TrackingStatus::Active);

    const auto wpId = vm.get_state().watchpointId;
    runtime.on_engine_event(dbg::EngineEvent {
        .kind = dbg::EngineEventKind::WatchpointHit,
        .detail = make_hit(wpId, 0x2000),
    });

    const auto entries = vm.snapshot_entries();
    ASSERT_EQ(entries.size(), 1u);
    const auto& entry = entries.front();
    EXPECT_EQ(entry.instructionAddress, 0x2000u);
    EXPECT_EQ(entry.hitCount, 1u);
    ASSERT_FALSE(entry.lastRegisters.generalPurpose.empty());
    EXPECT_EQ(entry.lastRegisters.generalPurpose.front().name, "rax");
    ASSERT_GE(entry.lastCallStack.size(), 2u);
    EXPECT_EQ(entry.lastCallStack[0].functionName, "top_fn");
    EXPECT_EQ(entry.lastCallStack[1].functionName, "caller_fn");
    EXPECT_EQ(entry.lastMnemonic, "mov [rcx], eax");

    EXPECT_EQ(runtime.register_calls(), 1u);
    EXPECT_EQ(runtime.stack_calls(), 1u);
    EXPECT_EQ(runtime.disasm_calls(), 1u);

    vm.stop_tracking();
}

TEST(AccessTrackerEnrichmentTest, RepeatedHitsSamePcCoalesceEnrichment)
{
    EnrichmentFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    InlineDispatcher dispatcher {};
    dispatcher.set_deferred(true);

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "test"};

    vm.start_tracking(0x1000, 4);
    const auto wpId = vm.get_state().watchpointId;

    for (int i = 0; i < 5; ++i)
    {
        runtime.on_engine_event(dbg::EngineEvent {
            .kind = dbg::EngineEventKind::WatchpointHit,
            .detail = make_hit(wpId, 0x2000),
        });
    }

    dispatcher.flush_deferred();

    const auto entries = vm.snapshot_entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries.front().hitCount, 5u);
    EXPECT_EQ(entries.front().lastMnemonic, "mov [rcx], eax");
    EXPECT_LT(runtime.register_calls(), 5u);

    vm.stop_tracking();
}

TEST(AccessTrackerEnrichmentTest, ClearDropsDeferredEnrichment)
{
    EnrichmentFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    InlineDispatcher dispatcher {};
    dispatcher.set_deferred(true);

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "test"};

    vm.start_tracking(0x1000, 4);
    const auto wpId = vm.get_state().watchpointId;

    runtime.on_engine_event(dbg::EngineEvent {
        .kind = dbg::EngineEventKind::WatchpointHit,
        .detail = make_hit(wpId, 0x3000),
    });

    vm.clear();
    EXPECT_EQ(vm.snapshot_entries().size(), 0u);

    const auto ran = dispatcher.flush_deferred();
    EXPECT_GE(ran, 1u);

    EXPECT_EQ(vm.snapshot_entries().size(), 0u);
    EXPECT_EQ(runtime.register_calls(), 0u);

    vm.stop_tracking();
}

TEST(AccessTrackerEnrichmentTest, StopDropsDeferredEnrichment)
{
    EnrichmentFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    InlineDispatcher dispatcher {};
    dispatcher.set_deferred(true);

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "test"};

    vm.start_tracking(0x1000, 4);
    const auto wpId = vm.get_state().watchpointId;

    runtime.on_engine_event(dbg::EngineEvent {
        .kind = dbg::EngineEventKind::WatchpointHit,
        .detail = make_hit(wpId, 0x4000),
    });

    vm.stop_tracking();
    EXPECT_EQ(vm.get_state().status, TrackingStatus::Idle);

    dispatcher.flush_deferred();

    EXPECT_EQ(runtime.register_calls(), 0u);
}
