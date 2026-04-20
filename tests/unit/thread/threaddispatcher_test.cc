//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/thread/threaddispatcher.hh>

#include <sdk/feature.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>

using namespace Vertex::Thread;
using namespace std::chrono_literals;

class ThreadDispatcherTest : public ::testing::Test
{
protected:
    std::unique_ptr<ThreadDispatcher> m_dispatcher {};

    void SetUp() override
    {
        m_dispatcher = std::make_unique<ThreadDispatcher>();
    }

    void TearDown() override
    {
        m_dispatcher.reset();
    }

    static std::packaged_task<StatusCode()> make_task(StatusCode result)
    {
        return std::packaged_task<StatusCode()>(
            [result]() -> StatusCode
            {
                return result;
            });
    }
};

class ThreadDispatcherMultiThreadedTest : public ThreadDispatcherTest
{
protected:
    void SetUp() override
    {
        ThreadDispatcherTest::SetUp();
        ASSERT_EQ(m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_STANDARD), StatusCode::STATUS_OK);
        ASSERT_EQ(m_dispatcher->start(), StatusCode::STATUS_OK);
    }
};

class ThreadDispatcherSingleThreadDependentTest : public ThreadDispatcherTest
{
protected:
    void SetUp() override
    {
        ThreadDispatcherTest::SetUp();
        ASSERT_EQ(m_dispatcher->configure(
            VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED | VERTEX_FEATURE_DEBUGGER_DEPENDENT),
            StatusCode::STATUS_OK);
        ASSERT_EQ(m_dispatcher->start(), StatusCode::STATUS_OK);
    }
};

class ThreadDispatcherSingleThreadIndependentTest : public ThreadDispatcherTest
{
protected:
    void SetUp() override
    {
        ThreadDispatcherTest::SetUp();
        ASSERT_EQ(m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED), StatusCode::STATUS_OK);
        ASSERT_EQ(m_dispatcher->start(), StatusCode::STATUS_OK);
    }
};





TEST_F(ThreadDispatcherMultiThreadedTest, DispatchWithPriorityHighExecutesOnDebugger)
{
    std::atomic<bool> executed {};

    std::packaged_task<StatusCode()> task(
        [&executed]() -> StatusCode
        {
            executed.store(true, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto result = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Debugger, DispatchPriority::High, std::move(task));

    ASSERT_TRUE(result.has_value());

    const auto status = result->get();
    EXPECT_EQ(status, StatusCode::STATUS_OK);
    EXPECT_TRUE(executed.load());
}

TEST_F(ThreadDispatcherMultiThreadedTest, DispatchWithPriorityLowExecutesOnDebugger)
{
    std::atomic<bool> executed {};

    std::packaged_task<StatusCode()> task(
        [&executed]() -> StatusCode
        {
            executed.store(true, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto result = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Debugger, DispatchPriority::Low, std::move(task));

    ASSERT_TRUE(result.has_value());

    const auto status = result->get();
    EXPECT_EQ(status, StatusCode::STATUS_OK);
    EXPECT_TRUE(executed.load());
}

TEST_F(ThreadDispatcherMultiThreadedTest, DispatchWithPriorityNonDebuggerFallsBackToNormal)
{
    std::atomic<bool> executed {};

    std::packaged_task<StatusCode()> task(
        [&executed]() -> StatusCode
        {
            executed.store(true, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto result = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Freeze, DispatchPriority::High, std::move(task));

    ASSERT_TRUE(result.has_value());

    const auto status = result->get();
    EXPECT_EQ(status, StatusCode::STATUS_OK);
    EXPECT_TRUE(executed.load());
}

TEST_F(ThreadDispatcherMultiThreadedTest, DispatchWithPriorityHighBeforeLow)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Debugger, DispatchPriority::High, std::move(blocker));

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
                    order.push_back(100 + i);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_dispatcher->dispatch_with_priority(
            ThreadChannel::Debugger, DispatchPriority::Low, std::move(lowTask));
    }

    for (int i = 0; i < 3; ++i)
    {
        std::packaged_task<StatusCode()> highTask(
            [&counter, i, &order, &orderMutex]() -> StatusCode
            {
                {
                    std::scoped_lock lock {orderMutex};
                    order.push_back(i);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
                return StatusCode::STATUS_OK;
            });
        std::ignore = m_dispatcher->dispatch_with_priority(
            ThreadChannel::Debugger, DispatchPriority::High, std::move(highTask));
    }

    gate.count_down();

    while (counter.load(std::memory_order_relaxed) < 6)
    {
        std::this_thread::sleep_for(1ms);
    }

    std::scoped_lock lock {orderMutex};

    int firstLowIndex {-1};
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
    {
        if (order[i] >= 100 && firstLowIndex == -1)
        {
            firstLowIndex = i;
        }
    }

    EXPECT_GT(firstLowIndex, 0)
        << "Low-priority tasks should not run first through dispatcher when High tasks are pending";
}





TEST_F(ThreadDispatcherMultiThreadedTest, ScheduleRecurringOnDebuggerChannel)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Debugger,
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

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

    EXPECT_GE(callCount.load(), 10);

    const auto status = m_dispatcher->cancel_recurring(handle.value());
    EXPECT_EQ(status, StatusCode::STATUS_OK);
}

TEST_F(ThreadDispatcherMultiThreadedTest, ScheduleRecurringOnNonDebuggerChannelSucceeds)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Scanner,
        DispatchPriority::Normal,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(m_dispatcher->cancel_recurring(handle.value()), StatusCode::STATUS_OK);
}

TEST_F(ThreadDispatcherMultiThreadedTest, ScheduleRecurringOnUiChannelRejected)
{
    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::UI,
        DispatchPriority::Normal,
        RecurringPolicy::FixedDelay,
        100ms,
        []() -> StatusCode { return StatusCode::STATUS_OK; },
        RecurringFailurePolicy::Continue);

    ASSERT_FALSE(handle.has_value());
    EXPECT_EQ(handle.error(), StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(ThreadDispatcherMultiThreadedTest, ScheduleRecurringPersistentSurvivesConfigure)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring_persistent(
        ThreadChannel::Scanner,
        DispatchPriority::Low,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(1ms);
    }

    ASSERT_EQ(m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_STANDARD), StatusCode::STATUS_OK);

    const auto before = callCount.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(20ms);
    EXPECT_GT(callCount.load(std::memory_order_relaxed), before);

    EXPECT_EQ(m_dispatcher->cancel_recurring(handle.value()), StatusCode::STATUS_OK);
}

TEST_F(ThreadDispatcherMultiThreadedTest, CancelRecurringStopsExecution)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Debugger,
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 5)
    {
        std::this_thread::sleep_for(1ms);
    }

    const auto status = m_dispatcher->cancel_recurring(handle.value());
    EXPECT_EQ(status, StatusCode::STATUS_OK);

    const auto countAtCancel = callCount.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(callCount.load(), countAtCancel);
}

