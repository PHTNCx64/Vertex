//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/thread/vertexmpscthread.hh>

#include <mimalloc.h>

// Used for plugins that specified the single thread mode feature.

namespace Vertex::Thread
{
    VertexMPSCThread::VertexMPSCThread()
    {
        std::ignore = start();
    }

    VertexMPSCThread::~VertexMPSCThread()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            std::ignore = stop();
        }
    }

    StatusCode VertexMPSCThread::start()
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

    StatusCode VertexMPSCThread::stop()
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        m_isRunning.store(false, std::memory_order_release);

        m_vertexThread.request_stop();
        m_semaphore.release();

        if (m_vertexThread.joinable())
        {
            m_vertexThread.join();
        }

        drain_queue();

        return StatusCode::STATUS_OK;
    }

    std::expected<std::future<StatusCode>, StatusCode> VertexMPSCThread::enqueue_task(std::packaged_task<StatusCode()>&& task)
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        auto future(task.get_future());

        if (m_taskQueue.enqueue(std::move(task)))
        {
            m_pendingTasks.fetch_add(1, std::memory_order_release);
            m_semaphore.release();
            return future;
        }
        return std::unexpected(StatusCode::STATUS_ERROR_THREAD_INVALID_TASK);
    }

    StatusCode VertexMPSCThread::is_busy() const
    {
        if (m_pendingTasks.load(std::memory_order_acquire) > 0)
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        return StatusCode::STATUS_OK;
    }

    bool VertexMPSCThread::is_running() const noexcept
    {
        return m_isRunning.load(std::memory_order_acquire);
    }

    std::size_t VertexMPSCThread::get_pending_tasks() const noexcept
    {
        return m_pendingTasks.load(std::memory_order_acquire);
    }

    void VertexMPSCThread::drain_queue()
    {
        std::packaged_task<StatusCode()> task{};
        while (m_taskQueue.try_dequeue(m_consumerToken, task))
        {
            m_pendingTasks.fetch_sub(1, std::memory_order_release);
        }
    }

    StatusCode VertexMPSCThread::worker_loop(const std::stop_token& token)
    {
        while (!token.stop_requested())
        {
            m_semaphore.acquire();

            if (token.stop_requested()) [[unlikely]]
            {
                break;
            }

            std::packaged_task<StatusCode()> task{};
            if (!m_taskQueue.try_dequeue(m_consumerToken, task))
            {
                continue;
            }

            if (!task.valid()) [[unlikely]]
            {
                continue;
            }

            task();

            m_pendingTasks.fetch_sub(1, std::memory_order_release);
        }

        mi_collect(true);
        return StatusCode::STATUS_OK;
    }
}
