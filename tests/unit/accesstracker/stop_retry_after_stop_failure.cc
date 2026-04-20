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

TEST(AccessTracker, StopRetryAfterFailureReachesIdle)
{
    ControlledFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    DeferrableDispatcher dispatcher {};

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "stop-retry"};

    vm.start_tracking(0x1000, 4);
    ASSERT_EQ(vm.get_state().status, TrackingStatus::Active);
    const auto subsBefore = runtime.active_subscription_count();

    runtime.push_next_remove_status(STATUS_ERROR_DEBUGGER_BREAK_FAILED);
    vm.stop_tracking();

    EXPECT_EQ(vm.get_state().status, TrackingStatus::StopFailed);
    EXPECT_EQ(vm.get_state().watchpointId, 100u) << "id retained until stop succeeds";
    EXPECT_EQ(runtime.active_subscription_count(), subsBefore)
        << "subscription retained while StopFailed";

    vm.stop_tracking();
    EXPECT_EQ(vm.get_state().status, TrackingStatus::Idle);
    EXPECT_EQ(vm.get_state().watchpointId, 0u);
    EXPECT_EQ(runtime.remove_watchpoint_call_count(), 2u);
}
