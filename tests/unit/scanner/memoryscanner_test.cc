//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../include/vertex/scanner/memoryscanner/memoryscanner.hh"
#include <vertex/scanner/imemoryreader.hh>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    class MockMemoryReader : public Vertex::Scanner::IMemoryReader
    {
    public:
        MOCK_METHOD(StatusCode, read_memory, (std::uint64_t address, std::uint64_t size, void* buffer), (override));
    };
}

class MemoryScannerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mockSettings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        mockLog = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        mockDispatcher = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>>();

        ON_CALL(*mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(2));
        ON_CALL(*mockSettings, get_int(::testing::HasSubstr("threadBufferSizeMB"), _)).WillByDefault(Return(32));
        ON_CALL(*mockSettings, get_int(::testing::HasSubstr("workerChunkSizeMB"), _)).WillByDefault(Return(8));
        ON_CALL(*mockDispatcher, is_single_threaded()).WillByDefault(Return(false));
        ON_CALL(*mockDispatcher, create_worker_pool(_, _)).WillByDefault(Return(StatusCode::STATUS_OK));
        ON_CALL(*mockDispatcher, destroy_worker_pool(_)).WillByDefault(Return(StatusCode::STATUS_OK));
        ON_CALL(*mockDispatcher, enqueue_on_worker(_, _, _)).WillByDefault(Return(StatusCode::STATUS_OK));

        scanner = std::make_unique<Vertex::Scanner::MemoryScanner>(*mockSettings, *mockLog, *mockDispatcher);
    }

    void TearDown() override { scanner->stop_scan(); }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> mockLog;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> mockDispatcher;
    std::unique_ptr<Vertex::Scanner::MemoryScanner> scanner;
};



TEST_F(MemoryScannerTest, HasMemoryReader_NoReaderSet_ReturnsFalse)
{
    bool result = scanner->has_memory_reader();

    EXPECT_FALSE(result);
}

TEST_F(MemoryScannerTest, HasMemoryReader_ReaderSet_ReturnsTrue)
{
    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();

    scanner->set_memory_reader(mockReader);
    bool result = scanner->has_memory_reader();

    EXPECT_TRUE(result);
}



TEST_F(MemoryScannerTest, InitializeScan_EmptyMemoryRegions_ReturnsError)
{
    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    scanner->set_memory_reader(mockReader);

    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);
    std::vector<Vertex::Scanner::ScanRegion> emptyRegions;

    StatusCode result = scanner->initialize_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType), emptyRegions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(MemoryScannerTest, InitializeScan_NoMemoryReader_ReturnsError)
{
    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);

    std::vector<Vertex::Scanner::ScanRegion> regions;
    regions.push_back(Vertex::Scanner::ScanRegion{.baseAddress = 0x1000, .size = 4096});

    StatusCode result = scanner->initialize_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType), regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}



TEST_F(MemoryScannerTest, StopScan_Succeeds)
{
    StatusCode result = scanner->stop_scan();

    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MemoryScannerTest, IsScanComplete_NoScanStarted_ReturnsTrue)
{
    bool result = scanner->is_scan_complete();

    EXPECT_TRUE(result);
}



TEST_F(MemoryScannerTest, GetRegionsScanned_InitiallyZero)
{
    std::uint64_t result = scanner->get_regions_scanned();

    EXPECT_EQ(0, result);
}

TEST_F(MemoryScannerTest, GetTotalRegions_InitiallyZero)
{
    std::uint64_t result = scanner->get_total_regions();

    EXPECT_EQ(0, result);
}

TEST_F(MemoryScannerTest, GetResultsCount_InitiallyZero)
{
    std::uint64_t result = scanner->get_results_count();

    EXPECT_EQ(0, result);
}



TEST_F(MemoryScannerTest, CanUndo_NoScansPerformed_ReturnsFalse)
{
    bool result = scanner->can_undo();

    EXPECT_FALSE(result);
}

TEST_F(MemoryScannerTest, UndoScan_NoHistory_ReturnsError)
{
    StatusCode result = scanner->undo_scan();

    EXPECT_EQ(StatusCode::STATUS_ERROR_GENERAL, result);
}



