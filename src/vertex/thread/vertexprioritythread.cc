//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/thread/vertexprioritythread.hh>

#include <algorithm>

namespace Vertex::Thread
{
    VertexPriorityThread::VertexPriorityThread()
    {
        std::ignore = start();
    }

    VertexPriorityThread::~VertexPriorityThread()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            std::ignore = stop();
        }
    }

    StatusCode VertexPriorityThread::start()
    {
        bool expected {};
        if (!m_isRunning.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        try
        {
            m_thread = std::jthread(
                [this](const std::stop_token& token)
                {
                    std::ignore = worker_loop(token);
                });
        }
        catch (...)
        {
            m_isRunning.store(false, std::memory_order_release);
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode VertexPriorityThread::stop()
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        m_isRunning.store(false, std::memory_order_release);
        m_thread.request_stop();
        m_semaphore.release();

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        {
            std::scoped_lock lock {m_recurringMutex};
            for (auto& entry : m_recurringTasks)
            {
                entry->cancelled.store(true, std::memory_order_release);
            }
            m_recurringTasks.clear();
        }

        std::packaged_task<StatusCode()> task {};
        while (m_highQueue.try_dequeue(m_highToken, task))
        {
            m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);
        }
        while (m_normalQueue.try_dequeue(m_normalToken, task))
        {
            m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);
        }
        while (m_lowQueue.try_dequeue(m_lowToken, task))
        {
            m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);
        }

        return StatusCode::STATUS_OK;
    }

    std::expected<std::future<StatusCode>, StatusCode>
    VertexPriorityThread::enqueue(const DispatchPriority priority, std::packaged_task<StatusCode()>&& task)
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        if (priority == DispatchPriority::Low &&
            m_lowQueue.size_approx() >= LOW_QUEUE_HARD_CAP)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_BUSY);
        }

        auto future = task.get_future();

        if (queue_for(priority).enqueue(std::move(task)))
        {
            m_pendingTasks.fetch_add(1, std::memory_order_release);
            m_semaphore.release();
            return future;
        }

        return std::unexpected(StatusCode::STATUS_ERROR_THREAD_INVALID_TASK);
    }

    StatusCode
    VertexPriorityThread::enqueue_fire_and_forget(const DispatchPriority priority,
                                                   std::packaged_task<StatusCode()>&& task)
    {
        auto result = enqueue(priority, std::move(task));
        if (!result.has_value())
        {
            return result.error();
        }
        return StatusCode::STATUS_OK;
    }

    std::expected<RecurringTaskHandle, StatusCode>
    VertexPriorityThread::add_recurring(const DispatchPriority priority,
                                         const RecurringPolicy policy,
                                         const std::chrono::milliseconds delay,
                                         std::function<StatusCode()> task,
                                         const RecurringFailurePolicy failurePolicy,
                                         const std::uint64_t epoch)
    {
        if (!task)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_INVALID_PARAMETER);
        }

        if (policy == RecurringPolicy::AsSoonAsPossible && delay != std::chrono::milliseconds::zero())
        {
            return std::unexpected(StatusCode::STATUS_ERROR_INVALID_PARAMETER);
        }

        const auto id = m_nextId.fetch_add(1, std::memory_order_relaxed);

        auto entry = std::make_shared<RecurringEntry>();
        entry->id = id;
        entry->epoch = epoch;
        entry->priority = priority;
        entry->policy = policy;
        entry->failurePolicy = failurePolicy;
        entry->baseDelay = delay;
        entry->currentDelay = delay;
        entry->callable = std::move(task);
        entry->nextRun = std::chrono::steady_clock::now();

        {
            std::scoped_lock lock {m_recurringMutex};
            m_recurringTasks.push_back(std::move(entry));
        }

        m_semaphore.release();

        return RecurringTaskHandle {.id = id, .epoch = epoch};
    }

    StatusCode VertexPriorityThread::remove_recurring(const RecurringTaskHandle handle)
    {
        std::shared_ptr<RecurringEntry> entry {};

        {
            std::scoped_lock lock {m_recurringMutex};
            auto it = std::ranges::find_if(m_recurringTasks,
                [&](const auto& e)
                {
                    return e->id == handle.id && e->epoch == handle.epoch;
                });

            if (it == m_recurringTasks.end())
            {
                if (handle.epoch < m_currentEpoch.load(std::memory_order_acquire))
                {
                    return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
                }
                return StatusCode::STATUS_OK;
            }

            entry = *it;
            entry->cancelled.store(true, std::memory_order_release);
        }

        if (m_thread.get_id() != std::this_thread::get_id())
        {
            while (entry->running.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }

        return StatusCode::STATUS_OK;
    }

    void VertexPriorityThread::invalidate_epoch(const std::uint64_t newEpoch)
    {
        m_currentEpoch.store(newEpoch, std::memory_order_release);

        std::scoped_lock lock {m_recurringMutex};
        std::erase_if(m_recurringTasks,
            [newEpoch](const auto& e)
            {
                if (e->epoch < newEpoch)
                {
                    e->cancelled.store(true, std::memory_order_release);
                    return true;
                }
                return false;
            });
    }

    bool VertexPriorityThread::is_running() const noexcept
    {
        return m_isRunning.load(std::memory_order_acquire);
    }

    std::size_t VertexPriorityThread::pending_tasks() const noexcept
    {
        return m_pendingTasks.load(std::memory_order_relaxed);
    }

    bool VertexPriorityThread::is_busy() const noexcept
    {
        return m_isBusy.load(std::memory_order_relaxed) ||
               m_pendingTasks.load(std::memory_order_relaxed) > 0;
    }

    StatusCode VertexPriorityThread::worker_loop(const std::stop_token& token)
    {
        while (!token.stop_requested())
        {
            m_isBusy.store(true, std::memory_order_relaxed);

            process_recurring();

            const auto executed = execute_one_shot_tasks(ONE_SHOT_BUDGET_PER_CYCLE);

            m_isBusy.store(false, std::memory_order_relaxed);

            if (executed == 0)
            {
                const auto waitTime = time_until_next_recurring();
                if (waitTime > std::chrono::milliseconds::zero())
                {
                    std::ignore = m_semaphore.try_acquire_for(waitTime);
                }
            }
        }

        return StatusCode::STATUS_OK;
    }

    std::uint32_t VertexPriorityThread::execute_one_shot_tasks(const std::uint32_t budget)
    {
        std::uint32_t executed {};

        for (std::uint32_t i {}; i < budget; ++i)
        {
            std::packaged_task<StatusCode()> task {};
            bool found {};

            if (m_consecutiveHighCount >= PRIORITY_HIGH_BURST_BEFORE_LOW)
            {
                if (m_normalQueue.try_dequeue(m_normalToken, task))
                {
                    found = true;
                    m_consecutiveHighCount = 0;
                }
                else if (m_lowQueue.try_dequeue(m_lowToken, task))
                {
                    found = true;
                    m_consecutiveHighCount = 0;
                }
                else
                {
                    m_consecutiveHighCount = 0;
                    if (m_highQueue.try_dequeue(m_highToken, task))
                    {
                        found = true;
                        ++m_consecutiveHighCount;
                    }
                }
            }
            else
            {
                if (m_highQueue.try_dequeue(m_highToken, task))
                {
                    found = true;
                    ++m_consecutiveHighCount;
                }
                else if (m_normalQueue.try_dequeue(m_normalToken, task))
                {
                    found = true;
                    m_consecutiveHighCount = 0;
                }
                else if (m_lowQueue.try_dequeue(m_lowToken, task))
                {
                    found = true;
                    m_consecutiveHighCount = 0;
                }
            }

            if (!found)
            {
                break;
            }

            m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);

            if (task.valid()) [[likely]]
            {
                try
                {
                    task();
                }
                catch (...)
                {
                }
            }

            ++executed;
        }

        return executed;
    }

    void VertexPriorityThread::process_recurring()
    {
        const auto now = std::chrono::steady_clock::now();

        std::vector<std::shared_ptr<RecurringEntry>> dueEntries {};

        {
            std::scoped_lock lock {m_recurringMutex};

            std::erase_if(m_recurringTasks,
                [](const auto& e)
                {
                    return e->cancelled.load(std::memory_order_acquire);
                });

            for (auto& entry : m_recurringTasks)
            {
                if (!entry->cancelled.load(std::memory_order_acquire) &&
                    !entry->running.load(std::memory_order_acquire) &&
                    entry->nextRun <= now)
                {
                    dueEntries.push_back(entry);
                }
            }
        }

        for (auto& entry : dueEntries)
        {
            if (entry->cancelled.load(std::memory_order_acquire))
            {
                continue;
            }

            entry->running.store(true, std::memory_order_release);

            StatusCode result {StatusCode::STATUS_OK};
            try
            {
                result = entry->callable();
            }
            catch (...)
            {
                result = StatusCode::STATUS_ERROR_GENERAL;
            }

            entry->running.store(false, std::memory_order_release);

            if (entry->cancelled.load(std::memory_order_acquire))
            {
                continue;
            }

            const auto completionTime = std::chrono::steady_clock::now();
            const bool failed = result != StatusCode::STATUS_OK;

            if (failed)
            {
                switch (entry->failurePolicy)
                {
                    case RecurringFailurePolicy::CancelOnFailure:
                    {
                        entry->cancelled.store(true, std::memory_order_release);
                        continue;
                    }
                    case RecurringFailurePolicy::BackoffOnFailure:
                    {
                        ++entry->consecutiveFailures;
                        entry->currentDelay = std::min(
                            std::chrono::milliseconds {
                                entry->baseDelay.count() *
                                (static_cast<std::int64_t>(1) << std::min(entry->consecutiveFailures, 15u))
                            },
                            MAX_BACKOFF_DELAY);
                        break;
                    }
                    case RecurringFailurePolicy::Continue:
                    {
                        break;
                    }
                }
            }
            else
            {
                entry->consecutiveFailures = 0;
                entry->currentDelay = entry->baseDelay;
            }

            switch (entry->policy)
            {
                case RecurringPolicy::AsSoonAsPossible:
                {
                    entry->nextRun = completionTime;
                    break;
                }
                case RecurringPolicy::FixedDelay:
                {
                    entry->nextRun = completionTime + entry->currentDelay;
                    break;
                }
            }
        }
    }

    std::chrono::milliseconds VertexPriorityThread::time_until_next_recurring() const
    {
        std::scoped_lock lock {m_recurringMutex};

        if (m_recurringTasks.empty())
        {
            return IDLE_POLL_INTERVAL;
        }

        const auto now = std::chrono::steady_clock::now();
        auto shortest = MAX_IDLE_WAIT;

        for (const auto& entry : m_recurringTasks)
        {
            if (entry->cancelled.load(std::memory_order_acquire) ||
                entry->running.load(std::memory_order_acquire))
            {
                continue;
            }

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                entry->nextRun - now);

            if (remaining <= std::chrono::milliseconds::zero())
            {
                return std::chrono::milliseconds::zero();
            }

            shortest = std::min(shortest, remaining);
        }

        return shortest;
    }

    moodycamel::ConcurrentQueue<std::packaged_task<StatusCode()>>&
    VertexPriorityThread::queue_for(const DispatchPriority priority)
    {
        switch (priority)
        {
            case DispatchPriority::High:
                return m_highQueue;
            case DispatchPriority::Normal:
                return m_normalQueue;
            case DispatchPriority::Low:
                return m_lowQueue;
        }
        std::unreachable();
    }
}
