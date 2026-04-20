//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <atomic>
#include <cstdint>

namespace Vertex::Runtime
{
    class SessionEpoch final
    {
    public:
        SessionEpoch() = default;
        ~SessionEpoch() = default;

        SessionEpoch(const SessionEpoch&) = delete;
        SessionEpoch& operator=(const SessionEpoch&) = delete;
        SessionEpoch(SessionEpoch&&) = delete;
        SessionEpoch& operator=(SessionEpoch&&) = delete;

        [[nodiscard]] std::uint64_t load() const noexcept;
        std::uint64_t bump() noexcept;

    private:
        std::atomic<std::uint64_t> m_value {0};
    };
}