TEST_F(ThreadDispatcherMultiThreadedTest, ConfigureInvalidatesRecurringHandles)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Debugger,
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(1ms);
    }

    ASSERT_EQ(m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_STANDARD), StatusCode::STATUS_OK);

    const auto countAfterConfigure = callCount.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(callCount.load(), countAfterConfigure)
        << "Recurring tasks must be invalidated after configure()";

    const auto status = m_dispatcher->cancel_recurring(handle.value());
    EXPECT_EQ(status, StatusCode::STATUS_ERROR_INVALID_PARAMETER)
        << "Stale handle after configure() must return error";
}

TEST_F(ThreadDispatcherMultiThreadedTest, StopInvalidatesRecurringHandles)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Debugger,
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    while (callCount.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(1ms);
    }

    ASSERT_EQ(m_dispatcher->stop(), StatusCode::STATUS_OK);

    const auto countAfterStop = callCount.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(callCount.load(), countAfterStop)
        << "Recurring tasks must stop after stop()";
}





TEST_F(ThreadDispatcherMultiThreadedTest, DebuggerChannelRoutesToPriorityThread)
{
    std::atomic<std::thread::id> executionThread {};

    std::packaged_task<StatusCode()> task(
        [&executionThread]() -> StatusCode
        {
            executionThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto result = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(task));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), StatusCode::STATUS_OK);

    EXPECT_NE(executionThread.load(), std::this_thread::get_id())
        << "Debugger task should execute on a different thread";
}

