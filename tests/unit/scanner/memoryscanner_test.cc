#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../include/vertex/scanner/memoryscanner/memoryscanner.hh"
#include <vertex/scanner/imemoryreader.hh>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILog.hh"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    class MockMemoryReader : public Vertex::Scanner::IMemoryReader
    {
    public:
        MOCK_METHOD(StatusCode, read_memory, (std::uint64_t address, std::uint64_t size, void* buffer), (override));
        MOCK_METHOD(bool, is_valid, (), (const, override));
    };
}

class MemoryScannerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mockSettings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        mockLog = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();

        ON_CALL(*mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(2));
        ON_CALL(*mockSettings, get_int(::testing::HasSubstr("threadBufferSizeMB"), _)).WillByDefault(Return(32));

        scanner = std::make_unique<Vertex::Scanner::MemoryScanner>(*mockSettings, *mockLog);
    }

    void TearDown() override { scanner->stop_scan(); }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> mockLog;
    std::unique_ptr<Vertex::Scanner::MemoryScanner> scanner;
};

// ==================== Memory Reader Tests ====================

TEST_F(MemoryScannerTest, HasMemoryReader_NoReaderSet_ReturnsFalse)
{
    bool result = scanner->has_memory_reader();

    EXPECT_FALSE(result);
}

TEST_F(MemoryScannerTest, HasMemoryReader_ValidReaderSet_ReturnsTrue)
{
    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*mockReader, is_valid()).WillByDefault(Return(true));

    scanner->set_memory_reader(mockReader);
    bool result = scanner->has_memory_reader();

    EXPECT_TRUE(result);
}

TEST_F(MemoryScannerTest, HasMemoryReader_InvalidReaderSet_ReturnsFalse)
{
    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*mockReader, is_valid()).WillByDefault(Return(false));

    scanner->set_memory_reader(mockReader);
    bool result = scanner->has_memory_reader();

    EXPECT_FALSE(result);
}

// ==================== Scan Initialization Tests ====================

TEST_F(MemoryScannerTest, InitializeScan_EmptyMemoryRegions_ReturnsError)
{
    auto mockReader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*mockReader, is_valid()).WillByDefault(Return(true));
    scanner->set_memory_reader(mockReader);

    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);
    std::vector<Vertex::Scanner::ScanRegion> emptyRegions;

    StatusCode result = scanner->initialize_scan(config, emptyRegions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(MemoryScannerTest, InitializeScan_NoMemoryReader_ReturnsError)
{
    Vertex::Scanner::ScanConfiguration config{};
    config.valueType = Vertex::Scanner::ValueType::Int32;
    config.scanMode = static_cast<std::uint8_t>(Vertex::Scanner::NumericScanMode::Exact);

    std::vector<Vertex::Scanner::ScanRegion> regions;
    regions.push_back(Vertex::Scanner::ScanRegion{.baseAddress = 0x1000, .size = 4096});

    StatusCode result = scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}

// ==================== Scan Control Tests ====================

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

// ==================== Progress Tracking Tests ====================

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

// ==================== Undo Tests ====================

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

// ==================== Abort State Tests ====================

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

// ==================== Scan Active State Tests ====================

TEST_F(MemoryScannerTest, IsScanActive_NoScanRunning_ReturnsOK)
{
    StatusCode result = scanner->is_scan_active();

    EXPECT_EQ(StatusCode::STATUS_OK, result);
}
