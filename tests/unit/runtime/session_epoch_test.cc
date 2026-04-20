//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/session_epoch.hh>

#include <algorithm>
#include <thread>
#include <vector>

using namespace Vertex::Runtime;

TEST(SessionEpochTest, StartsAtZero)
{
    SessionEpoch epoch {};
    EXPECT_EQ(epoch.load(), 0u);
}

TEST(SessionEpochTest, BumpIncreasesMonotonically)
{
    SessionEpoch epoch {};
    EXPECT_EQ(epoch.bump(), 1u);
    EXPECT_EQ(epoch.bump(), 2u);
    EXPECT_EQ(epoch.load(), 2u);
}

TEST(SessionEpochTest, ConcurrentBumpsAreUnique)
{
    SessionEpoch epoch {};
    constexpr int threadCount {16};
    constexpr int bumpsPerThread {500};

    std::vector<std::thread> workers {};
    std::vector<std::vector<std::uint64_t>> results(threadCount);

    for (int i = 0; i < threadCount; ++i)
    {
        workers.emplace_back([&, i]
        {
            results[i].reserve(bumpsPerThread);
            for (int j = 0; j < bumpsPerThread; ++j)
            {
                results[i].push_back(epoch.bump());
            }
        });
    }
    for (auto& w : workers) w.join();

    std::vector<std::uint64_t> all {};
    for (const auto& r : results) all.insert(all.end(), r.begin(), r.end());
    std::sort(all.begin(), all.end());
    EXPECT_EQ(all.front(), 1u);
    EXPECT_EQ(all.back(), threadCount * bumpsPerThread);
    EXPECT_EQ(std::adjacent_find(all.begin(), all.end()), all.end());
}
