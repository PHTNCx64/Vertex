//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//








#include <gtest/gtest.h>

#include <vertex/model/accesstrackermodel.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include "../../mocks/MockILog.hh"
#include "../../support/accesstracker_common.hh"

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    using Vertex::Testing::AccessTracker::ControlledFakeRuntime;
    using Vertex::Testing::AccessTracker::DeferrableDispatcher;
    using Vertex::Model::AccessTrackerModel;
    using Vertex::ViewModel::AccessTrackerViewModel;
    using Vertex::ViewModel::TrackingStatus;

    void silence_log(Vertex::Testing::Mocks::MockILog& log)
    {
        using ::testing::_;
        using ::testing::Return;
        ON_CALL(log, log_error(_)).WillByDefault(Return(STATUS_OK));
        ON_CALL(log, log_warn(_)).WillByDefault(Return(STATUS_OK));
        ON_CALL(log, log_info(_)).WillByDefault(Return(STATUS_OK));
    }
}

TEST(AccessTracker, ConcurrentWriterAndReaderGuardStayConsistent)
{
    ControlledFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    DeferrableDispatcher dispatcher {};
    dispatcher.set_deferred(true);

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "rw"};

    vm.start_tracking(0x1000, 4);
    const auto wpId = vm.get_state().watchpointId;

    std::atomic<bool> stop {false};
    std::atomic<std::size_t> lastReaderCount {0};
    std::atomic<bool> monotonic {true};

    std::thread writer {[&] {
        std::uint64_t pc {0x2000};
        while (!stop.load(std::memory_order_acquire))
        {
            runtime.fire_watchpoint_hit(wpId, pc);
            pc += 4;
            if (pc > 0x2000 + 4096) pc = 0x2000;
        }
    }};

    std::thread reader {[&] {
        while (!stop.load(std::memory_order_acquire))
        {
            auto guard = vm.lock_entries_for_read();
            const auto n = guard.size();
            std::size_t counted {};
            for (const auto& entry : guard.entries())
            {
                (void)entry;
                ++counted;
            }
            if (counted != n)
            {
                monotonic.store(false, std::memory_order_release);
            }
            const auto prior = lastReaderCount.load(std::memory_order_acquire);
            if (n < prior)
            {
                monotonic.store(false, std::memory_order_release);
            }
            lastReaderCount.store(n, std::memory_order_release);
        }
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    stop.store(true, std::memory_order_release);
    writer.join();
    reader.join();

    EXPECT_TRUE(monotonic.load(std::memory_order_acquire));
    EXPECT_GT(vm.snapshot_entries().size(), 0u);

    dispatcher.set_deferred(false);
    dispatcher.flush_deferred();
    vm.stop_tracking();
}