TEST_F(MemoryScannerTest, SetScanAbortState_True_SetsState)
{
    scanner->set_scan_abort_state(true);

    StatusCode result = scanner->stop_scan();
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MemoryScannerTest, SetScanAbortState_False_SetsState)
{
    scanner->set_scan_abort_state(false);

    StatusCode result = scanner->stop_scan();
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}



TEST_F(MemoryScannerTest, IsScanActive_NoScanRunning_ReturnsOK)
{
    StatusCode result = scanner->is_scan_active();

    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MemoryScannerTest, InitializeNextScan_RecreatesWorkerPoolWithSameThreadCount)
{
    ON_CALL(*mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(1));

    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    scanner->set_memory_reader(mockReader);

    constexpr std::int32_t expectedValue = 1337;
    ON_CALL(*mockReader, read_memory(_, _, _))
      .WillByDefault(Invoke(
        [expectedValue](std::uint64_t, std::uint64_t size, void* buffer) -> StatusCode
        {
            if (buffer == nullptr || size < sizeof(expectedValue))
            {
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }

            std::memset(buffer, 0, static_cast<std::size_t>(size));
            std::memcpy(buffer, &expectedValue, sizeof(expectedValue));
            return StatusCode::STATUS_OK;
        }));

    ON_CALL(*mockDispatcher, enqueue_on_worker(_, _, _))
      .WillByDefault(Invoke(
        [](Vertex::Thread::ThreadChannel, std::size_t, std::packaged_task<StatusCode()>&& task) -> StatusCode
        {
            task();
            return StatusCode::STATUS_OK;
        }));

    EXPECT_CALL(*mockDispatcher, create_worker_pool(Vertex::Thread::ThreadChannel::Scanner, 1)).Times(2);

    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);
    config.alignmentRequired = true;
    config.alignment = sizeof(expectedValue);
    const auto* valueBytes = reinterpret_cast<const std::uint8_t*>(&expectedValue);
    config.input.assign(valueBytes, valueBytes + sizeof(expectedValue));

    std::vector<Vertex::Scanner::ScanRegion> regions{
        Vertex::Scanner::ScanRegion{.baseAddress = 0x1000, .size = sizeof(expectedValue)}
    };

    EXPECT_EQ(StatusCode::STATUS_OK, scanner->initialize_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType), regions));
    EXPECT_TRUE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());

    EXPECT_EQ(StatusCode::STATUS_OK, scanner->initialize_next_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType)));
    EXPECT_TRUE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());
}

TEST_F(MemoryScannerTest, InitializeNextScan_BlocksFastPathUntilFinalize)
{
    ON_CALL(*mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(1));

    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    scanner->set_memory_reader(mockReader);

    constexpr std::int32_t expectedValue = 1337;
    ON_CALL(*mockReader, read_memory(_, _, _))
      .WillByDefault(Invoke(
        [expectedValue](std::uint64_t, std::uint64_t size, void* buffer) -> StatusCode
        {
            if (buffer == nullptr || size < sizeof(expectedValue))
            {
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }

            std::memset(buffer, 0, static_cast<std::size_t>(size));
            std::memcpy(buffer, &expectedValue, sizeof(expectedValue));
            return StatusCode::STATUS_OK;
        }));

    ON_CALL(*mockDispatcher, enqueue_on_worker(_, _, _))
      .WillByDefault(Invoke(
        [](Vertex::Thread::ThreadChannel, std::size_t, std::packaged_task<StatusCode()>&& task) -> StatusCode
        {
            task();
            return StatusCode::STATUS_OK;
        }));

    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);
    config.alignmentRequired = true;
    config.alignment = sizeof(expectedValue);
    const auto* valueBytes = reinterpret_cast<const std::uint8_t*>(&expectedValue);
    config.input.assign(valueBytes, valueBytes + sizeof(expectedValue));

    std::vector<Vertex::Scanner::ScanRegion> regions{
        Vertex::Scanner::ScanRegion{.baseAddress = 0x1000, .size = sizeof(expectedValue)}
    };

    EXPECT_EQ(StatusCode::STATUS_OK, scanner->initialize_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType), regions));
    EXPECT_TRUE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());

    std::mutex gateMutex;
    std::condition_variable gateCv;
    bool firstReaderDone = false;
    bool releaseEnqueue = false;
    std::atomic<bool> pauseFirstEnqueue{true};

    ON_CALL(*mockDispatcher, enqueue_on_worker(_, _, _))
      .WillByDefault(Invoke(
        [&](Vertex::Thread::ThreadChannel, std::size_t, std::packaged_task<StatusCode()>&& task) -> StatusCode
        {
            task();

            if (pauseFirstEnqueue.exchange(false, std::memory_order_acq_rel))
            {
                {
                    std::scoped_lock lock(gateMutex);
                    firstReaderDone = true;
                }
                gateCv.notify_one();

                std::unique_lock lock(gateMutex);
                std::ignore = gateCv.wait_for(lock, std::chrono::seconds(2),
                                              [&]
                                              {
                                                  return releaseEnqueue;
                                              });
            }

            return StatusCode::STATUS_OK;
        }));

    StatusCode nextScanStatus = StatusCode::STATUS_ERROR_GENERAL;
    std::thread nextScanThread(
      [&]
      {
          nextScanStatus = scanner->initialize_next_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType));
      });

    {
        std::unique_lock lock(gateMutex);
        const bool entered = gateCv.wait_for(lock, std::chrono::seconds(2),
                                             [&]
                                             {
                                                 return firstReaderDone;
                                             });
        EXPECT_TRUE(entered);
        if (!entered)
        {
            releaseEnqueue = true;
        }
    }

    EXPECT_FALSE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());

    {
        std::scoped_lock lock(gateMutex);
        releaseEnqueue = true;
    }
    gateCv.notify_one();

    nextScanThread.join();

    EXPECT_EQ(StatusCode::STATUS_OK, nextScanStatus);
    EXPECT_TRUE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());
}

