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

TEST(AccessTracker, DetachDuringStartingDropsLateSuccess)
{
    for (int iteration = 0; iteration < 50; ++iteration)
    {
        ControlledFakeRuntime runtime {};
        Vertex::Testing::Mocks::MockILog log {};
        silence_log(log);
        DeferrableDispatcher dispatcher {};

        runtime.set_auto_complete(false);

        auto model = std::make_unique<AccessTrackerModel>(runtime);
        AccessTrackerViewModel vm {std::move(model), dispatcher, log, "detach-start"};

        std::atomic<int> completionHits {0};
        std::atomic<StatusCode> completionStatus {STATUS_OK};

        vm.start_tracking(0x1000, 4, [&](const StatusCode status) {
            completionHits.fetch_add(1, std::memory_order_relaxed);
            completionStatus.store(status, std::memory_order_relaxed);
        });

        EXPECT_EQ(vm.get_state().status, TrackingStatus::Starting);

        const auto pending = runtime.peek_oldest_pending();
        ASSERT_TRUE(pending.has_value());

        runtime.fire_detached();

        EXPECT_EQ(vm.get_state().status, TrackingStatus::Idle);
        EXPECT_EQ(completionHits.load(), 1);
        EXPECT_EQ(completionStatus.load(), STATUS_CANCELED);

        runtime.complete_command(*pending, STATUS_OK);

        EXPECT_EQ(vm.get_state().status, TrackingStatus::Idle);
        EXPECT_EQ(vm.get_state().watchpointId, 0u);
        EXPECT_EQ(completionHits.load(), 1) << "late success must not fire second completion";
        EXPECT_EQ(vm.snapshot_entries().size(), 0u);
    }
}
