//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/viewmodel/enrichmentqueue.hh>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    using Vertex::ViewModel::EnrichmentJob;
    using Vertex::ViewModel::EnrichmentQueue;

    EnrichmentJob make_job(std::uint64_t pc, std::uint64_t generation = 1, std::uint64_t epoch = 1)
    {
        return EnrichmentJob {
            .pc = pc,
            .threadId = 1,
            .sessionEpoch = epoch,
            .requestGeneration = generation,
        };
    }
}

TEST(EnrichmentQueueTest, EnqueueNewPcReturnsTrue)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.enqueue(make_job(0x1000)));
    EXPECT_EQ(queue.pending_size(), 1u);
    EXPECT_EQ(queue.dropped_jobs(), 0u);
}

TEST(EnrichmentQueueTest, EnqueueExistingPcCoalesces)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.enqueue(make_job(0x1000, 1)));
    EXPECT_FALSE(queue.enqueue(make_job(0x1000, 2)));
    EXPECT_EQ(queue.pending_size(), 1u);
    EXPECT_EQ(queue.dropped_jobs(), 0u);

    const auto job = queue.pop_next();
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->pc, 0x1000u);
    EXPECT_EQ(job->requestGeneration, 2u);
}

TEST(EnrichmentQueueTest, OverflowEvictsOldestDistinctPc)
{
    EnrichmentQueue queue {};
    for (std::size_t i = 0; i < EnrichmentQueue::MAX_PENDING_DISTINCT_PCS; ++i)
    {
        EXPECT_TRUE(queue.enqueue(make_job(0x1000 + i)));
    }
    EXPECT_EQ(queue.pending_size(), EnrichmentQueue::MAX_PENDING_DISTINCT_PCS);

    EXPECT_TRUE(queue.enqueue(make_job(0x9000)));
    EXPECT_EQ(queue.pending_size(), EnrichmentQueue::MAX_PENDING_DISTINCT_PCS);
    EXPECT_EQ(queue.dropped_jobs(), 1u);

    const auto first = queue.pop_next();
    ASSERT_TRUE(first.has_value());
    EXPECT_NE(first->pc, 0x1000u);
}

TEST(EnrichmentQueueTest, CoalesceMovesToTail)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.enqueue(make_job(0x1000)));
    EXPECT_TRUE(queue.enqueue(make_job(0x2000)));
    EXPECT_TRUE(queue.enqueue(make_job(0x3000)));
    EXPECT_FALSE(queue.enqueue(make_job(0x1000, 5)));

    auto first = queue.pop_next();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->pc, 0x2000u);

    auto second = queue.pop_next();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->pc, 0x3000u);

    auto third = queue.pop_next();
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->pc, 0x1000u);
    EXPECT_EQ(third->requestGeneration, 5u);
}

TEST(EnrichmentQueueTest, PopRespectsInFlightCap)
{
    EnrichmentQueue queue {};
    for (std::size_t i = 0; i < EnrichmentQueue::MAX_IN_FLIGHT + 2; ++i)
    {
        EXPECT_TRUE(queue.enqueue(make_job(0x1000 + i)));
    }

    std::vector<EnrichmentJob> popped {};
    for (std::size_t i = 0; i < EnrichmentQueue::MAX_IN_FLIGHT; ++i)
    {
        auto job = queue.pop_next();
        ASSERT_TRUE(job.has_value()) << "failed at " << i;
        popped.push_back(*job);
    }
    EXPECT_EQ(queue.pop_next(), std::nullopt);

    queue.complete_job();
    auto another = queue.pop_next();
    EXPECT_TRUE(another.has_value());

    for (std::size_t i = 0; i < popped.size(); ++i)
    {
        queue.complete_job();
    }
    queue.complete_job();
}

TEST(EnrichmentQueueTest, WaitForDrainReturnsTrueWhenIdle)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.wait_for_drain(std::chrono::milliseconds {10}));
}

TEST(EnrichmentQueueTest, WaitForDrainTimesOutWhenBlocked)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.enqueue(make_job(0x1000)));
    const auto job = queue.pop_next();
    ASSERT_TRUE(job.has_value());

    EXPECT_FALSE(queue.wait_for_drain(std::chrono::milliseconds {50}));
    EXPECT_EQ(queue.in_flight(), 1u);

    queue.complete_job();
    EXPECT_TRUE(queue.wait_for_drain(std::chrono::milliseconds {100}));
    EXPECT_EQ(queue.in_flight(), 0u);
}

TEST(EnrichmentQueueTest, CompleteJobWakesConcurrentDrain)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.enqueue(make_job(0x1000)));
    const auto job = queue.pop_next();
    ASSERT_TRUE(job.has_value());

    std::atomic<bool> drained {false};
    std::thread waiter {[&]
    {
        drained.store(queue.wait_for_drain(std::chrono::seconds {2}));
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds {50});
    queue.complete_job();
    waiter.join();

    EXPECT_TRUE(drained.load());
}

TEST(EnrichmentQueueTest, CancelPendingClearsQueueKeepsInFlight)
{
    EnrichmentQueue queue {};
    EXPECT_TRUE(queue.enqueue(make_job(0x1000)));
    EXPECT_TRUE(queue.enqueue(make_job(0x2000)));
    const auto popped = queue.pop_next();
    ASSERT_TRUE(popped.has_value());

    EXPECT_EQ(queue.pending_size(), 1u);
    EXPECT_EQ(queue.in_flight(), 1u);

    queue.cancel_pending();
    EXPECT_EQ(queue.pending_size(), 0u);
    EXPECT_EQ(queue.in_flight(), 1u);
    EXPECT_EQ(queue.pop_next(), std::nullopt);

    queue.complete_job();
    EXPECT_EQ(queue.in_flight(), 0u);
}

TEST(EnrichmentQueueTest, DroppedCounterSurvivesMultipleOverflows)
{
    EnrichmentQueue queue {};
    for (std::size_t i = 0; i < EnrichmentQueue::MAX_PENDING_DISTINCT_PCS; ++i)
    {
        EXPECT_TRUE(queue.enqueue(make_job(0x1000 + i)));
    }
    for (std::size_t extra = 0; extra < 10; ++extra)
    {
        EXPECT_TRUE(queue.enqueue(make_job(0xA000 + extra)));
    }
    EXPECT_EQ(queue.pending_size(), EnrichmentQueue::MAX_PENDING_DISTINCT_PCS);
    EXPECT_EQ(queue.dropped_jobs(), 10u);
}
