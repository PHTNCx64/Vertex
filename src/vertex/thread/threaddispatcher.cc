//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/thread/threaddispatcher.hh>

#include <sdk/feature.hh>

namespace Vertex::Thread
{
    ThreadDispatcher::ThreadDispatcher()
    {
        create_dedicated_threads();
    }

    ThreadDispatcher::~ThreadDispatcher()
    {
        m_workerPools.clear();
        m_workerPoolLogicalSizes.clear();
        destroy_dedicated_threads();
        destroy_shared_thread();
    }

    StatusCode ThreadDispatcher::configure(const std::uint64_t featureFlags)
    {
        std::scoped_lock lock{m_mutex};

        m_debuggerIndependent = !(featureFlags & VERTEX_FEATURE_DEBUGGER_DEPENDENT);

        if (featureFlags & VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED)
        {
            m_mode = DispatchMode::SingleThreaded;

            destroy_dedicated_threads();
            create_shared_thread();

            if (m_debuggerIndependent && !m_dedicatedDebuggerThread)
            {
                m_dedicatedDebuggerThread = std::make_unique<VertexSPSCThread>();
            }
        }
        else
        {
            m_mode = DispatchMode::MultiThreaded;

            destroy_shared_thread();
            m_dedicatedDebuggerThread.reset();
            create_dedicated_threads();
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ThreadDispatcher::start()
    {
        std::scoped_lock lock{m_mutex};

        if (m_mode == DispatchMode::SingleThreaded)
        {
            if (!m_sharedThread)
            {
                create_shared_thread();
            }
        }
        else
        {
            if (m_dedicatedThreads.empty())
            {
                create_dedicated_threads();
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ThreadDispatcher::stop()
    {
        std::scoped_lock lock{m_mutex};

        m_workerPools.clear();
        m_workerPoolLogicalSizes.clear();
        destroy_dedicated_threads();
        destroy_shared_thread();
        m_dedicatedDebuggerThread.reset();

        return StatusCode::STATUS_OK;
    }

    std::expected<std::future<StatusCode>, StatusCode>
    ThreadDispatcher::dispatch(const ThreadChannel channel, std::packaged_task<StatusCode()>&& task)
    {
        std::scoped_lock lock{m_mutex};

        if (channel == ThreadChannel::Debugger && m_debuggerIndependent)
        {
            if (m_dedicatedDebuggerThread)
            {
                return dispatch_to_spsc(channel, std::move(task));
            }
        }

        if (m_mode == DispatchMode::SingleThreaded)
        {
            return dispatch_to_mpsc(std::move(task));
        }

        return dispatch_to_spsc(channel, std::move(task));
    }

    StatusCode
    ThreadDispatcher::dispatch_fire_and_forget(ThreadChannel channel, std::packaged_task<StatusCode()>&& task)
    {
        auto result = dispatch(channel, std::move(task));
        if (!result.has_value())
        {
            return result.error();
        }
        return StatusCode::STATUS_OK;
    }

    bool ThreadDispatcher::is_single_threaded() const noexcept
    {
        return m_mode == DispatchMode::SingleThreaded;
    }

    bool ThreadDispatcher::is_channel_busy(const ThreadChannel channel) const
    {
        std::scoped_lock lock{m_mutex};

        if (channel == ThreadChannel::Debugger && m_debuggerIndependent && m_dedicatedDebuggerThread)
        {
            return m_dedicatedDebuggerThread->is_busy() == StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        if (m_mode == DispatchMode::SingleThreaded)
        {
            if (m_sharedThread)
            {
                return m_sharedThread->is_busy() == StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
            }
            return false;
        }

        const auto it = m_dedicatedThreads.find(channel);
        if (it != m_dedicatedThreads.end())
        {
            return it->second->is_busy() == StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        return false;
    }

    std::size_t ThreadDispatcher::pending_tasks(const ThreadChannel channel) const
    {
        std::scoped_lock lock{m_mutex};

        if (channel == ThreadChannel::Debugger && m_debuggerIndependent && m_dedicatedDebuggerThread)
        {
            return m_dedicatedDebuggerThread->get_pending_tasks();
        }

        if (m_mode == DispatchMode::SingleThreaded)
        {
            if (m_sharedThread)
            {
                return m_sharedThread->get_pending_tasks();
            }
            return 0;
        }

        const auto it = m_dedicatedThreads.find(channel);
        if (it != m_dedicatedThreads.end())
        {
            return it->second->get_pending_tasks();
        }

        return 0;
    }

    void ThreadDispatcher::create_dedicated_threads()
    {
        if (m_dedicatedThreads.empty())
        {
            m_dedicatedThreads[ThreadChannel::Freeze] = std::make_unique<VertexSPSCThread>();
            m_dedicatedThreads[ThreadChannel::ProcessList] = std::make_unique<VertexSPSCThread>();
            m_dedicatedThreads[ThreadChannel::Debugger] = std::make_unique<VertexSPSCThread>();
            m_dedicatedThreads[ThreadChannel::Scanner] = std::make_unique<VertexSPSCThread>();
        }
    }

    void ThreadDispatcher::destroy_dedicated_threads()
    {
        m_dedicatedThreads.clear();
    }

    void ThreadDispatcher::create_shared_thread()
    {
        if (!m_sharedThread)
        {
            m_sharedThread = std::make_unique<VertexMPSCThread>();
        }
    }

    void ThreadDispatcher::destroy_shared_thread()
    {
        m_sharedThread.reset();
    }

    std::expected<std::future<StatusCode>, StatusCode>
    ThreadDispatcher::dispatch_to_mpsc(std::packaged_task<StatusCode()>&& task) const
    {
        if (!m_sharedThread)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        return m_sharedThread->enqueue_task(std::move(task));
    }

    std::expected<std::future<StatusCode>, StatusCode>
    ThreadDispatcher::dispatch_to_spsc(const ThreadChannel channel, std::packaged_task<StatusCode()>&& task)
    {
        auto resultFuture = task.get_future();

        std::packaged_task<StatusCode()> wrapper(
            [inner = std::move(task)]() mutable -> StatusCode
            {
                inner();
                return StatusCode::STATUS_OK;
            });

        VertexSPSCThread* target{};

        if (channel == ThreadChannel::Debugger && m_debuggerIndependent && m_dedicatedDebuggerThread)
        {
            target = m_dedicatedDebuggerThread.get();
        }
        else
        {
            const auto it = m_dedicatedThreads.find(channel);
            if (it == m_dedicatedThreads.end() || !it->second)
            {
                return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
            }
            target = it->second.get();
        }

        const StatusCode status = target->enqueue_task(std::move(wrapper));
        if (status != StatusCode::STATUS_OK)
        {
            return std::unexpected(status);
        }
        return resultFuture;
    }

    StatusCode ThreadDispatcher::create_worker_pool(const ThreadChannel channel, const std::size_t workerCount)
    {
        std::scoped_lock lock{m_mutex};

        m_workerPools.erase(channel);
        m_workerPoolLogicalSizes[channel] = workerCount;

        if (m_mode == DispatchMode::SingleThreaded)
        {
            return StatusCode::STATUS_OK;
        }

        auto& pool = m_workerPools[channel];
        pool.reserve(workerCount);
        for (std::size_t i = 0; i < workerCount; ++i)
        {
            auto thread = std::make_unique<VertexSPSCThread>();
            if (!thread->is_running())
            {
                pool.clear();
                m_workerPoolLogicalSizes.erase(channel);
                return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
            }
            pool.push_back(std::move(thread));
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ThreadDispatcher::destroy_worker_pool(const ThreadChannel channel)
    {
        std::scoped_lock lock{m_mutex};

        m_workerPools.erase(channel);
        m_workerPoolLogicalSizes.erase(channel);

        return StatusCode::STATUS_OK;
    }

    StatusCode ThreadDispatcher::enqueue_on_worker(const ThreadChannel channel, const std::size_t workerIndex,
                                                    std::packaged_task<StatusCode()>&& task)
    {
        std::scoped_lock lock{m_mutex};

        if (m_mode == DispatchMode::SingleThreaded)
        {
            if (!m_sharedThread)
            {
                return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
            }

            auto result = m_sharedThread->enqueue_task(std::move(task));
            if (!result.has_value())
            {
                return result.error();
            }
            return StatusCode::STATUS_OK;
        }

        const auto poolIt = m_workerPools.find(channel);
        if (poolIt == m_workerPools.end() || poolIt->second.empty())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        auto& pool = poolIt->second;
        if (workerIndex >= pool.size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& preferred = pool[workerIndex];
        if (preferred && preferred->is_running())
        {
            const StatusCode status = preferred->enqueue_task(std::move(task));
            if (status == StatusCode::STATUS_OK)
            {
                return StatusCode::STATUS_OK;
            }

            if (status == StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING)
            {
                const StatusCode restartStatus = preferred->start();
                if (restartStatus == StatusCode::STATUS_OK)
                {
                    const StatusCode retryStatus = preferred->enqueue_task(std::move(task));
                    if (retryStatus == StatusCode::STATUS_OK)
                    {
                        return StatusCode::STATUS_OK;
                    }
                }
            }
        }

        for (std::size_t i = 0; i < pool.size(); ++i)
        {
            if (i == workerIndex)
            {
                continue;
            }
            if (pool[i] && pool[i]->is_running())
            {
                const StatusCode altStatus = pool[i]->enqueue_task(std::move(task));
                if (altStatus == StatusCode::STATUS_OK)
                {
                    return StatusCode::STATUS_OK;
                }
            }
        }

        return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
    }
}
