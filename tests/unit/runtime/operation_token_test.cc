//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/operation_token.hh>

#include <functional>

using namespace Vertex::Runtime;

namespace
{
    using Completion = std::function<void(int)>;
}

TEST(OperationTokenTest, EmptyOnConstruct)
{
    OperationToken<Completion> token {};
    EXPECT_EQ(token.current_locked(), 0u);
    EXPECT_FALSE(token.has_pending_locked());
    EXPECT_FALSE(token.matches_locked(0));
}

TEST(OperationTokenTest, BumpCreatesNewOp)
{
    OperationToken<Completion> token {};
    int called {};
    auto first = token.bump_locked([&](int v) { called = v; });
    EXPECT_FALSE(first.has_value());
    EXPECT_EQ(token.current_locked(), 1u);
    EXPECT_TRUE(token.matches_locked(1));
    EXPECT_TRUE(token.has_pending_locked());
}

TEST(OperationTokenTest, BumpInvalidatesPrior)
{
    OperationToken<Completion> token {};
    auto first = token.bump_locked([](int) {});
    EXPECT_FALSE(first.has_value());

    auto prior = token.bump_locked([](int) {});
    ASSERT_TRUE(prior.has_value());
    EXPECT_EQ(prior->opId, 1u);
    EXPECT_EQ(token.current_locked(), 2u);
    EXPECT_TRUE(token.matches_locked(2));
    EXPECT_FALSE(token.matches_locked(1));
}

TEST(OperationTokenTest, ConsumeReturnsMatching)
{
    OperationToken<Completion> token {};
    (void)token.bump_locked([](int) {});
    auto consumed = token.consume_locked(1);
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(consumed->opId, 1u);
    EXPECT_FALSE(token.has_pending_locked());
}

TEST(OperationTokenTest, ConsumeMismatchNoop)
{
    OperationToken<Completion> token {};
    (void)token.bump_locked([](int) {});
    auto consumed = token.consume_locked(99);
    EXPECT_FALSE(consumed.has_value());
    EXPECT_TRUE(token.has_pending_locked());
}

TEST(OperationTokenTest, InvalidatedCallbackDispatchable)
{
    OperationToken<Completion> token {};
    int sum {0};

    (void)token.bump_locked([&](int v) { sum += v; });
    auto invalidated = token.bump_locked([&](int v) { sum += v * 100; });
    ASSERT_TRUE(invalidated.has_value());

    invalidated->onComplete(7);
    EXPECT_EQ(sum, 7);

    auto current = token.consume_locked(token.current_locked());
    ASSERT_TRUE(current.has_value());
    current->onComplete(5);
    EXPECT_EQ(sum, 7 + 500);
}
