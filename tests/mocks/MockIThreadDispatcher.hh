#pragma once

#include <gmock/gmock.h>
#include <vertex/thread/ithreaddispatcher.hh>

namespace Vertex::Testing::Mocks
{
    class MockIThreadDispatcher : public Thread::IThreadDispatcher
    {
    public:
        ~MockIThreadDispatcher() override = default;

        MOCK_METHOD((std::expected<std::future<StatusCode>, StatusCode>),
                    dispatch, (Thread::ThreadChannel channel, std::packaged_task<StatusCode()>&& task), (override));
        MOCK_METHOD(StatusCode,
                    dispatch_fire_and_forget, (Thread::ThreadChannel channel, std::packaged_task<StatusCode()>&& task), (override));
        MOCK_METHOD(StatusCode, configure, (std::uint64_t featureFlags), (override));
        MOCK_METHOD(StatusCode, start, (), (override));
        MOCK_METHOD(StatusCode, stop, (), (override));
        MOCK_METHOD(bool, is_single_threaded, (), (const, noexcept, override));
        MOCK_METHOD(bool, is_channel_busy, (Thread::ThreadChannel channel), (const, override));
        MOCK_METHOD(std::size_t, pending_tasks, (Thread::ThreadChannel channel), (const, override));

        MOCK_METHOD(StatusCode, create_worker_pool, (Thread::ThreadChannel channel, std::size_t workerCount), (override));
        MOCK_METHOD(StatusCode, destroy_worker_pool, (Thread::ThreadChannel channel), (override));
        MOCK_METHOD(StatusCode, enqueue_on_worker,
                    (Thread::ThreadChannel channel, std::size_t workerIndex, std::packaged_task<StatusCode()>&& task), (override));
    };
}
