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

TEST(AccessTracker, ClearDuringEnrichmentDropsPendingAndRepopulates)
{
    ControlledFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    DeferrableDispatcher dispatcher {};
    dispatcher.set_deferred(true);

    auto model = std::make_unique<AccessTrackerModel>(runtime);
    AccessTrackerViewModel vm {std::move(model), dispatcher, log, "clear-enrich"};

    vm.start_tracking(0x1000, 4);
    const auto wpId = vm.get_state().watchpointId;

    for (std::uint64_t pc = 0x2000; pc < 0x2000 + 16; ++pc)
    {
        runtime.fire_watchpoint_hit(wpId, pc);
    }
    ASSERT_EQ(vm.snapshot_entries().size(), 16u);
    EXPECT_GT(dispatcher.deferred_count(), 0u);

    vm.clear();
    EXPECT_EQ(vm.snapshot_entries().size(), 0u);

    dispatcher.flush_deferred();
    EXPECT_EQ(vm.snapshot_entries().size(), 0u)
        << "workers with stale sessionEpoch must not resurrect entries";

    runtime.fire_watchpoint_hit(wpId, 0x4000);
    EXPECT_EQ(vm.snapshot_entries().size(), 1u);
    EXPECT_EQ(vm.snapshot_entries().front().instructionAddress, 0x4000u);

    dispatcher.set_deferred(false);
    dispatcher.flush_deferred();
    vm.stop_tracking();
}
