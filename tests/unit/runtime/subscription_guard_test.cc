//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/subscription_guard.hh>

#include <vector>

using namespace Vertex::Runtime;

namespace
{
    struct FakeService final
    {
        std::vector<SubscriptionId> unsubscribed {};
        void unsubscribe(SubscriptionId id) noexcept
        {
            unsubscribed.push_back(id);
        }
    };
}

TEST(SubscriptionGuardTest, DefaultIsEmpty)
{
    SubscriptionGuard<FakeService> guard {};
    EXPECT_EQ(guard.id(), INVALID_SUBSCRIPTION_ID);
    EXPECT_FALSE(static_cast<bool>(guard));
}

TEST(SubscriptionGuardTest, DtorUnsubscribes)
{
    FakeService service {};
    {
        SubscriptionGuard<FakeService> guard {service, 42};
        EXPECT_EQ(guard.id(), 42u);
        EXPECT_TRUE(static_cast<bool>(guard));
    }
    ASSERT_EQ(service.unsubscribed.size(), 1u);
    EXPECT_EQ(service.unsubscribed[0], 42u);
}

TEST(SubscriptionGuardTest, ResetUnsubscribesOnce)
{
    FakeService service {};
    SubscriptionGuard<FakeService> guard {service, 7};
    guard.reset();
    EXPECT_FALSE(static_cast<bool>(guard));
    guard.reset();
    ASSERT_EQ(service.unsubscribed.size(), 1u);
}

TEST(SubscriptionGuardTest, MoveTransfers)
{
    FakeService service {};
    SubscriptionGuard<FakeService> a {service, 11};
    SubscriptionGuard<FakeService> b {std::move(a)};
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_TRUE(static_cast<bool>(b));
    EXPECT_EQ(b.id(), 11u);
    b.reset();
    ASSERT_EQ(service.unsubscribed.size(), 1u);
    EXPECT_EQ(service.unsubscribed[0], 11u);
}

TEST(SubscriptionGuardTest, MoveAssignUnsubscribesPrior)
{
    FakeService service {};
    SubscriptionGuard<FakeService> a {service, 1};
    SubscriptionGuard<FakeService> b {service, 2};
    b = std::move(a);
    ASSERT_EQ(service.unsubscribed.size(), 1u);
    EXPECT_EQ(service.unsubscribed[0], 2u);
    EXPECT_EQ(b.id(), 1u);
}

TEST(SubscriptionGuardTest, InvalidIdSkipsUnsubscribe)
{
    FakeService service {};
    {
        SubscriptionGuard<FakeService> guard {service, INVALID_SUBSCRIPTION_ID};
    }
    EXPECT_TRUE(service.unsubscribed.empty());
}
