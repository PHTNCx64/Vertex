//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//








#include <gtest/gtest.h>

#include <vertex/runtime/command.hh>
#include <vertex/runtime/fanout.hh>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace
{
    enum class TestKind : std::uint32_t
    {
        None = 0,
        Tick = 1u << 0,
    };

    struct TestEvent final
    {
        TestKind kind {TestKind::Tick};
        std::uint64_t seq {};
    };
}

TEST(AccessTracker, FanoutUnsubscribeDrainsInFlightCallbacks)
{
    using Fan = Vertex::Runtime::Fanout<TestKind, TestEvent>;
    Fan fanout {};

    std::atomic<bool> stop {false};
    std::atomic<bool> violation {false};
    constexpr auto MASK = static_cast<Fan::Mask>(TestKind::Tick);

    std::thread firer {[&] {
        std::uint64_t seq {};
        while (!stop.load(std::memory_order_acquire))
        {
            fanout.fire(TestEvent {.kind = TestKind::Tick, .seq = seq++});
        }
    }};

    std::thread churner {[&] {
        while (!stop.load(std::memory_order_acquire))
        {
            std::atomic<bool> callbackAlive {true};
            const auto id = fanout.subscribe(MASK,
                [&callbackAlive, &violation](const TestEvent&)
                {
                    if (!callbackAlive.load(std::memory_order_acquire))
                    {
                        violation.store(true, std::memory_order_release);
                    }
                });

            fanout.unsubscribe(id);
            callbackAlive.store(false, std::memory_order_release);
        }
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    stop.store(true, std::memory_order_release);
    firer.join();
    churner.join();

    EXPECT_FALSE(violation.load(std::memory_order_acquire))
        << "callback executed after unsubscribe returned";
    EXPECT_EQ(fanout.subscriber_count(), 0u);
}
