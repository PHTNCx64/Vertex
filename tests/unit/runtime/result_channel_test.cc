//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/result_channel.hh>

#include <chrono>
#include <thread>

using namespace Vertex::Runtime;
using namespace std::chrono_literals;

TEST(ResultChannelTest, PostDeliversToLateCallback)
{
    ResultChannel<int> channel {};
    EXPECT_TRUE(channel.post(42));
    int seen {0};
    channel.on_result([&](const int& value) { seen = value; });
    EXPECT_EQ(seen, 42);
}

TEST(ResultChannelTest, PostDeliversToEarlyCallback)
{
    ResultChannel<int> channel {};
    int seen {0};
    channel.on_result([&](const int& value) { seen = value; });
    EXPECT_EQ(seen, 0);
    EXPECT_TRUE(channel.post(99));
    EXPECT_EQ(seen, 99);
}

TEST(ResultChannelTest, SecondPostRejected)
{
    ResultChannel<int> channel {};
    EXPECT_TRUE(channel.post(1));
    EXPECT_FALSE(channel.post(2));
    auto result = channel.copy_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST(ResultChannelTest, WaitForTimesOut)
{
    ResultChannel<int> channel {};
    EXPECT_FALSE(channel.wait_for(20ms));
}

TEST(ResultChannelTest, WaitForUnblocksOnPost)
{
    ResultChannel<int> channel {};
    std::thread producer {[&]
    {
        std::this_thread::sleep_for(20ms);
        (void)channel.post(7);
    }};
    EXPECT_TRUE(channel.wait_for(500ms));
    producer.join();
    EXPECT_EQ(channel.copy_result().value_or(-1), 7);
}
