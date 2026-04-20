//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace Vertex::Runtime
{
    class BoundedDrain final
    {
    public:
        BoundedDrain() = default;
        ~BoundedDrain() = default;

        BoundedDrain(const BoundedDrain&) = delete;
        BoundedDrain& operator=(const BoundedDrain&) = delete;
        BoundedDrain(BoundedDrain&&) = delete;
        BoundedDrain& operator=(BoundedDrain&&) = delete;

        void acquire() noexcept;
        void release() noexcept;

        [[nodiscard]] bool wait(std::chrono::milliseconds timeout) noexcept;
        [[nodiscard]] std::uint32_t in_flight() const noexcept;

    private:
        mutable std::mutex m_mutex {};
        std::condition_variable m_cv {};
        std::uint32_t m_inFlight {0};
    };

    class BoundedDrainGuard final
    {
    public:
        explicit BoundedDrainGuard(BoundedDrain& drain) noexcept;
        ~BoundedDrainGuard();

        BoundedDrainGuard(const BoundedDrainGuard&) = delete;
        BoundedDrainGuard& operator=(const BoundedDrainGuard&) = delete;
        BoundedDrainGuard(BoundedDrainGuard&&) = delete;
        BoundedDrainGuard& operator=(BoundedDrainGuard&&) = delete;

    private:
        BoundedDrain& m_drain;
    };
}
