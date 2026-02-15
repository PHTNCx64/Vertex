//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/macrohelp.hh>

#include <sdk/statuscode.h>

#include <thread>
#include <new>
#include <future>
#include <atomic>

#include <concurrentqueue/moodycamel/blockingconcurrentqueue.h>

namespace Vertex::Thread
{
    // We're using a blocking queue right now which makes sense for long living threads that are generally long living.
    // this implementation isn't quite ideal for tasks such as the memory scanner in which high throughput and lower latency is more important, but we stick with it for now.
    // maybe it makes sense to make a separate mpmc thread class that uses a non-blocking queue for those tasks?
    class VertexMPMCThread final
    {
    public:
        VertexMPMCThread();
        ~VertexMPMCThread();

        [[nodiscard]] StatusCode enqueue_task(std::packaged_task<StatusCode()>&& task);

        [[nodiscard]] StatusCode start();
        [[nodiscard]] StatusCode stop();
        [[nodiscard]] StatusCode get_last_status();
        [[nodiscard]] StatusCode is_busy() const;
        [[nodiscard]] bool is_running() const noexcept;
        [[nodiscard]] std::size_t get_pending_tasks() const noexcept;

    private:
        [[nodiscard]] StatusCode worker_loop(const std::stop_token& token);

        moodycamel::BlockingConcurrentQueue<std::packaged_task<StatusCode()>> m_taskQueue {};

        std::jthread m_vertexThread {};

        MSVC_SUPPRESS_PADDING_WARNING

        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_isRunning {};
        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_isBusy {};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_pendingTasks {};

        MSVC_END_WARNING_SUPPRESSION

        mutable std::mutex m_futureMutex {};
        std::future<StatusCode> m_lastFuture {};
    };
}
