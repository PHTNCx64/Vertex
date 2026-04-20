//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/lifetime_token.hh>

namespace Vertex::Runtime
{
    LifetimeToken::LifetimeToken()
        : m_alive {std::make_shared<std::atomic<bool>>(true)}
    {
    }

    LifetimeToken::~LifetimeToken()
    {
        kill();
    }

    void LifetimeToken::kill() const noexcept
    {
        if (m_alive)
        {
            m_alive->store(false, std::memory_order_release);
        }
    }

    bool LifetimeToken::alive() const noexcept
    {
        return m_alive && m_alive->load(std::memory_order_acquire);
    }

    std::weak_ptr<std::atomic<bool>> LifetimeToken::weak() const noexcept
    {
        return {m_alive};
    }
}
