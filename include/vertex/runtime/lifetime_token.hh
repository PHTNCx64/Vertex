//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

namespace Vertex::Runtime
{
    class LifetimeToken final
    {
    public:
        LifetimeToken();
        ~LifetimeToken();

        LifetimeToken(const LifetimeToken&) = delete;
        LifetimeToken& operator=(const LifetimeToken&) = delete;
        LifetimeToken(LifetimeToken&&) = delete;
        LifetimeToken& operator=(LifetimeToken&&) = delete;

        void kill() const noexcept;
        [[nodiscard]] bool alive() const noexcept;
        [[nodiscard]] std::weak_ptr<std::atomic<bool>> weak() const noexcept;

    private:
        std::shared_ptr<std::atomic<bool>> m_alive {};
    };

    template<class Self, class Fn>
    [[nodiscard]] auto weak_bind(Self* self, const LifetimeToken& token, Fn fn)
    {
        return [self, weak = token.weak(), fn = std::move(fn)](auto&&... args) mutable
        {
            auto alive = weak.lock();
            if (!alive || !alive->load(std::memory_order_acquire))
            {
                return;
            }
            std::invoke(fn, self, std::forward<decltype(args)>(args)...);
        };
    }
}
