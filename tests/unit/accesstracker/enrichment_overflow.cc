//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//







#include <gtest/gtest.h>

#include <vertex/model/accesstrackermodel.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>
#include <vertex/viewmodel/enrichmentqueue.hh>

#include "../../mocks/MockILog.hh"
#include "../../support/accesstracker_common.hh"

namespace
{
    using Vertex::Testing::AccessTracker::ControlledFakeRuntime;
    using Vertex::Testing::AccessTracker::DeferrableDispatcher;
    using Vertex::Model::AccessTrackerModel;
    using Vertex::ViewModel::AccessTrackerViewModel;
    using Vertex::ViewModel::EnrichmentQueue;
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

TEST(AccessTracker, EnrichmentQueueCapsAtMaxPendingDistinctPcs)
{
    EnrichmentQueue queue {};

    for (std::size_t i = 0; i < EnrichmentQueue::MAX_PENDING_DISTINCT_PCS; ++i)
    {
        EXPECT_TRUE(queue.enqueue(Vertex::ViewModel::EnrichmentJob {
            .pc = 0x1000 + i,
            .threadId = 1,
            .sessionEpoch = 1,
            .requestGeneration = 1,
        }));
    }
    EXPECT_EQ(queue.pending_size(), EnrichmentQueue::MAX_PENDING_DISTINCT_PCS);
    EXPECT_EQ(queue.dropped_jobs(), 0u);

    constexpr std::size_t OVERFLOW {32};
    for (std::size_t i = 0; i < OVERFLOW; ++i)
    {
        (void)queue.enqueue(Vertex::ViewModel::EnrichmentJob {
            .pc = 0x9000 + i,
            .threadId = 1,
            .sessionEpoch = 1,
            .requestGeneration = 1,
        });
        EXPECT_LE(queue.pending_size(), EnrichmentQueue::MAX_PENDING_DISTINCT_PCS);
    }
    EXPECT_EQ(queue.pending_size(), EnrichmentQueue::MAX_PENDING_DISTINCT_PCS);
    EXPECT_EQ(queue.dropped_jobs(), OVERFLOW);
}
