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
}

TEST(AccessTracker, DetachDuringActiveClearsStateAndSuppressesFurtherHits)
{
    ControlledFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    DeferrableDispatcher dispatcher {};

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "detach-active"};

    vm.start_tracking(0x1000, 4);
    ASSERT_EQ(vm.get_state().status, TrackingStatus::Active);
    const auto wpId = vm.get_state().watchpointId;

    runtime.fire_watchpoint_hit(wpId, 0x2000);
    runtime.fire_watchpoint_hit(wpId, 0x2004);
    ASSERT_EQ(vm.snapshot_entries().size(), 2u);

    const auto removesBefore = runtime.remove_watchpoint_call_count();

    runtime.fire_detached();

    EXPECT_EQ(vm.get_state().status, TrackingStatus::Idle);
    EXPECT_EQ(runtime.remove_watchpoint_call_count(), removesBefore)
        << "detach must not issue CmdRemoveWatchpoint (engine already released DR)";

    const auto entriesBefore = vm.snapshot_entries().size();
    runtime.fire_watchpoint_hit(wpId, 0x3000);
    EXPECT_EQ(vm.snapshot_entries().size(), entriesBefore)
        << "post-detach hit must be dropped (sessionEpoch bumped)";
}
