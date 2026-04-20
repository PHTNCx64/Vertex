//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/event/eventbus.hh>
#include <vertex/event/types/debuggerevent.hh>

#include <cstdint>
#include <optional>

namespace
{
    using Vertex::Event::DebuggerNavigateEvent;
    using Vertex::Event::EventBus;
    using Vertex::Event::NavigationTarget;
}

TEST(DebuggerNavigateEventTest, CarriesAddressAndTarget)
{
    const DebuggerNavigateEvent event {0xDEADBEEFULL, NavigationTarget::Disassembly};
    EXPECT_EQ(event.get_id(), Vertex::Event::DEBUGGER_NAVIGATE_EVENT);
    EXPECT_EQ(event.get_address(), 0xDEADBEEFULL);
    EXPECT_EQ(event.get_target(), NavigationTarget::Disassembly);
}

TEST(DebuggerNavigateEventTest, EventBusDeliversToSubscriber)
{
    EventBus bus {};
    std::optional<std::uint64_t> receivedAddress {};
    std::optional<NavigationTarget> receivedTarget {};

    const auto sub = bus.subscribe<DebuggerNavigateEvent>(
        "test-subscriber",
        Vertex::Event::DEBUGGER_NAVIGATE_EVENT,
        [&](const DebuggerNavigateEvent& event)
        {
            receivedAddress = event.get_address();
            receivedTarget = event.get_target();
        });
    ASSERT_NE(sub, 0u);

    const DebuggerNavigateEvent event {0x1000ULL, NavigationTarget::Disassembly};
    bus.broadcast(event);

    ASSERT_TRUE(receivedAddress.has_value());
    EXPECT_EQ(*receivedAddress, 0x1000ULL);
    ASSERT_TRUE(receivedTarget.has_value());
    EXPECT_EQ(*receivedTarget, NavigationTarget::Disassembly);
}

TEST(DebuggerNavigateEventTest, UnsubscribeStopsDelivery)
{
    EventBus bus {};
    int callCount {};

    const auto sub = bus.subscribe<DebuggerNavigateEvent>(
        "test-subscriber",
        Vertex::Event::DEBUGGER_NAVIGATE_EVENT,
        [&](const DebuggerNavigateEvent&)
        {
            ++callCount;
        });

    bus.broadcast(DebuggerNavigateEvent {0x1000ULL, NavigationTarget::Disassembly});
    EXPECT_EQ(callCount, 1);

    EXPECT_TRUE(bus.unsubscribe(sub));
    bus.broadcast(DebuggerNavigateEvent {0x2000ULL, NavigationTarget::Disassembly});
    EXPECT_EQ(callCount, 1);
}

TEST(DebuggerNavigateEventTest, DistinctTargetsPreserved)
{
    EventBus bus {};
    std::optional<NavigationTarget> lastTarget {};

    std::ignore = bus.subscribe<DebuggerNavigateEvent>(
        "test",
        Vertex::Event::DEBUGGER_NAVIGATE_EVENT,
        [&](const DebuggerNavigateEvent& event)
        {
            lastTarget = event.get_target();
        });

    bus.broadcast(DebuggerNavigateEvent {0x1ULL, NavigationTarget::Memory});
    EXPECT_EQ(lastTarget, NavigationTarget::Memory);

    bus.broadcast(DebuggerNavigateEvent {0x2ULL, NavigationTarget::HexEditor});
    EXPECT_EQ(lastTarget, NavigationTarget::HexEditor);
}
