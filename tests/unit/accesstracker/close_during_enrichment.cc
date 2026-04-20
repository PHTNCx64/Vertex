//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//








#include <gtest/gtest.h>

#include <vertex/model/accesstrackermodel.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include "../../mocks/MockILog.hh"
#include "../../support/accesstracker_common.hh"

#include <memory>

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

TEST(AccessTracker, DtorWithDeferredEnrichmentWorkersNoUAF)
{
    ControlledFakeRuntime runtime {};
    Vertex::Testing::Mocks::MockILog log {};
    silence_log(log);
    DeferrableDispatcher dispatcher {};
    dispatcher.set_deferred(true);

    std::uint32_t wpId {};
    {
        auto model = std::make_unique<AccessTrackerModel>(runtime);
        AccessTrackerViewModel vm {std::move(model), dispatcher, log, "close-enrich"};

        vm.start_tracking(0x1000, 4);
        ASSERT_EQ(vm.get_state().status, TrackingStatus::Active);
        wpId = vm.get_state().watchpointId;

        for (std::uint64_t pc = 0x2000; pc < 0x2000 + 32; ++pc)
        {
            runtime.fire_watchpoint_hit(wpId, pc);
        }

        EXPECT_GT(dispatcher.deferred_count(), 0u);
    }

    const auto drained = dispatcher.flush_deferred();
    EXPECT_GT(drained, 0u);
}
