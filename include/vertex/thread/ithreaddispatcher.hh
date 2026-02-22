//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/thread/threadchannel.hh>

#include <cstdint>
#include <expected>
#include <future>

namespace Vertex::Thread
{
    class IThreadDispatcher
    {
    public:
        virtual ~IThreadDispatcher() = default;

        [[nodiscard]] virtual std::expected<std::future<StatusCode>, StatusCode>
        dispatch(ThreadChannel channel, std::packaged_task<StatusCode()>&& task) = 0;

        [[nodiscard]] virtual StatusCode
        dispatch_fire_and_forget(ThreadChannel channel, std::packaged_task<StatusCode()>&& task) = 0;

        [[nodiscard]] virtual StatusCode configure(std::uint64_t featureFlags) = 0;
        [[nodiscard]] virtual StatusCode start() = 0;
        [[nodiscard]] virtual StatusCode stop() = 0;

        [[nodiscard]] virtual bool is_single_threaded() const noexcept = 0;
        [[nodiscard]] virtual bool is_channel_busy(ThreadChannel channel) const = 0;
        [[nodiscard]] virtual std::size_t pending_tasks(ThreadChannel channel) const = 0;

        [[nodiscard]] virtual StatusCode create_worker_pool(ThreadChannel channel, std::size_t workerCount) = 0;
        [[nodiscard]] virtual StatusCode destroy_worker_pool(ThreadChannel channel) = 0;
        [[nodiscard]] virtual StatusCode enqueue_on_worker(ThreadChannel channel, std::size_t workerIndex,
                                                           std::packaged_task<StatusCode()>&& task) = 0;
    };
}
