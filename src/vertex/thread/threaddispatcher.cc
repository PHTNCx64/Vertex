//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/thread/threaddispatcher.hh>

#include <sdk/feature.h>

#include <wx/app.h>
#include <wx/thread.h>

namespace Vertex::Thread
{
    namespace
    {
        std::expected<std::future<StatusCode>, StatusCode>
        dispatch_ui(std::packaged_task<StatusCode()>&& task, bool forceAsync)
        {
            auto* app = wxTheApp;
            if (!app)
            {
                return std::unexpected(StatusCode::STATUS_ERROR_INVALID_STATE);
            }

            if (!forceAsync && wxIsMainThread())
            {
                std::packaged_task<StatusCode()> local{std::move(task)};
                auto future = local.get_future();
                local();
                return future;
            }

            auto shared = std::make_shared<std::packaged_task<StatusCode()>>(std::move(task));
            auto future = shared->get_future();
            app->CallAfter([shared] { (*shared)(); });
            return future;
        }
    }

    ThreadDispatcher::ThreadDispatcher()
    {
        m_priorityThread = std::make_unique<VertexPriorityThread>();
        create_dedicated_threads();
    }

    ThreadDispatcher::~ThreadDispatcher()
    {
        m_workerPools.clear();
        m_workerPoolLogicalSizes.clear();
        destroy_dedicated_threads();
        destroy_shared_thread();
        m_priorityThread.reset();
    }

    StatusCode ThreadDispatcher::configure(const std::uint64_t featureFlags)
    {
        std::scoped_lock lock{m_mutex};

        const auto newEpoch = m_epoch.fetch_add(1, std::memory_order_relaxed) + 1;

        if (m_priorityThread)
        {
            m_priorityThread->invalidate_epoch(newEpoch);
        }

        m_debuggerIndependent = !(featureFlags & VERTEX_FEATURE_DEBUGGER_DEPENDENT);

        if (featureFlags & VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED)
        {
            m_mode = DispatchMode::SingleThreaded;

            destroy_dedicated_threads();

            if (m_debuggerIndependent)
            {
                create_shared_thread();
            }
            else
            {
                destroy_shared_thread();
            }
        }
        else
        {
            m_mode = DispatchMode::MultiThreaded;

            destroy_shared_thread();
            create_dedicated_threads();
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ThreadDispatcher::start()
    {
        std::scoped_lock lock{m_mutex};

        if (m_mode == DispatchMode::SingleThreaded)
        {
            if (m_debuggerIndependent && !m_sharedThread)
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

        const auto newEpoch = m_epoch.fetch_add(1, std::memory_order_relaxed) + 1;

        if (m_priorityThread)
        {
            m_priorityThread->invalidate_epoch(newEpoch);
        }

        m_workerPools.clear();
        m_workerPoolLogicalSizes.clear();
        destroy_dedicated_threads();
        destroy_shared_thread();

        return StatusCode::STATUS_OK;
    }

    std::expected<std::future<StatusCode>, StatusCode>
    ThreadDispatcher::dispatch(const ThreadChannel channel, std::packaged_task<StatusCode()>&& task)
    {
        if (channel == ThreadChannel::UI)
        {
            return dispatch_ui(std::move(task), false);
        }

        std::scoped_lock lock{m_mutex};
        return dispatch_locked(channel, std::move(task));
    }

    StatusCode
    ThreadDispatcher::dispatch_fire_and_forget(const ThreadChannel channel, std::packaged_task<StatusCode()>&& task)
    {
        if (channel == ThreadChannel::UI)
        {
            auto result = dispatch_ui(std::move(task), true);
            if (!result.has_value())
            {
                return result.error();
            }
            return StatusCode::STATUS_OK;
        }

        auto result = dispatch(channel, std::move(task));
        if (!result.has_value())
        {
            return result.error();
        }
        return StatusCode::STATUS_OK;
    }

    std::expected<RecurringTaskHandle, StatusCode>
    ThreadDispatcher::schedule_recurring(const ThreadChannel channel,
                                         const DispatchPriority priority,
                                         const RecurringPolicy policy,
                                         const std::chrono::milliseconds delay,
                                         std::function<StatusCode()> task,
                                         const RecurringFailurePolicy failurePolicy)
    {
        if (channel == ThreadChannel::UI)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_INVALID_PARAMETER);
        }

        std::scoped_lock lock{m_mutex};

        if (!m_priorityThread)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        const auto epoch = m_epoch.load(std::memory_order_relaxed);
        return m_priorityThread->add_recurring(
            priority, policy, delay, std::move(task), failurePolicy, epoch);
    }

    std::expected<RecurringTaskHandle, StatusCode>
    ThreadDispatcher::schedule_recurring_persistent(const ThreadChannel channel,
                                                     const DispatchPriority priority,
                                                     const RecurringPolicy policy,
                                                     const std::chrono::milliseconds delay,
                                                     std::function<StatusCode()> task,
                                                     const RecurringFailurePolicy failurePolicy)
    {
        if (channel == ThreadChannel::UI)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_INVALID_PARAMETER);
        }

        std::scoped_lock lock{m_mutex};

