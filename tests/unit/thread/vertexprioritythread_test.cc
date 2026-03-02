#include <gtest/gtest.h>
#include <vertex/thread/vertexprioritythread.hh>

#include <atomic>
#include <barrier>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

using namespace Vertex::Thread;
using namespace std::chrono_literals;

class VertexPriorityThreadTest : public ::testing::Test
{
protected:
    std::unique_ptr<VertexPriorityThread> m_thread {};

    void SetUp() override
    {
        m_thread = std::make_unique<VertexPriorityThread>();
    }

    void TearDown() override
    {
        m_thread.reset();
    }

    static std::packaged_task<StatusCode()> make_task(StatusCode result)
    {
        return std::packaged_task<StatusCode()>(
            [result]() -> StatusCode
            {
                return result;
            });
    }

    static std::packaged_task<StatusCode()> make_recording_task(std::atomic<int>& counter, int id,
                                                                  std::vector<int>& order, std::mutex& orderMutex)
    {
        return std::packaged_task<StatusCode()>(
            [&counter, id, &order, &orderMutex]() -> StatusCode
            {
                {
                    std::scoped_lock lock {orderMutex};
                    order.push_back(id);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
    }
};

TEST_F(VertexPriorityThreadTest, EnqueueAndExecute)
{
    auto task = make_task(StatusCode::STATUS_OK);
    auto result = m_thread->enqueue(DispatchPriority::Normal, std::move(task));

    ASSERT_TRUE(result.has_value());

    const auto status = result->get();
    EXPECT_EQ(status, StatusCode::STATUS_OK);
}

TEST_F(VertexPriorityThreadTest, EnqueuePropagatesErrorStatus)
{
    auto task = make_task(StatusCode::STATUS_ERROR_GENERAL);
    auto result = m_thread->enqueue(DispatchPriority::Normal, std::move(task));

    ASSERT_TRUE(result.has_value());

    const auto status = result->get();
    EXPECT_EQ(status, StatusCode::STATUS_ERROR_GENERAL);
}

TEST_F(VertexPriorityThreadTest, HighPriorityRunsBeforeLow)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_thread->enqueue(DispatchPriority::High, std::move(blocker));

    std::atomic<int> counter {};
    std::vector<int> order {};
    std::mutex orderMutex {};

    for (int i = 0; i < 3; ++i)
    {
        std::ignore = m_thread->enqueue(DispatchPriority::Low,
            make_recording_task(counter, 100 + i, order, orderMutex));
    }
    for (int i = 0; i < 3; ++i)
    {
        std::ignore = m_thread->enqueue(DispatchPriority::High,
            make_recording_task(counter, i, order, orderMutex));
    }

    gate.count_down();

    while (counter.load(std::memory_order_relaxed) < 6)
    {
        std::this_thread::sleep_for(1ms);
    }

    std::scoped_lock lock {orderMutex};

    int firstLowIndex {-1};
    int lastHighIndex {-1};
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
    {
        if (order[i] < 100 && i > lastHighIndex)
        {
            lastHighIndex = i;
        }
        if (order[i] >= 100 && (firstLowIndex == -1 || i < firstLowIndex))
        {
            firstLowIndex = i;
        }
    }

    EXPECT_GT(firstLowIndex, 0) << "Low tasks should not run first when High tasks are pending";
}

TEST_F(VertexPriorityThreadTest, AntiStarvationServicesLowAfterHighBurst)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_thread->enqueue(DispatchPriority::High, std::move(blocker));

    std::atomic<int> lowCount {};
    std::atomic<int> highCount {};

    for (int i = 0; i < 20; ++i)
    {
        std::packaged_task<StatusCode()> highTask(
            [&highCount]() -> StatusCode
            {
                highCount.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_thread->enqueue(DispatchPriority::High, std::move(highTask));
    }

    for (int i = 0; i < 5; ++i)
    {
        std::packaged_task<StatusCode()> lowTask(
            [&lowCount]() -> StatusCode
            {
                lowCount.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_thread->enqueue(DispatchPriority::Low, std::move(lowTask));
    }

    gate.count_down();

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (lowCount.load(std::memory_order_relaxed) > 0 &&
            highCount.load(std::memory_order_relaxed) >= 20)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(highCount.load(), 20);
    EXPECT_GT(lowCount.load(), 0) << "Low priority tasks must not be starved";
    EXPECT_EQ(lowCount.load(), 5);
}

TEST_F(VertexPriorityThreadTest, RecurringTaskExecutesRepeatedly)
{
    std::atomic<int> callCount {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    const auto deadline = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= 10)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_GE(callCount.load(), 10) << "Recurring task should execute many times";

    const auto status = m_thread->remove_recurring(handle.value());
    EXPECT_EQ(status, StatusCode::STATUS_OK);
}

TEST_F(VertexPriorityThreadTest, RecurringCancelStopsFurtherInvocations)
{
    std::atomic<int> callCount {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 5)
    {
        std::this_thread::sleep_for(1ms);
    }

    const auto status = m_thread->remove_recurring(handle.value());
    EXPECT_EQ(status, StatusCode::STATUS_OK);

    const auto countAtCancel = callCount.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(50ms);
    const auto countAfterWait = callCount.load(std::memory_order_relaxed);

    EXPECT_EQ(countAtCancel, countAfterWait)
        << "No further invocations should occur after cancel returns";
}

TEST_F(VertexPriorityThreadTest, RecurringCancelIsIdempotent)
{
    auto handle = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::FixedDelay,
        10ms,
        []() -> StatusCode { return StatusCode::STATUS_OK; },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    const auto first = m_thread->remove_recurring(handle.value());
    EXPECT_EQ(first, StatusCode::STATUS_OK);

    std::this_thread::sleep_for(20ms);

    const auto second = m_thread->remove_recurring(handle.value());
    EXPECT_EQ(second, StatusCode::STATUS_OK) << "Second cancel must succeed (idempotent)";

    const auto third = m_thread->remove_recurring(handle.value());
    EXPECT_EQ(third, StatusCode::STATUS_OK) << "Third cancel must succeed (idempotent)";
}

TEST_F(VertexPriorityThreadTest, RecurringNoReentrancy)
{
    std::atomic<int> concurrency {};
    std::atomic<int> maxConcurrency {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&concurrency, &maxConcurrency]() -> StatusCode
        {
            const auto prev = concurrency.fetch_add(1, std::memory_order_relaxed);
            const auto current = prev + 1;

            auto expected = maxConcurrency.load(std::memory_order_relaxed);
            while (current > expected &&
                   !maxConcurrency.compare_exchange_weak(expected, current,
                       std::memory_order_relaxed))
            {
            }

            std::this_thread::sleep_for(1ms);
            concurrency.fetch_sub(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    std::this_thread::sleep_for(100ms);

    std::ignore = m_thread->remove_recurring(handle.value());

    EXPECT_EQ(maxConcurrency.load(), 1) << "Recurring task must not execute concurrently with itself";
}

TEST_F(VertexPriorityThreadTest, CancelFromInsideCallbackIsDeadlockFree)
{
    std::atomic<bool> cancelled {};
    RecurringTaskHandle capturedHandle {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [this, &cancelled, &capturedHandle]() -> StatusCode
        {
            if (!cancelled.load(std::memory_order_relaxed))
            {
                cancelled.store(true, std::memory_order_relaxed);
                std::ignore = m_thread->remove_recurring(capturedHandle);
            }
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());
    capturedHandle = handle.value();

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (cancelled.load(std::memory_order_relaxed))
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_TRUE(cancelled.load()) << "Self-cancellation should complete without deadlock";
}

TEST_F(VertexPriorityThreadTest, StaleHandleCancelReturnsError)
{
    auto handle = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::FixedDelay,
        100ms,
        []() -> StatusCode { return StatusCode::STATUS_OK; },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    m_thread->invalidate_epoch(1);

    const auto status = m_thread->remove_recurring(handle.value());
    EXPECT_EQ(status, StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(VertexPriorityThreadTest, CancelOnFailurePolicyStopsRecurring)
{
    std::atomic<int> callCount {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_ERROR_GENERAL;
        },
        RecurringFailurePolicy::CancelOnFailure,
        0);

    ASSERT_TRUE(handle.has_value());

    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(callCount.load(), 1) << "CancelOnFailure should stop after first failure";
}

TEST_F(VertexPriorityThreadTest, QueueFloodDoesNotBlockHighPriority)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_thread->enqueue(DispatchPriority::High, std::move(blocker));

    for (int i = 0; i < 100; ++i)
    {
        std::packaged_task<StatusCode()> lowTask(
            []() -> StatusCode { return StatusCode::STATUS_OK; });
        std::ignore = m_thread->enqueue(DispatchPriority::Low, std::move(lowTask));
    }

    std::atomic<bool> highDone {};
    std::packaged_task<StatusCode()> highTask(
        [&highDone]() -> StatusCode
        {
            highDone.store(true, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto highFuture = m_thread->enqueue(DispatchPriority::High, std::move(highTask));
    ASSERT_TRUE(highFuture.has_value());

    gate.count_down();

    const auto status = highFuture->get();
    EXPECT_EQ(status, StatusCode::STATUS_OK);
    EXPECT_TRUE(highDone.load());
}

TEST_F(VertexPriorityThreadTest, StopDrainsAllQueues)
{
    for (int i = 0; i < 10; ++i)
    {
        auto task = make_task(StatusCode::STATUS_OK);
        std::ignore = m_thread->enqueue(DispatchPriority::Normal, std::move(task));
    }

    m_thread.reset();

    SUCCEED() << "Stop completed without hanging";
}

TEST_F(VertexPriorityThreadTest, BackoffOnFailureDoublesDelay)
{
    std::atomic<int> callCount {};
    std::vector<std::chrono::steady_clock::time_point> timestamps {};
    std::mutex tsMutex {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::FixedDelay,
        10ms,
        [&callCount, &timestamps, &tsMutex]() -> StatusCode
        {
            {
                std::scoped_lock lock {tsMutex};
                timestamps.push_back(std::chrono::steady_clock::now());
            }
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_ERROR_GENERAL;
        },
        RecurringFailurePolicy::BackoffOnFailure,
        0);

    ASSERT_TRUE(handle.has_value());

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= 4)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    std::ignore = m_thread->remove_recurring(handle.value());

    ASSERT_GE(callCount.load(), 4) << "Should have executed at least 4 times under backoff";

    std::scoped_lock lock {tsMutex};
    ASSERT_GE(timestamps.size(), 4u);

    for (std::size_t i {2}; i < timestamps.size(); ++i)
    {
        const auto prevGap = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamps[i - 1] - timestamps[i - 2]);
        const auto currGap = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamps[i] - timestamps[i - 1]);
        EXPECT_GE(currGap.count(), prevGap.count() - 5)
            << "Gap at index " << i << " should be >= previous gap (with tolerance)";
    }
}

TEST_F(VertexPriorityThreadTest, BackoffOnFailureResetsOnSuccess)
{
    std::atomic<int> callCount {};
    std::atomic<bool> shouldFail {true};

    auto handle = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::FixedDelay,
        5ms,
        [&callCount, &shouldFail]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            if (shouldFail.load(std::memory_order_relaxed))
            {
                return StatusCode::STATUS_ERROR_GENERAL;
            }
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::BackoffOnFailure,
        0);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(1ms);
    }

    shouldFail.store(false, std::memory_order_relaxed);

    const auto countBeforeReset = callCount.load(std::memory_order_relaxed);
    const auto deadline = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= countBeforeReset + 10)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    std::ignore = m_thread->remove_recurring(handle.value());

    EXPECT_GE(callCount.load(), countBeforeReset + 10)
        << "After success, delay should reset and task should run frequently again";
}

TEST_F(VertexPriorityThreadTest, FixedDelayRespectsInterval)
{
    std::atomic<int> callCount {};
    std::vector<std::chrono::steady_clock::time_point> timestamps {};
    std::mutex tsMutex {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::FixedDelay,
        50ms,
        [&callCount, &timestamps, &tsMutex]() -> StatusCode
        {
            {
                std::scoped_lock lock {tsMutex};
                timestamps.push_back(std::chrono::steady_clock::now());
            }
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= 5)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    std::ignore = m_thread->remove_recurring(handle.value());

    std::scoped_lock lock {tsMutex};
    ASSERT_GE(timestamps.size(), 3u);

    for (std::size_t i {1}; i < timestamps.size(); ++i)
    {
        const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamps[i] - timestamps[i - 1]);
        EXPECT_GE(gap.count(), 40) << "Gap at index " << i << " should respect ~50ms delay";
    }
}

TEST_F(VertexPriorityThreadTest, FixedDelayMeasuredAfterCompletion)
{
    std::atomic<int> callCount {};
    std::vector<std::chrono::steady_clock::time_point> endTimestamps {};
    std::mutex tsMutex {};

    auto handle = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::FixedDelay,
        30ms,
        [&callCount, &endTimestamps, &tsMutex]() -> StatusCode
        {
            std::this_thread::sleep_for(20ms);
            {
                std::scoped_lock lock {tsMutex};
                endTimestamps.push_back(std::chrono::steady_clock::now());
            }
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handle.has_value());

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= 4)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    std::ignore = m_thread->remove_recurring(handle.value());

    std::scoped_lock lock {tsMutex};
    ASSERT_GE(endTimestamps.size(), 3u);

    for (std::size_t i {1}; i < endTimestamps.size(); ++i)
    {
        const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTimestamps[i] - endTimestamps[i - 1]);
        EXPECT_GE(gap.count(), 45)
            << "Gap between completions should be >= task_duration(20ms) + delay(30ms) - tolerance";
    }
}

TEST_F(VertexPriorityThreadTest, NormalPriorityRunsBetweenHighAndLow)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_thread->enqueue(DispatchPriority::High, std::move(blocker));

    std::atomic<int> counter {};
    std::vector<int> order {};
    std::mutex orderMutex {};

    for (int i = 0; i < 3; ++i)
    {
        std::packaged_task<StatusCode()> lowTask(
            [&counter, i, &order, &orderMutex]() -> StatusCode
            {
                {
                    std::scoped_lock lock {orderMutex};
                    order.push_back(300 + i);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_thread->enqueue(DispatchPriority::Low, std::move(lowTask));
    }

    for (int i = 0; i < 3; ++i)
    {
        std::packaged_task<StatusCode()> normalTask(
            [&counter, i, &order, &orderMutex]() -> StatusCode
            {
                {
                    std::scoped_lock lock {orderMutex};
                    order.push_back(200 + i);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_thread->enqueue(DispatchPriority::Normal, std::move(normalTask));
    }

    for (int i = 0; i < 3; ++i)
    {
        std::packaged_task<StatusCode()> highTask(
            [&counter, i, &order, &orderMutex]() -> StatusCode
            {
                {
                    std::scoped_lock lock {orderMutex};
                    order.push_back(100 + i);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_thread->enqueue(DispatchPriority::High, std::move(highTask));
    }

    gate.count_down();

    while (counter.load(std::memory_order_relaxed) < 9)
    {
        std::this_thread::sleep_for(1ms);
    }

    std::scoped_lock lock {orderMutex};
    ASSERT_EQ(order.size(), 9u);

    int firstNormalIndex {-1};
    int firstLowIndex {-1};
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
    {
        if (order[i] >= 200 && order[i] < 300 && firstNormalIndex == -1)
        {
            firstNormalIndex = i;
        }
        if (order[i] >= 300 && firstLowIndex == -1)
        {
            firstLowIndex = i;
        }
    }

    EXPECT_NE(firstNormalIndex, -1);
    EXPECT_NE(firstLowIndex, -1);
    EXPECT_LT(firstNormalIndex, firstLowIndex)
        << "Normal priority tasks should run before Low priority tasks";
}

TEST_F(VertexPriorityThreadTest, AsSoonAsPossibleRejectsNonZeroDelay)
{
    auto handle = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::AsSoonAsPossible,
        10ms,
        []() -> StatusCode { return StatusCode::STATUS_OK; },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_FALSE(handle.has_value());
    EXPECT_EQ(handle.error(), StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(VertexPriorityThreadTest, RecurringRejectsNullCallable)
{
    auto handle = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::FixedDelay,
        10ms,
        nullptr,
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_FALSE(handle.has_value());
    EXPECT_EQ(handle.error(), StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(VertexPriorityThreadTest, EnqueueAfterStopReturnsError)
{
    std::ignore = m_thread->stop();

    auto task = make_task(StatusCode::STATUS_OK);
    auto result = m_thread->enqueue(DispatchPriority::Normal, std::move(task));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING);
}

TEST_F(VertexPriorityThreadTest, EpochInvalidationCancelsAllOlderRecurring)
{
    std::atomic<int> callCountA {};
    std::atomic<int> callCountB {};

    auto handleA = m_thread->add_recurring(
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCountA]() -> StatusCode
        {
            callCountA.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    auto handleB = m_thread->add_recurring(
        DispatchPriority::Normal,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCountB]() -> StatusCode
        {
            callCountB.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue,
        0);

    ASSERT_TRUE(handleA.has_value());
    ASSERT_TRUE(handleB.has_value());

    while (callCountA.load(std::memory_order_relaxed) < 3 ||
           callCountB.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(1ms);
    }

    m_thread->invalidate_epoch(1);

    const auto countA = callCountA.load(std::memory_order_relaxed);
    const auto countB = callCountB.load(std::memory_order_relaxed);

    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(callCountA.load(), countA) << "Task A must stop after epoch invalidation";
    EXPECT_EQ(callCountB.load(), countB) << "Task B must stop after epoch invalidation";
}
