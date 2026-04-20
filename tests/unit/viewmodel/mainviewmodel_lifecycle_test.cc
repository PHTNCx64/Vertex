//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/event/eventbus.hh>
#include <vertex/model/mainmodel.hh>
#include <vertex/scanner/iscannerruntimeservice.hh>
#include <vertex/viewmodel/mainviewmodel.hh>

#include "../../mocks/MockILog.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockIMemoryScanner.hh"
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace
{
    using ::testing::_;
    using ::testing::NiceMock;
    using ::testing::Return;
    namespace scn = Vertex::Scanner;

    class StressFakeScannerService final : public scn::IScannerRuntimeService
    {
      public:
        Vertex::Runtime::CommandId
        send_command(scn::service::Command, std::chrono::milliseconds) override
        {
            return m_nextCmdId.fetch_add(1, std::memory_order_relaxed);
        }

        void subscribe_result(Vertex::Runtime::CommandId, ResultCallback) override {}

        scn::service::CommandResult
        await_result(Vertex::Runtime::CommandId id, std::chrono::milliseconds) override
        {
            return scn::service::CommandResult{.id = id, .code = STATUS_TIMEOUT};
        }

        Vertex::Runtime::SubscriptionId
        subscribe(scn::ScannerEventKindMask mask, EventCallback cb) override
        {
            const auto id = m_nextSubId.fetch_add(1, std::memory_order_relaxed);
            std::scoped_lock lock{m_mutex};
            m_subscriptions.emplace(id, Subscriber{.mask = mask, .callback = std::move(cb)});
            return id;
        }

        void unsubscribe(Vertex::Runtime::SubscriptionId id) noexcept override
        {
            std::scoped_lock lock{m_mutex};
            m_subscriptions.erase(id);
        }

        std::uint64_t results_count() const override { return 0; }
        StatusCode snapshot_results(std::vector<scn::IMemoryScanner::ScanResultEntry>&,
                                     std::size_t, std::size_t) const override { return STATUS_OK; }
        bool can_undo() const override { return false; }
        bool is_scanning() const override { return false; }

        std::optional<scn::TypeSchema> find_type(scn::TypeId) const override { return std::nullopt; }
        std::vector<scn::TypeSchema> list_types() const override { return {}; }
        std::uint32_t invalidate_plugin_types(std::size_t) override { return 0; }

        void on_scanner_event(scn::ScannerEvent event) override
        {
            std::vector<EventCallback*> callbacks{};
            {
                std::scoped_lock lock{m_mutex};
                for (auto& [id, sub] : m_subscriptions)
                {
                    if ((sub.mask & static_cast<scn::ScannerEventKindMask>(event.kind)) != 0)
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

        void on_scanner_command_result(scn::service::CommandResult) override {}

        void shutdown() override {}

        [[nodiscard]] std::size_t active_subscription_count() const
        {
            std::scoped_lock lock{m_mutex};
            return m_subscriptions.size();
        }

      private:
        struct Subscriber final
        {
            scn::ScannerEventKindMask mask{};
            EventCallback callback{};
        };

        mutable std::mutex m_mutex{};
        std::unordered_map<Vertex::Runtime::SubscriptionId, Subscriber> m_subscriptions{};
        std::atomic<Vertex::Runtime::CommandId> m_nextCmdId{1};
        std::atomic<Vertex::Runtime::SubscriptionId> m_nextSubId{1};
    };

    struct TestRig final
    {
        NiceMock<Vertex::Testing::Mocks::MockISettings> settings{};
        NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner> scanner{};
        NiceMock<Vertex::Testing::Mocks::MockILoader> loader{};
        NiceMock<Vertex::Testing::Mocks::MockILog> log{};
        NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher> dispatcher{};
        StressFakeScannerService scannerService{};
        Vertex::Event::EventBus eventBus{};

        TestRig()
        {
            ON_CALL(log, log_error(_)).WillByDefault(Return(STATUS_OK));
            ON_CALL(log, log_warn(_)).WillByDefault(Return(STATUS_OK));
            ON_CALL(log, log_info(_)).WillByDefault(Return(STATUS_OK));

            ON_CALL(settings, get_int(_, _))
                .WillByDefault([](std::string_view, int def) { return def; });
            ON_CALL(settings, get_bool(_, _))
                .WillByDefault([](std::string_view, bool def) { return def; });

            ON_CALL(dispatcher, dispatch_fire_and_forget(_, _))
                .WillByDefault([](Vertex::Thread::ThreadChannel,
                                   std::packaged_task<StatusCode()>&& task) -> StatusCode
                {
                    task();
                    return STATUS_OK;
                });
        }

        [[nodiscard]] std::unique_ptr<Vertex::ViewModel::MainViewModel> make_vm()
        {
            auto model = std::make_unique<Vertex::Model::MainModel>(
                settings, scanner, scannerService, loader, log, dispatcher);
            return std::make_unique<Vertex::ViewModel::MainViewModel>(
                std::move(model), eventBus, dispatcher, scannerService, "test");
        }
    };

    scn::ScannerEvent make_scan_complete_event()
    {
        return scn::ScannerEvent{.kind = scn::ScannerEventKind::ScanComplete,
                                  .detail = scn::ScanCompleteInfo{.matchCount = 0,
                                                                  .elapsed = std::chrono::milliseconds{0},
                                                                  .typeId = scn::TypeId::Invalid}};
    }
}

TEST(MainViewModelLifecycleTest, HundredCyclesLeakNoSubscriptions)
{
    TestRig rig{};

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        auto vm = rig.make_vm();
        (void) vm;
    }

    EXPECT_EQ(rig.scannerService.active_subscription_count(), 0u);
}

TEST(MainViewModelLifecycleTest, EventMidDestructionNoCrash)
{
    TestRig rig{};

    for (int iteration = 0; iteration < 50; ++iteration)
    {
        auto vm = rig.make_vm();
        rig.scannerService.on_scanner_event(make_scan_complete_event());
        vm.reset();
        rig.scannerService.on_scanner_event(make_scan_complete_event());
    }

    EXPECT_EQ(rig.scannerService.active_subscription_count(), 0u);
}
