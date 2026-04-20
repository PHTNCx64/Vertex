//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/runtime/lifetime_token.hh>

#include <atomic>
#include <thread>

using namespace Vertex::Runtime;

TEST(LifetimeTokenTest, NewTokenAlive)
{
    LifetimeToken token {};
    EXPECT_TRUE(token.alive());
    EXPECT_FALSE(token.weak().expired());
}

TEST(LifetimeTokenTest, KillMarksDead)
{
    LifetimeToken token {};
    token.kill();
    EXPECT_FALSE(token.alive());
}

TEST(LifetimeTokenTest, KillIsIdempotent)
{
    LifetimeToken token {};
    token.kill();
    token.kill();
    EXPECT_FALSE(token.alive());
}

TEST(LifetimeTokenTest, WeakBindInvokesWhenAlive)
{
    struct Target final
    {
        int hits {0};
        void on_event(int value) { hits += value; }
    };

    LifetimeToken token {};
    Target target {};
    auto fn = weak_bind(&target, token, [](Target* self, int value)
    {
        self->on_event(value);
    });

    fn(7);
    EXPECT_EQ(target.hits, 7);
}

TEST(LifetimeTokenTest, WeakBindNoopAfterKill)
{
    struct Target final
    {
        int hits {0};
    };

    LifetimeToken token {};
    Target target {};
    auto fn = weak_bind(&target, token, [](Target* self, int value)
    {
        self->hits += value;
    });

    token.kill();
    fn(7);
    EXPECT_EQ(target.hits, 0);
}

TEST(LifetimeTokenTest, WeakBindNoopAfterDestruction)
{
    struct Target final
    {
        int hits {0};
    };

    Target target {};
    std::function<void(int)> fn {};
    {
        LifetimeToken token {};
        fn = weak_bind(&target, token, [](Target* self, int value)
        {
            self->hits += value;
        });
    }
    fn(7);
    EXPECT_EQ(target.hits, 0);
}
