//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/drain.hh>

#include <chrono>
#include <thread>

using namespace Vertex::Runtime;
using namespace std::chrono_literals;

TEST(BoundedDrainTest, StartsEmpty)
{
    BoundedDrain drain {};
    EXPECT_EQ(drain.in_flight(), 0u);
    EXPECT_TRUE(drain.wait(1ms));
}

TEST(BoundedDrainTest, AcquireReleaseTracks)
{
    BoundedDrain drain {};
    drain.acquire();
    drain.acquire();
    EXPECT_EQ(drain.in_flight(), 2u);
    drain.release();
    EXPECT_EQ(drain.in_flight(), 1u);
    drain.release();
    EXPECT_EQ(drain.in_flight(), 0u);
}

TEST(BoundedDrainTest, WaitTimesOutWhileInFlight)
{
    BoundedDrain drain {};
    drain.acquire();
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(drain.wait(50ms));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, 40ms);
    drain.release();
}

TEST(BoundedDrainTest, WaitUnblocksAfterRelease)
{
    BoundedDrain drain {};
    drain.acquire();

    std::thread releaser {[&]
    {
        std::this_thread::sleep_for(20ms);
        drain.release();
    }};

    EXPECT_TRUE(drain.wait(500ms));
    releaser.join();
}

TEST(BoundedDrainTest, GuardReleasesOnScopeExit)
{
    BoundedDrain drain {};
    {
        BoundedDrainGuard guard {drain};
        EXPECT_EQ(drain.in_flight(), 1u);
    }
    EXPECT_EQ(drain.in_flight(), 0u);
}
