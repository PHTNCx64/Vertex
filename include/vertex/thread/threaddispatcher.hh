//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/thread/vertexspscthread.hh>
#include <vertex/thread/vertexmpscthread.hh>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace Vertex::Thread
{
    enum class DispatchMode : std::uint8_t
    {
        MultiThreaded,
        SingleThreaded
    };

    class ThreadDispatcher final : public IThreadDispatcher
    {
    public:
        ThreadDispatcher();
        ~ThreadDispatcher() override;

        ThreadDispatcher(const ThreadDispatcher&) = delete;
        ThreadDispatcher& operator=(const ThreadDispatcher&) = delete;
        ThreadDispatcher(ThreadDispatcher&&) = delete;
        ThreadDispatcher& operator=(ThreadDispatcher&&) = delete;

        [[nodiscard]] std::expected<std::future<StatusCode>, StatusCode>
        dispatch(ThreadChannel channel, std::packaged_task<StatusCode()>&& task) override;

        [[nodiscard]] StatusCode
        dispatch_fire_and_forget(ThreadChannel channel, std::packaged_task<StatusCode()>&& task) override;

        [[nodiscard]] StatusCode configure(std::uint64_t featureFlags) override;
        [[nodiscard]] StatusCode start() override;
        [[nodiscard]] StatusCode stop() override;

        [[nodiscard]] bool is_single_threaded() const noexcept override;
        [[nodiscard]] bool is_channel_busy(ThreadChannel channel) const override;
        [[nodiscard]] std::size_t pending_tasks(ThreadChannel channel) const override;

        [[nodiscard]] StatusCode create_worker_pool(ThreadChannel channel, std::size_t workerCount) override;
        [[nodiscard]] StatusCode destroy_worker_pool(ThreadChannel channel) override;
        [[nodiscard]] StatusCode enqueue_on_worker(ThreadChannel channel, std::size_t workerIndex,
                                                    std::packaged_task<StatusCode()>&& task) override;

    private:
        void create_dedicated_threads();
        void destroy_dedicated_threads();
        void create_shared_thread();
        void destroy_shared_thread();

        [[nodiscard]] std::expected<std::future<StatusCode>, StatusCode>
        dispatch_to_mpsc(std::packaged_task<StatusCode()>&& task) const;

        [[nodiscard]] std::expected<std::future<StatusCode>, StatusCode>
        dispatch_to_spsc(ThreadChannel channel, std::packaged_task<StatusCode()>&& task);

        DispatchMode m_mode {DispatchMode::MultiThreaded};
        bool m_debuggerIndependent {true};

        std::unique_ptr<VertexMPSCThread> m_sharedThread {};
        std::unordered_map<ThreadChannel, std::unique_ptr<VertexSPSCThread>> m_dedicatedThreads {};
        std::unique_ptr<VertexSPSCThread> m_dedicatedDebuggerThread {};

        std::unordered_map<ThreadChannel, std::vector<std::unique_ptr<VertexSPSCThread>>> m_workerPools {};
        std::unordered_map<ThreadChannel, std::size_t> m_workerPoolLogicalSizes {};

        mutable std::mutex m_mutex {};
    };
}
