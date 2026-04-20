//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/enrichmentqueue.hh>

#include <utility>

namespace Vertex::ViewModel
{
    void EnrichmentQueue::link_tail_locked(Node* node) noexcept
    {
        node->prev = m_tail;
        node->next = nullptr;
        if (m_tail)
        {
            m_tail->next = node;
        }
        else
        {
            m_head = node;
        }
        m_tail = node;
    }

    void EnrichmentQueue::unlink_locked(Node* node) noexcept
    {
        if (node->prev)
        {
            node->prev->next = node->next;
        }
        else
        {
            m_head = node->next;
        }
        if (node->next)
        {
            node->next->prev = node->prev;
        }
        else
        {
            m_tail = node->prev;
        }
        node->prev = nullptr;
        node->next = nullptr;
    }

    bool EnrichmentQueue::enqueue(EnrichmentJob job)
    {
        std::scoped_lock lock {m_mutex};

        if (const auto it = m_nodesByPc.find(job.pc); it != m_nodesByPc.end())
        {
            Node* const node = it->second.get();
            node->job = job;
            unlink_locked(node);
            link_tail_locked(node);
            return false;
        }

        if (m_nodesByPc.size() >= MAX_PENDING_DISTINCT_PCS)
        {
            Node* const oldest = m_head;
            if (oldest)
            {
                const auto oldestPc = oldest->job.pc;
                unlink_locked(oldest);
                m_nodesByPc.erase(oldestPc);
                m_droppedEnrichmentJobs.fetch_add(1, std::memory_order_relaxed);
            }
        }

        auto node = std::make_unique<Node>();
        node->job = job;
        Node* const rawNode = node.get();
        m_nodesByPc.emplace(job.pc, std::move(node));
        link_tail_locked(rawNode);
        return true;
    }

    std::optional<EnrichmentJob> EnrichmentQueue::pop_next()
    {
        std::scoped_lock lock {m_mutex};

        if (m_inFlight >= MAX_IN_FLIGHT)
        {
            return std::nullopt;
        }
        if (!m_head)
        {
            return std::nullopt;
        }

        Node* const node = m_head;
        const EnrichmentJob job = node->job;
        const auto pc = job.pc;
        unlink_locked(node);
        m_nodesByPc.erase(pc);
        ++m_inFlight;
        return job;
    }

    void EnrichmentQueue::complete_job() noexcept
    {
        {
            std::scoped_lock lock {m_mutex};
            if (m_inFlight > 0)
            {
                --m_inFlight;
            }
        }
        m_drainCv.notify_all();
    }

    bool EnrichmentQueue::wait_for_drain(std::chrono::milliseconds timeout) noexcept
    {
        std::unique_lock lock {m_mutex};
        return m_drainCv.wait_for(lock, timeout,
            [this] { return m_inFlight == 0; });
    }

    void EnrichmentQueue::cancel_pending() noexcept
    {
        std::scoped_lock lock {m_mutex};
        m_nodesByPc.clear();
        m_head = nullptr;
        m_tail = nullptr;
    }

    std::uint64_t EnrichmentQueue::dropped_jobs() const noexcept
    {
        return m_droppedEnrichmentJobs.load(std::memory_order_relaxed);
    }

    std::size_t EnrichmentQueue::in_flight() const noexcept
    {
        std::scoped_lock lock {m_mutex};
        return static_cast<std::size_t>(m_inFlight);
    }

    std::size_t EnrichmentQueue::pending_size() const noexcept
    {
        std::scoped_lock lock {m_mutex};
        return m_nodesByPc.size();
    }
}
