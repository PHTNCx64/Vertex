//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/thread/threadchannel.hh>

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <future>

namespace Vertex::Thread
{
    enum class DispatchPriority : std::uint8_t
    {
        High,
        Normal,
        Low
    };

    static constexpr std::uint32_t PRIORITY_HIGH_BURST_BEFORE_LOW {4};

    struct RecurringTaskHandle final
    {
        std::uint64_t id {};
        std::uint64_t epoch {};

        [[nodiscard]] bool operator==(const RecurringTaskHandle&) const noexcept = default;
    };

    enum class RecurringPolicy : std::uint8_t
    {
        FixedDelay,
        AsSoonAsPossible
    };

    enum class RecurringFailurePolicy : std::uint8_t
    {
        Continue,
        CancelOnFailure,
        BackoffOnFailure
    };

    class IThreadDispatcher
    {
    public:
        virtual ~IThreadDispatcher() = default;

        [[nodiscard]] virtual std::expected<std::future<StatusCode>, StatusCode>
        dispatch(ThreadChannel channel, std::packaged_task<StatusCode()>&& task) = 0;

        [[nodiscard]] virtual StatusCode
        dispatch_fire_and_forget(ThreadChannel channel, std::packaged_task<StatusCode()>&& task) = 0;

        [[nodiscard]] virtual std::expected<RecurringTaskHandle, StatusCode>
        schedule_recurring(ThreadChannel channel,
                           DispatchPriority priority,
                           RecurringPolicy policy,
                           std::chrono::milliseconds delay,
                           std::function<StatusCode()> task,
                           RecurringFailurePolicy failurePolicy = RecurringFailurePolicy::Continue) = 0;

        [[nodiscard]] virtual StatusCode cancel_recurring(RecurringTaskHandle handle) = 0;

        [[nodiscard]] virtual std::expected<std::future<StatusCode>, StatusCode>
        dispatch_with_priority(ThreadChannel channel,
                               DispatchPriority priority,
                               std::packaged_task<StatusCode()>&& task) = 0;

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