TEST_F(MemoryScannerTest, InitializeNextScan_DuringSetup_IsNotComplete)
{
    ON_CALL(*mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(1));

    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    scanner->set_memory_reader(mockReader);

    constexpr std::int32_t expectedValue = 1337;
    ON_CALL(*mockReader, read_memory(_, _, _))
      .WillByDefault(Invoke(
        [expectedValue](std::uint64_t, std::uint64_t size, void* buffer) -> StatusCode
        {
            if (buffer == nullptr || size < sizeof(expectedValue))
            {
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }

            std::memset(buffer, 0, static_cast<std::size_t>(size));
            std::memcpy(buffer, &expectedValue, sizeof(expectedValue));
            return StatusCode::STATUS_OK;
        }));

    ON_CALL(*mockDispatcher, enqueue_on_worker(_, _, _))
      .WillByDefault(Invoke(
        [](Vertex::Thread::ThreadChannel, std::size_t, std::packaged_task<StatusCode()>&& task) -> StatusCode
        {
            task();
            return StatusCode::STATUS_OK;
        }));

    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);
    config.alignmentRequired = true;
    config.alignment = sizeof(expectedValue);
    const auto* valueBytes = reinterpret_cast<const std::uint8_t*>(&expectedValue);
    config.input.assign(valueBytes, valueBytes + sizeof(expectedValue));

    std::vector<Vertex::Scanner::ScanRegion> regions{
        Vertex::Scanner::ScanRegion{.baseAddress = 0x1000, .size = sizeof(expectedValue)}
    };

    EXPECT_EQ(StatusCode::STATUS_OK, scanner->initialize_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType), regions));
    EXPECT_TRUE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());

    std::mutex gateMutex;
    std::condition_variable gateCv;
    bool createPoolEntered = false;
    bool releaseCreatePool = false;
    std::atomic<int> createPoolCalls{0};

    ON_CALL(*mockDispatcher, create_worker_pool(_, _))
      .WillByDefault(Invoke(
        [&](Vertex::Thread::ThreadChannel, std::size_t) -> StatusCode
        {
            const int call = createPoolCalls.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (call == 1)
            {
                {
                    std::scoped_lock lock(gateMutex);
                    createPoolEntered = true;
                }
                gateCv.notify_one();

                std::unique_lock lock(gateMutex);
                std::ignore = gateCv.wait_for(lock, std::chrono::seconds(2),
                                              [&]
                                              {
                                                  return releaseCreatePool;
                                              });
            }
            return StatusCode::STATUS_OK;
        }));

    StatusCode nextScanStatus = StatusCode::STATUS_ERROR_GENERAL;
    std::thread nextScanThread(
      [&]
      {
          nextScanStatus = scanner->initialize_next_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType));
      });

    {
        std::unique_lock lock(gateMutex);
        const bool entered = gateCv.wait_for(lock, std::chrono::seconds(2),
                                             [&]
                                             {
                                                 return createPoolEntered;
                                             });
        EXPECT_TRUE(entered);
        if (!entered)
        {
            releaseCreatePool = true;
        }
    }

    EXPECT_FALSE(scanner->is_scan_complete());

    {
        std::scoped_lock lock(gateMutex);
        releaseCreatePool = true;
    }
    gateCv.notify_one();

    nextScanThread.join();

    EXPECT_EQ(StatusCode::STATUS_OK, nextScanStatus);
    EXPECT_TRUE(scanner->is_scan_complete());
    EXPECT_EQ(1U, scanner->get_results_count());
}