        if (!m_priorityThread)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        return m_priorityThread->add_recurring(
            priority, policy, delay, std::move(task), failurePolicy, PERSISTENT_RECURRING_EPOCH);
    }

    StatusCode ThreadDispatcher::cancel_recurring(const RecurringTaskHandle handle)
    {
        std::scoped_lock lock{m_mutex};

        if (m_priorityThread)
        {
            return m_priorityThread->remove_recurring(handle);
        }

        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    std::expected<std::future<StatusCode>, StatusCode>
    ThreadDispatcher::dispatch_with_priority(const ThreadChannel channel,
                                              const DispatchPriority priority,
                                              std::packaged_task<StatusCode()>&& task)
    {
        if (channel == ThreadChannel::UI)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_INVALID_PARAMETER);
        }

        std::scoped_lock lock{m_mutex};

        if (channel == ThreadChannel::Debugger && m_priorityThread)
        {
            return m_priorityThread->enqueue(priority, std::move(task));
        }

        return dispatch_locked(channel, std::move(task));
    }

    bool ThreadDispatcher::is_single_threaded() const noexcept
    {
        return m_mode == DispatchMode::SingleThreaded;
    }

    bool ThreadDispatcher::is_channel_busy(const ThreadChannel channel) const
    {
        if (channel == ThreadChannel::UI)
        {
            return false;
        }

        std::scoped_lock lock{m_mutex};

        if (channel == ThreadChannel::Debugger && m_priorityThread)
        {
            return m_priorityThread->is_busy();
        }

        if (is_dependent_mode())
        {
            if (m_priorityThread)
            {
                return m_priorityThread->is_busy();
            }
            return false;
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
        if (channel == ThreadChannel::UI)
        {
            return 0;
        }

        std::scoped_lock lock{m_mutex};

        if (channel == ThreadChannel::Debugger && m_priorityThread)
        {
            return m_priorityThread->pending_tasks();
        }

        if (is_dependent_mode())
        {
            if (m_priorityThread)
            {
                return m_priorityThread->pending_tasks();
            }
            return 0;
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

    std::expected<std::future<StatusCode>, StatusCode>
    ThreadDispatcher::dispatch_locked(const ThreadChannel channel, std::packaged_task<StatusCode()>&& task)
    {
        if (channel == ThreadChannel::Debugger && m_priorityThread)
        {
            return m_priorityThread->enqueue(DispatchPriority::Normal, std::move(task));
        }

        if (is_dependent_mode())
        {
            if (m_priorityThread)
            {
                return m_priorityThread->enqueue(DispatchPriority::Normal, std::move(task));
            }
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        if (m_mode == DispatchMode::SingleThreaded)
        {
            return dispatch_to_mpsc(std::move(task));
        }

        return dispatch_to_spsc(channel, std::move(task));
    }

    bool ThreadDispatcher::is_dependent_mode() const noexcept
    {
        return m_mode == DispatchMode::SingleThreaded && !m_debuggerIndependent;
    }

    void ThreadDispatcher::create_dedicated_threads()
    {
        if (m_dedicatedThreads.empty())
        {
            m_dedicatedThreads[ThreadChannel::Freeze] = std::make_unique<VertexSPSCThread>();
            m_dedicatedThreads[ThreadChannel::ProcessList] = std::make_unique<VertexSPSCThread>();
            m_dedicatedThreads[ThreadChannel::Scanner] = std::make_unique<VertexSPSCThread>();
            m_dedicatedThreads[ThreadChannel::Script] = std::make_unique<VertexSPSCThread>();
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
        auto innerFuture = task.get_future().share();

        std::packaged_task<StatusCode()> wrapper(
            [inner = std::move(task), innerFuture]() mutable -> StatusCode
            {
                inner();
                return innerFuture.get();
            });

        auto callerFuture = wrapper.get_future();

        const auto it = m_dedicatedThreads.find(channel);
        if (it == m_dedicatedThreads.end() || !it->second)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
        }

        const StatusCode status = it->second->enqueue_task(std::move(wrapper));
        if (status != StatusCode::STATUS_OK)
        {
            return std::unexpected(status);
        }
        return callerFuture;
    }

    StatusCode ThreadDispatcher::create_worker_pool(const ThreadChannel channel, const std::size_t workerCount)
    {
        if (channel == ThreadChannel::UI)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

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
        if (channel == ThreadChannel::UI)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::scoped_lock lock{m_mutex};

        if (m_mode == DispatchMode::SingleThreaded)
        {
            if (is_dependent_mode() && m_priorityThread)
            {
                return m_priorityThread->enqueue_fire_and_forget(
                    DispatchPriority::Normal, std::move(task));
            }

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
        if (!preferred || !preferred->is_running())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        const StatusCode status = preferred->enqueue_task(std::move(task));
        if (status == StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_OK;
        }

        if (status == StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING)
        {
            const StatusCode restartStatus = preferred->start();
            if (restartStatus != StatusCode::STATUS_OK)
            {
                return restartStatus;
            }
            return preferred->enqueue_task(std::move(task));
        }

        return status;
    }
}
