//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

namespace Vertex::Runtime
{
    template<class TCompletion>
    class OperationToken final
    {
    public:
        struct Pending final
        {
            std::uint64_t opId {0};
            TCompletion onComplete {};
        };

        OperationToken() = default;
        ~OperationToken() = default;

        OperationToken(const OperationToken&) = delete;
        OperationToken& operator=(const OperationToken&) = delete;
        OperationToken(OperationToken&&) = delete;
        OperationToken& operator=(OperationToken&&) = delete;

        [[nodiscard]] std::optional<Pending> bump_locked(TCompletion onComplete)
        {
            std::optional<Pending> invalidated {std::move(m_pending)};
            m_pending.reset();
            m_current = m_next.fetch_add(1, std::memory_order_relaxed) + 1;
            m_pending = Pending {m_current, std::move(onComplete)};
            return invalidated;
        }

        [[nodiscard]] std::uint64_t current_locked() const noexcept
        {
            return m_current;
        }

        [[nodiscard]] bool matches_locked(std::uint64_t opId) const noexcept
        {
            return opId != 0 && opId == m_current;
        }

        [[nodiscard]] std::optional<Pending> consume_locked(std::uint64_t opId)
        {
            if (!m_pending || m_pending->opId != opId)
            {
                return std::nullopt;
            }
            std::optional<Pending> out {std::move(m_pending)};
            m_pending.reset();
            return out;
        }

        [[nodiscard]] std::optional<Pending> consume_current_locked()
        {
            if (!m_pending)
            {
                return std::nullopt;
            }
            std::optional<Pending> out {std::move(m_pending)};
            m_pending.reset();
            return out;
        }

        [[nodiscard]] bool has_pending_locked() const noexcept
        {
            return m_pending.has_value();
        }

        [[nodiscard]] std::uint64_t peek_next() const noexcept
        {
            return m_next.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<std::uint64_t> m_next {0};
        std::uint64_t m_current {0};
        std::optional<Pending> m_pending {};
    };
}