TEST_F(ThreadDispatcherMultiThreadedTest, NonDebuggerChannelRoutesToDedicatedThread)
{
    std::atomic<std::thread::id> freezeThread {};
    std::atomic<std::thread::id> debuggerThread {};

    std::packaged_task<StatusCode()> freezeTask(
        [&freezeThread]() -> StatusCode
        {
            freezeThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    std::packaged_task<StatusCode()> debuggerTask(
        [&debuggerThread]() -> StatusCode
        {
            debuggerThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto r1 = m_dispatcher->dispatch(ThreadChannel::Freeze, std::move(freezeTask));
    auto r2 = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(debuggerTask));
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->get(), StatusCode::STATUS_OK);
    EXPECT_EQ(r2->get(), StatusCode::STATUS_OK);

    EXPECT_NE(freezeThread.load(), debuggerThread.load())
        << "Debugger and Freeze should run on different threads in multi-threaded mode";
}

TEST_F(ThreadDispatcherMultiThreadedTest, IsNotSingleThreaded)
{
    EXPECT_FALSE(m_dispatcher->is_single_threaded());
}





TEST_F(ThreadDispatcherTest, MultiThreadedWithDependentFlagIgnoresBit1)
{
    ASSERT_EQ(m_dispatcher->configure(
        VERTEX_FEATURE_RUN_MODE_STANDARD | VERTEX_FEATURE_DEBUGGER_DEPENDENT),
        StatusCode::STATUS_OK);
    ASSERT_EQ(m_dispatcher->start(), StatusCode::STATUS_OK);

    EXPECT_FALSE(m_dispatcher->is_single_threaded());

    std::atomic<std::thread::id> freezeThread {};
    std::atomic<std::thread::id> debuggerThread {};

    std::packaged_task<StatusCode()> freezeTask(
        [&freezeThread]() -> StatusCode
        {
            freezeThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    std::packaged_task<StatusCode()> debuggerTask(
        [&debuggerThread]() -> StatusCode
        {
            debuggerThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto r1 = m_dispatcher->dispatch(ThreadChannel::Freeze, std::move(freezeTask));
    auto r2 = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(debuggerTask));
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->get(), StatusCode::STATUS_OK);
    EXPECT_EQ(r2->get(), StatusCode::STATUS_OK);

    EXPECT_NE(freezeThread.load(), debuggerThread.load())
        << "In multi-threaded mode, bit1 should be ignored; channels should be independent";
}





TEST_F(ThreadDispatcherSingleThreadDependentTest, IsSingleThreaded)
{
    EXPECT_TRUE(m_dispatcher->is_single_threaded());
}

TEST_F(ThreadDispatcherSingleThreadDependentTest, AllChannelsShareSameThread)
{
    std::atomic<std::thread::id> debuggerThread {};
    std::atomic<std::thread::id> freezeThread {};

    std::packaged_task<StatusCode()> debuggerTask(
        [&debuggerThread]() -> StatusCode
        {
            debuggerThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto r1 = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(debuggerTask));
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->get(), StatusCode::STATUS_OK);

    std::packaged_task<StatusCode()> freezeTask(
        [&freezeThread]() -> StatusCode
        {
            freezeThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto r2 = m_dispatcher->dispatch(ThreadChannel::Freeze, std::move(freezeTask));
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->get(), StatusCode::STATUS_OK);

    EXPECT_EQ(debuggerThread.load(), freezeThread.load())
        << "In single-threaded dependent mode, all channels must share the same thread";
}

TEST_F(ThreadDispatcherSingleThreadDependentTest, ScheduleRecurringWorks)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Debugger,
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    const auto deadline = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= 5)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_GE(callCount.load(), 5);

    EXPECT_EQ(m_dispatcher->cancel_recurring(handle.value()), StatusCode::STATUS_OK);
}

TEST_F(ThreadDispatcherSingleThreadDependentTest, DispatchWithPriorityWorks)
{
    std::atomic<bool> executed {};

    std::packaged_task<StatusCode()> task(
        [&executed]() -> StatusCode
        {
            executed.store(true, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto result = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Debugger, DispatchPriority::Low, std::move(task));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), StatusCode::STATUS_OK);
    EXPECT_TRUE(executed.load());
}





TEST_F(ThreadDispatcherSingleThreadIndependentTest, IsSingleThreaded)
{
    EXPECT_TRUE(m_dispatcher->is_single_threaded());
}

TEST_F(ThreadDispatcherSingleThreadIndependentTest, DebuggerRunsOnDedicatedThread)
{
    std::atomic<std::thread::id> debuggerThread {};
    std::atomic<std::thread::id> freezeThread {};

    std::packaged_task<StatusCode()> debuggerTask(
        [&debuggerThread]() -> StatusCode
        {
            debuggerThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto r1 = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(debuggerTask));
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->get(), StatusCode::STATUS_OK);

    std::packaged_task<StatusCode()> freezeTask(
        [&freezeThread]() -> StatusCode
        {
            freezeThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto r2 = m_dispatcher->dispatch(ThreadChannel::Freeze, std::move(freezeTask));
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->get(), StatusCode::STATUS_OK);

    EXPECT_NE(debuggerThread.load(), freezeThread.load())
        << "In single-threaded independent mode, debugger should run on a dedicated thread";
}

TEST_F(ThreadDispatcherSingleThreadIndependentTest, NonDebuggerChannelsShareMPSCThread)
{
    std::atomic<std::thread::id> freezeThread {};
    std::atomic<std::thread::id> processListThread {};

    std::packaged_task<StatusCode()> freezeTask(
        [&freezeThread]() -> StatusCode
        {
            freezeThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto r1 = m_dispatcher->dispatch(ThreadChannel::Freeze, std::move(freezeTask));
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->get(), StatusCode::STATUS_OK);

    std::packaged_task<StatusCode()> processListTask(
        [&processListThread]() -> StatusCode
        {
            processListThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    auto r2 = m_dispatcher->dispatch(ThreadChannel::ProcessList, std::move(processListTask));
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->get(), StatusCode::STATUS_OK);

    EXPECT_EQ(freezeThread.load(), processListThread.load())
        << "Non-debugger channels should share the same MPSC thread in single-threaded independent mode";
}

TEST_F(ThreadDispatcherSingleThreadIndependentTest, ScheduleRecurringOnDebuggerWorks)
{
    std::atomic<int> callCount {};

    auto handle = m_dispatcher->schedule_recurring(
        ThreadChannel::Debugger,
        DispatchPriority::High,
        RecurringPolicy::AsSoonAsPossible,
        0ms,
        [&callCount]() -> StatusCode
        {
            callCount.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        },
        RecurringFailurePolicy::Continue);

    ASSERT_TRUE(handle.has_value());

    const auto deadline = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callCount.load(std::memory_order_relaxed) >= 5)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_GE(callCount.load(), 5);

    EXPECT_EQ(m_dispatcher->cancel_recurring(handle.value()), StatusCode::STATUS_OK);
}





TEST_F(ThreadDispatcherTest, ReconfigureFromMultiToSingleThreaded)
{
    ASSERT_EQ(m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_STANDARD), StatusCode::STATUS_OK);
    ASSERT_EQ(m_dispatcher->start(), StatusCode::STATUS_OK);
    EXPECT_FALSE(m_dispatcher->is_single_threaded());

    ASSERT_EQ(m_dispatcher->configure(
        VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED | VERTEX_FEATURE_DEBUGGER_DEPENDENT),
        StatusCode::STATUS_OK);
    EXPECT_TRUE(m_dispatcher->is_single_threaded());

    std::atomic<bool> executed {};
    std::packaged_task<StatusCode()> task(
        [&executed]() -> StatusCode
        {
            executed.store(true, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });

    auto result = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(task));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), StatusCode::STATUS_OK);
    EXPECT_TRUE(executed.load());
}





TEST_F(ThreadDispatcherMultiThreadedTest, LegacyDispatchDebuggerUsesNormalPriority)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Debugger, DispatchPriority::High, std::move(blocker));

    std::atomic<int> counter {};
    std::vector<int> order {};
    std::mutex orderMutex {};

    std::packaged_task<StatusCode()> highTask(
        [&counter, &order, &orderMutex]() -> StatusCode
        {
            {
                std::scoped_lock lock {orderMutex};
                order.push_back(1);
            }
            counter.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_dispatcher->dispatch_with_priority(
        ThreadChannel::Debugger, DispatchPriority::High, std::move(highTask));

    std::packaged_task<StatusCode()> legacyTask(
        [&counter, &order, &orderMutex]() -> StatusCode
        {
            {
                std::scoped_lock lock {orderMutex};
                order.push_back(2);
            }
            counter.fetch_add(1, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(legacyTask));

    gate.count_down();

    while (counter.load(std::memory_order_relaxed) < 2)
    {
        std::this_thread::sleep_for(1ms);
    }

    std::scoped_lock lock {orderMutex};
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1) << "High-priority task should execute before legacy (Normal priority) task";
    EXPECT_EQ(order[1], 2);
}





TEST_F(ThreadDispatcherMultiThreadedTest, PendingTasksReflectsDebuggerQueue)
{
    std::latch gate {1};
    std::packaged_task<StatusCode()> blocker(
        [&gate]() -> StatusCode
        {
            gate.wait();
            return StatusCode::STATUS_OK;
        });
    std::ignore = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(blocker));

    std::this_thread::sleep_for(10ms);

    for (int i = 0; i < 5; ++i)
    {
        auto task = make_task(StatusCode::STATUS_OK);
        std::ignore = m_dispatcher->dispatch(ThreadChannel::Debugger, std::move(task));
    }

    EXPECT_GE(m_dispatcher->pending_tasks(ThreadChannel::Debugger), 1u);

    gate.count_down();

    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (m_dispatcher->pending_tasks(ThreadChannel::Debugger) == 0)
        {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(m_dispatcher->pending_tasks(ThreadChannel::Debugger), 0u);
}
