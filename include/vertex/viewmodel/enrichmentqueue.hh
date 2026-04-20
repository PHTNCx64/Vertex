//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace Vertex::ViewModel
{
    struct EnrichmentJob final
    {
        std::uint64_t pc {};
        std::uint32_t threadId {};
        std::uint64_t sessionEpoch {};
        std::uint64_t requestGeneration {};
    };

    class EnrichmentQueue final
    {
    public:
        static constexpr std::size_t MAX_PENDING_DISTINCT_PCS {128};
        static constexpr std::size_t MAX_IN_FLIGHT {4};
        static constexpr std::chrono::milliseconds DEFAULT_DRAIN_TIMEOUT {std::chrono::seconds{2}};

        EnrichmentQueue() = default;
        ~EnrichmentQueue() = default;

        EnrichmentQueue(const EnrichmentQueue&) = delete;
        EnrichmentQueue& operator=(const EnrichmentQueue&) = delete;
        EnrichmentQueue(EnrichmentQueue&&) = delete;
        EnrichmentQueue& operator=(EnrichmentQueue&&) = delete;

        bool enqueue(EnrichmentJob job);

        [[nodiscard]] std::optional<EnrichmentJob> pop_next();

        void complete_job() noexcept;

        [[nodiscard]] bool wait_for_drain(
            std::chrono::milliseconds timeout = DEFAULT_DRAIN_TIMEOUT) noexcept;

        void cancel_pending() noexcept;

        [[nodiscard]] std::uint64_t dropped_jobs() const noexcept;
        [[nodiscard]] std::size_t in_flight() const noexcept;
        [[nodiscard]] std::size_t pending_size() const noexcept;

    private:
        struct Node final
        {
            EnrichmentJob job {};
            Node* prev {};
            Node* next {};
        };

        void link_tail_locked(Node* node) noexcept;
        void unlink_locked(Node* node) noexcept;

        mutable std::mutex m_mutex {};
        std::condition_variable m_drainCv {};
        std::unordered_map<std::uint64_t, std::unique_ptr<Node>> m_nodesByPc {};
        Node* m_head {};
        Node* m_tail {};
        std::uint64_t m_inFlight {};
        std::atomic<std::uint64_t> m_droppedEnrichmentJobs {};
    };
}
