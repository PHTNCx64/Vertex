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

TEST(AccessTracker, StartRetryAfterFailureReachesActive)
{
    ControlledFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    DeferrableDispatcher dispatcher {};

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "start-retry"};

    const auto subsBaseline = runtime.active_subscription_count();

    runtime.push_next_add_status(STATUS_ERROR_BREAKPOINT_SET_FAILED);
    vm.start_tracking(0x1000, 4);

    EXPECT_EQ(vm.get_state().status, TrackingStatus::StartFailed);
    EXPECT_EQ(vm.get_state().watchpointId, 0u);

    vm.acknowledge_start_failure();
    EXPECT_EQ(vm.get_state().status, TrackingStatus::Idle);

    vm.start_tracking(0x1000, 4);
    EXPECT_EQ(vm.get_state().status, TrackingStatus::Active);
    EXPECT_NE(vm.get_state().watchpointId, 0u);
    EXPECT_EQ(runtime.active_subscription_count(), subsBaseline)
        << "no subscription leaked across attempts";
    EXPECT_EQ(runtime.add_watchpoint_call_count(), 2u);

    vm.stop_tracking();
}
