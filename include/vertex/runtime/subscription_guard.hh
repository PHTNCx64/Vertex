//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/command.hh>

#include <utility>

namespace Vertex::Runtime
{
    template<class TService>
    class SubscriptionGuard final
    {
    public:
        SubscriptionGuard() noexcept = default;

        SubscriptionGuard(TService& service, SubscriptionId id) noexcept
            : m_service {&service}
            , m_id {id}
        {
        }

        ~SubscriptionGuard()
        {
            reset();
        }

        SubscriptionGuard(const SubscriptionGuard&) = delete;
        SubscriptionGuard& operator=(const SubscriptionGuard&) = delete;

        SubscriptionGuard(SubscriptionGuard&& other) noexcept
            : m_service {std::exchange(other.m_service, nullptr)}
            , m_id {std::exchange(other.m_id, INVALID_SUBSCRIPTION_ID)}
        {
        }

        SubscriptionGuard& operator=(SubscriptionGuard&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                m_service = std::exchange(other.m_service, nullptr);
                m_id = std::exchange(other.m_id, INVALID_SUBSCRIPTION_ID);
            }
            return *this;
        }

        void reset() noexcept
        {
            if (m_service != nullptr && m_id != INVALID_SUBSCRIPTION_ID)
            {
                m_service->unsubscribe(m_id);
            }
            m_service = nullptr;
            m_id = INVALID_SUBSCRIPTION_ID;
        }

        [[nodiscard]] SubscriptionId id() const noexcept
        {
            return m_id;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_service != nullptr && m_id != INVALID_SUBSCRIPTION_ID;
        }

    private:
        TService* m_service {nullptr};
        SubscriptionId m_id {INVALID_SUBSCRIPTION_ID};
    };
}
