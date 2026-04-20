//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/fanout.hh>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace Vertex::Runtime;
using namespace std::chrono_literals;

namespace
{
    enum class Kind : std::uint32_t
    {
        None = 0,
        A    = 1u << 0,
        B    = 1u << 1,
        C    = 1u << 2,
    };

    struct Event final
    {
        Kind kind {Kind::None};
        int value {};
    };
}

TEST(FanoutTest, SubscribeFireInvokesCallback)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    const auto id = fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                                      [&](const Event& e) { hits += e.value; });
    EXPECT_NE(id, INVALID_SUBSCRIPTION_ID);

    fanout.fire(Event {Kind::A, 5});
    EXPECT_EQ(hits.load(), 5);

    fanout.fire(Event {Kind::B, 10});
    EXPECT_EQ(hits.load(), 5);
}

TEST(FanoutTest, MaskBitwiseMatch)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    const auto id = fanout.subscribe(
        static_cast<std::uint32_t>(Kind::A) | static_cast<std::uint32_t>(Kind::C),
        [&](const Event& e) { hits += e.value; });
    EXPECT_NE(id, INVALID_SUBSCRIPTION_ID);

    fanout.fire(Event {Kind::A, 1});
    fanout.fire(Event {Kind::B, 100});
    fanout.fire(Event {Kind::C, 2});
    EXPECT_EQ(hits.load(), 3);
}

TEST(FanoutTest, ZeroMaskReturnsInvalid)
{
    Fanout<Kind, Event> fanout {};
    const auto id = fanout.subscribe(0, [](const Event&) {});
    EXPECT_EQ(id, INVALID_SUBSCRIPTION_ID);
}

TEST(FanoutTest, UnsubscribeStopsDispatch)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    const auto id = fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                                      [&](const Event& e) { hits += e.value; });
    fanout.unsubscribe(id);
    fanout.fire(Event {Kind::A, 42});
    EXPECT_EQ(hits.load(), 0);
}

TEST(FanoutTest, UnsubscribeDrainsInFlight)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> entered {0};
    std::atomic<bool> release {false};
    const auto id = fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
        [&](const Event&)
        {
            ++entered;
            while (!release.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(1ms);
            }
        });

    std::thread firer {[&] { fanout.fire(Event {Kind::A, 1}); }};
    while (entered.load() == 0) std::this_thread::sleep_for(1ms);

    std::atomic<bool> unsubscribed {false};
    std::thread unsubber {[&]
    {
        fanout.unsubscribe(id);
        unsubscribed.store(true);
    }};

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(unsubscribed.load());

    release.store(true);
    firer.join();
    unsubber.join();
    EXPECT_TRUE(unsubscribed.load());
}

TEST(FanoutTest, ShutdownPreventsFurtherSubscribe)
{
    Fanout<Kind, Event> fanout {};
    fanout.shutdown();
    const auto id = fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                                      [](const Event&) {});
    EXPECT_EQ(id, INVALID_SUBSCRIPTION_ID);
}

TEST(FanoutTest, ShutdownIsIdempotent)
{
    Fanout<Kind, Event> fanout {};
    (void)fanout.subscribe(static_cast<std::uint32_t>(Kind::A), [](const Event&) {});
    fanout.shutdown();
    fanout.shutdown();
    SUCCEED();
}

TEST(FanoutTest, SelfUnsubscribeInsideCallbackNoDeadlock)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    std::atomic<SubscriptionId> storedId {INVALID_SUBSCRIPTION_ID};

    const auto id = fanout.subscribe(
        static_cast<std::uint32_t>(Kind::A),
        [&](const Event&)
        {
            ++hits;
            fanout.unsubscribe(storedId.load());
        });
    storedId.store(id);

    fanout.fire(Event {Kind::A, 1});
    EXPECT_EQ(hits.load(), 1);

    fanout.fire(Event {Kind::A, 2});
    EXPECT_EQ(hits.load(), 1);
}

TEST(FanoutTest, CrossUnsubscribeInsideCallbackNoDeadlock)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hitsA {0};
    std::atomic<int> hitsB {0};
    std::atomic<SubscriptionId> idB {INVALID_SUBSCRIPTION_ID};

    (void)fanout.subscribe(
        static_cast<std::uint32_t>(Kind::A),
        [&](const Event&)
        {
            ++hitsA;
            fanout.unsubscribe(idB.load());
        });
    idB.store(fanout.subscribe(
        static_cast<std::uint32_t>(Kind::A),
        [&](const Event&) { ++hitsB; }));

    fanout.fire(Event {Kind::A, 1});
    fanout.fire(Event {Kind::A, 2});
    EXPECT_EQ(hitsA.load(), 2);
    EXPECT_LE(hitsB.load(), 1);
}

