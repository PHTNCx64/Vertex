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
#include <expected>
#include <atomic>
#include <semaphore>

#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace Vertex::Thread
{
    class VertexMPSCThread final
    {
    public:
        VertexMPSCThread();
        ~VertexMPSCThread();

        [[nodiscard]] std::expected<std::future<StatusCode>, StatusCode> enqueue_task(std::packaged_task<StatusCode()>&& task);

        [[nodiscard]] StatusCode start();
        [[nodiscard]] StatusCode stop();
        [[nodiscard]] StatusCode is_busy() const;
        [[nodiscard]] bool is_running() const noexcept;
        [[nodiscard]] std::size_t get_pending_tasks() const noexcept;

    private:
        [[nodiscard]] StatusCode worker_loop(const std::stop_token& token);
        void drain_queue();

        moodycamel::ConcurrentQueue<std::packaged_task<StatusCode()>> m_taskQueue {};
        moodycamel::ConsumerToken m_consumerToken {m_taskQueue};

        std::jthread m_vertexThread {};

        MSVC_SUPPRESS_PADDING_WARNING

        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_isRunning {};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_pendingTasks {};

        MSVC_END_WARNING_SUPPRESSION

        std::counting_semaphore<> m_semaphore {0};
    };
}