TEST_F(MemoryScannerTest, GetScanResults_UsesReadableRegionCountsDuringInProgressScan)
{
    ON_CALL(*mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(1));

    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    scanner->set_memory_reader(mockReader);

    constexpr std::int32_t expectedValue = 1337;
    ON_CALL(*mockReader, read_memory(_, _, _))
      .WillByDefault(Invoke(
        [expectedValue](std::uint64_t, std::uint64_t size, void* buffer) -> StatusCode
        {
            if (buffer == nullptr || size < sizeof(expectedValue))
            {
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }

            std::memset(buffer, 0, static_cast<std::size_t>(size));
            std::memcpy(buffer, &expectedValue, sizeof(expectedValue));
            return StatusCode::STATUS_OK;
        }));

    std::mutex gateMutex;
    std::condition_variable gateCv;
    bool firstReaderDone = false;
    bool releaseEnqueue = false;
    std::atomic<bool> pauseFirstEnqueue{true};

    ON_CALL(*mockDispatcher, enqueue_on_worker(_, _, _))
      .WillByDefault(Invoke(
        [&](Vertex::Thread::ThreadChannel, std::size_t, std::packaged_task<StatusCode()>&& task) -> StatusCode
        {
            task();

            if (pauseFirstEnqueue.exchange(false, std::memory_order_acq_rel))
            {
                {
                    std::scoped_lock lock(gateMutex);
                    firstReaderDone = true;
                }
                gateCv.notify_one();

                std::unique_lock lock(gateMutex);
                std::ignore = gateCv.wait_for(lock, std::chrono::seconds(2),
                                              [&]
                                              {
                                                  return releaseEnqueue;
                                              });
            }

            return StatusCode::STATUS_OK;
        }));

    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);
    config.alignmentRequired = true;
    config.alignment = sizeof(expectedValue);
    const auto* valueBytes = reinterpret_cast<const std::uint8_t*>(&expectedValue);
    config.input.assign(valueBytes, valueBytes + sizeof(expectedValue));

    std::vector<Vertex::Scanner::ScanRegion> regions{
        Vertex::Scanner::ScanRegion{.baseAddress = 0x1000, .size = sizeof(expectedValue)}
    };

    StatusCode initialScanStatus = StatusCode::STATUS_ERROR_GENERAL;
    std::thread initialScanThread(
      [&]
      {
          initialScanStatus = scanner->initialize_scan(config, Vertex::Scanner::make_builtin_schema(config.valueType), regions);
      });

    {
        std::unique_lock lock(gateMutex);
        const bool entered = gateCv.wait_for(lock, std::chrono::seconds(2),
                                             [&]
                                             {
                                                 return firstReaderDone;
                                             });
        EXPECT_TRUE(entered);
        if (!entered)
        {
            releaseEnqueue = true;
        }
    }

    std::vector<Vertex::Scanner::IMemoryScanner::ScanResultEntry> scanResults;
    const StatusCode getResultsStatus = scanner->get_scan_results(scanResults, 10);
    EXPECT_EQ(StatusCode::STATUS_OK, getResultsStatus);
    EXPECT_EQ(1U, scanResults.size());

    {
        std::scoped_lock lock(gateMutex);
        releaseEnqueue = true;
    }
    gateCv.notify_one();

    initialScanThread.join();

    EXPECT_EQ(StatusCode::STATUS_OK, initialScanStatus);
    EXPECT_TRUE(scanner->is_scan_complete());
}
