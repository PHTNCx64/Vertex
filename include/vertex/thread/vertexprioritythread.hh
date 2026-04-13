//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/macrohelp.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <atomic>
#include <chrono>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <semaphore>
#include <thread>
#include <vector>

#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace Vertex::Thread
{
    class VertexPriorityThread final
    {
    public:
        VertexPriorityThread();
        ~VertexPriorityThread();

        VertexPriorityThread(const VertexPriorityThread&) = delete;
        VertexPriorityThread& operator=(const VertexPriorityThread&) = delete;
        VertexPriorityThread(VertexPriorityThread&&) = delete;
        VertexPriorityThread& operator=(VertexPriorityThread&&) = delete;

        [[nodiscard]] StatusCode start();
        [[nodiscard]] StatusCode stop();

        [[nodiscard]] std::expected<std::future<StatusCode>, StatusCode>
        enqueue(DispatchPriority priority, std::packaged_task<StatusCode()>&& task);

        [[nodiscard]] StatusCode
        enqueue_fire_and_forget(DispatchPriority priority, std::packaged_task<StatusCode()>&& task);

        [[nodiscard]] std::expected<RecurringTaskHandle, StatusCode>
        add_recurring(DispatchPriority priority,
                      RecurringPolicy policy,
                      std::chrono::milliseconds delay,
                      std::function<StatusCode()> task,
                      RecurringFailurePolicy failurePolicy,
                      std::uint64_t epoch);

        [[nodiscard]] StatusCode remove_recurring(RecurringTaskHandle handle);

        void invalidate_epoch(std::uint64_t newEpoch);

        [[nodiscard]] bool is_running() const noexcept;
        [[nodiscard]] std::size_t pending_tasks() const noexcept;
        [[nodiscard]] bool is_busy() const noexcept;

    private:
        struct RecurringEntry final
        {
            std::uint64_t id {};
            std::uint64_t epoch {};
            DispatchPriority priority {};
            RecurringPolicy policy {};
            RecurringFailurePolicy failurePolicy {};
            std::chrono::milliseconds baseDelay {};
            std::chrono::milliseconds currentDelay {};
            std::function<StatusCode()> callable {};
            std::chrono::steady_clock::time_point nextRun {};
            std::uint32_t consecutiveFailures {};
            std::atomic<bool> cancelled {};
            std::atomic<bool> running {};
        };

        [[nodiscard]] StatusCode worker_loop(const std::stop_token& token);
        std::uint32_t execute_one_shot_tasks(std::uint32_t budget);
        void process_recurring();
        [[nodiscard]] std::chrono::milliseconds time_until_next_recurring() const;

        [[nodiscard]] moodycamel::ConcurrentQueue<std::packaged_task<StatusCode()>>&
        queue_for(DispatchPriority priority);

        moodycamel::ConcurrentQueue<std::packaged_task<StatusCode()>> m_highQueue {};
        moodycamel::ConcurrentQueue<std::packaged_task<StatusCode()>> m_normalQueue {};
        moodycamel::ConcurrentQueue<std::packaged_task<StatusCode()>> m_lowQueue {};

        moodycamel::ConsumerToken m_highToken {m_highQueue};
        moodycamel::ConsumerToken m_normalToken {m_normalQueue};
        moodycamel::ConsumerToken m_lowToken {m_lowQueue};

        std::jthread m_thread {};
        std::counting_semaphore<> m_semaphore {0};

        START_PADDING_WARNING_SUPPRESSION

        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_isRunning {};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_pendingTasks {};
        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_isBusy {};

        END_PADDING_WARNING_SUPPRESSION

        mutable std::mutex m_recurringMutex {};
        std::vector<std::shared_ptr<RecurringEntry>> m_recurringTasks {};
        std::atomic<std::uint64_t> m_nextId {1};
        std::atomic<std::uint64_t> m_currentEpoch {};

        std::uint32_t m_consecutiveHighCount {};

        static constexpr std::uint32_t ONE_SHOT_BUDGET_PER_CYCLE {8};
        static constexpr std::uint32_t LOW_QUEUE_HARD_CAP {32};
        static constexpr std::chrono::milliseconds IDLE_POLL_INTERVAL {1};
        static constexpr std::chrono::milliseconds MAX_IDLE_WAIT {500};
        static constexpr std::chrono::milliseconds MAX_BACKOFF_DELAY {30000};
    };
}