TEST(FanoutTest, ThrowingCallbackDoesNotWedgeDrain)
{
    Fanout<Kind, Event> fanout {};
    const auto id = fanout.subscribe(
        static_cast<std::uint32_t>(Kind::A),
        [](const Event&) { throw std::runtime_error {"boom"}; });

    try
    {
        fanout.fire(Event {Kind::A, 1});
        FAIL() << "expected throw";
    }
    catch (const std::runtime_error&)
    {
    }

    fanout.unsubscribe(id);
    SUCCEED();
}

TEST(FanoutTest, ThrowingCallbackDoesNotWedgeShutdown)
{
    Fanout<Kind, Event> fanout {};
    (void)fanout.subscribe(
        static_cast<std::uint32_t>(Kind::A),
        [](const Event&) { throw std::runtime_error {"boom"}; });

    try
    {
        fanout.fire(Event {Kind::A, 1});
        FAIL() << "expected throw";
    }
    catch (const std::runtime_error&)
    {
    }

    fanout.shutdown();
    SUCCEED();
}

TEST(FanoutTest, FireAfterShutdownIsNoop)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    (void)fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                           [&](const Event&) { ++hits; });
    fanout.shutdown();
    fanout.fire(Event {Kind::A, 1});
    EXPECT_EQ(hits.load(), 0);
}

TEST(FanoutTest, UnsubscribeInvalidIdNoop)
{
    Fanout<Kind, Event> fanout {};
    fanout.unsubscribe(INVALID_SUBSCRIPTION_ID);
    fanout.unsubscribe(999999);
    SUCCEED();
}

TEST(FanoutTest, NullCallbackRejected)
{
    Fanout<Kind, Event> fanout {};
    Fanout<Kind, Event>::Callback empty {};
    const auto id = fanout.subscribe(static_cast<std::uint32_t>(Kind::A), std::move(empty));
    EXPECT_EQ(id, INVALID_SUBSCRIPTION_ID);
}

TEST(FanoutTest, ConcurrentFireFromMultipleThreads)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    (void)fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                           [&](const Event& e) { hits.fetch_add(e.value); });

    constexpr int threads {8};
    constexpr int perThread {250};
    std::vector<std::thread> workers {};
    workers.reserve(threads);
    for (int i = 0; i < threads; ++i)
    {
        workers.emplace_back([&]
        {
            for (int j = 0; j < perThread; ++j)
            {
                fanout.fire(Event {Kind::A, 1});
            }
        });
    }
    for (auto& w : workers) w.join();
    EXPECT_EQ(hits.load(), threads * perThread);
}

TEST(FanoutTest, UnsubscribeRaceWithFire)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> hits {0};
    std::atomic<bool> stop {false};

    std::vector<SubscriptionId> ids {};
    for (int i = 0; i < 16; ++i)
    {
        ids.push_back(fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                                        [&](const Event&) { ++hits; }));
    }

    std::thread firer {[&]
    {
        while (!stop.load(std::memory_order_acquire))
        {
            fanout.fire(Event {Kind::A, 1});
        }
    }};

    for (auto id : ids)
    {
        fanout.unsubscribe(id);
    }
    stop.store(true, std::memory_order_release);
    firer.join();
    SUCCEED();
}

TEST(FanoutTest, MultipleSubscribersReceiveInParallel)
{
    Fanout<Kind, Event> fanout {};
    std::atomic<int> a {0};
    std::atomic<int> b {0};
    (void)fanout.subscribe(static_cast<std::uint32_t>(Kind::A),
                           [&](const Event& e) { a += e.value; });
    (void)fanout.subscribe(static_cast<std::uint32_t>(Kind::A) | static_cast<std::uint32_t>(Kind::B),
                           [&](const Event& e) { b += e.value; });

    fanout.fire(Event {Kind::A, 2});
    fanout.fire(Event {Kind::B, 3});
    EXPECT_EQ(a.load(), 2);
    EXPECT_EQ(b.load(), 5);
}
