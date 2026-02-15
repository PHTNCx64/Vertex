//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/thread/vertexmpmcthread.hh>

#include <chrono>

namespace Vertex::Thread
{
    VertexMPMCThread::VertexMPMCThread()
    {
        // TODO: Graceful handling
        std::ignore = start();
    }

    VertexMPMCThread::~VertexMPMCThread()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            // TODO: Graceful handling
            std::ignore = stop();
        }
    }

    StatusCode VertexMPMCThread::start()
    {
        bool expected{};
        if (!m_isRunning.compare_exchange_strong(expected, true,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        try
        {
            m_vertexThread = std::jthread(
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

    StatusCode VertexMPMCThread::stop()
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        m_vertexThread.request_stop();
        m_isRunning.store(false, std::memory_order_release);

        return StatusCode::STATUS_OK;
    }

    StatusCode VertexMPMCThread::enqueue_task(std::packaged_task<StatusCode()>&& task)
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        if (m_taskQueue.enqueue(std::move(task)))
        {
            m_pendingTasks.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        }
        return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
    }

    StatusCode VertexMPMCThread::is_busy() const
    {
        if (m_isBusy.load(std::memory_order_relaxed))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        if (m_pendingTasks.load(std::memory_order_relaxed) > 0)
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode VertexMPMCThread::get_last_status()
    {
        std::future<StatusCode> future{};

        {
            std::scoped_lock lock{m_futureMutex};
            if (!m_lastFuture.valid())
            {
                return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
            }
            future = std::move(m_lastFuture);
        }

        try
        {
            return future.get();
        }
        catch (...)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }
    }

    bool VertexMPMCThread::is_running() const noexcept
    {
        return m_isRunning.load(std::memory_order_acquire);
    }

    std::size_t VertexMPMCThread::get_pending_tasks() const noexcept
    {
        return m_pendingTasks.load(std::memory_order_relaxed);
    }

    StatusCode VertexMPMCThread::worker_loop(const std::stop_token& token)
    {
        static constexpr auto DEQUEUE_TIMEOUT{std::chrono::milliseconds{1}};

        while (!token.stop_requested())
        {
            std::packaged_task<StatusCode()> task{};

            if (!m_taskQueue.wait_dequeue_timed(task, DEQUEUE_TIMEOUT))
            {
                continue;
            }

            if (!task.valid()) [[unlikely]]
            {
                continue;
            }

            m_isBusy.store(true, std::memory_order_relaxed);
            m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);

            try
            {
                std::future<StatusCode> future{task.get_future()};

                task();
                {
                    std::scoped_lock lock{m_futureMutex};
                    m_lastFuture = std::move(future);
                }
            }
            catch (...)
            {
                m_isBusy.store(false, std::memory_order_relaxed);
                continue;
            }

            m_isBusy.store(false, std::memory_order_relaxed);
        }

        return StatusCode::STATUS_OK;
    }
}
