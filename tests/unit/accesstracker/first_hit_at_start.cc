//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//








#include <gtest/gtest.h>

#include <vertex/model/accesstrackermodel.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include "../../mocks/MockILog.hh"
#include "../../support/accesstracker_common.hh"

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

    constexpr std::uint32_t PREDICTED_WATCHPOINT_ID {100};
}

TEST(AccessTracker, FirstHitArrivesDuringStartingBuffersAndMerges)
{
    for (int iteration = 0; iteration < 200; ++iteration)
    {
        ControlledFakeRuntime runtime {};
        Vertex::Testing::Mocks::MockILog log {};
        silence_log(log);
        DeferrableDispatcher dispatcher {};

        runtime.set_auto_complete(false);

        auto model = std::make_unique<AccessTrackerModel>(runtime);
        AccessTrackerViewModel vm {std::move(model), dispatcher, log, "first-hit"};

        vm.start_tracking(0x1000, 4);
        ASSERT_EQ(vm.get_state().status, TrackingStatus::Starting);

        const auto pending = runtime.peek_oldest_pending();
        ASSERT_TRUE(pending.has_value());

        const int extraHits = iteration % 4;
        runtime.fire_watchpoint_hit(PREDICTED_WATCHPOINT_ID, 0x2000);
        for (int i = 0; i < extraHits; ++i)
        {
            runtime.fire_watchpoint_hit(PREDICTED_WATCHPOINT_ID, 0x2000);
        }

        runtime.complete_command(*pending, STATUS_OK);

        ASSERT_EQ(vm.get_state().status, TrackingStatus::Active) << "iteration " << iteration;
        ASSERT_EQ(vm.get_state().watchpointId, PREDICTED_WATCHPOINT_ID);

        const auto entries = vm.snapshot_entries();
        ASSERT_EQ(entries.size(), 1u) << "iteration " << iteration;
        EXPECT_EQ(entries.front().instructionAddress, 0x2000u);
        EXPECT_EQ(entries.front().hitCount, static_cast<std::uint32_t>(1 + extraHits));

        vm.stop_tracking();
    }
}
