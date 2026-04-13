#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/scanner/pointerscanner/pointerscanner.hh>
#include <vertex/scanner/imemoryreader.hh>
#include <vertex/thread/threaddispatcher.hh>
#include <vertex/runtime/iloader.hh>
#include <sdk/feature.h>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILog.hh"

#include <cstring>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    class MockMemoryReader : public Vertex::Scanner::IMemoryReader
    {
    public:
        MOCK_METHOD(StatusCode, read_memory, (std::uint64_t address, std::uint64_t size, void* buffer), (override));
    };

    class MockLoader : public Vertex::Runtime::ILoader
    {
    public:
        MOCK_METHOD(StatusCode, load_plugins, (std::filesystem::path& path), (override));
        MOCK_METHOD(StatusCode, load_plugin, (std::filesystem::path path), (override));
        MOCK_METHOD(StatusCode, unload_plugin, (std::size_t pluginIndex), (override));
        MOCK_METHOD(StatusCode, resolve_functions, (Vertex::Runtime::Plugin& plugin), (override));
        MOCK_METHOD(StatusCode, set_active_plugin, (Vertex::Runtime::Plugin& plugin), (override));
        MOCK_METHOD(StatusCode, set_active_plugin, (std::size_t index), (override));
        MOCK_METHOD(StatusCode, set_active_plugin, (const std::filesystem::path& path), (override));
        MOCK_METHOD(StatusCode, has_plugin_loaded, (), (override));
        MOCK_METHOD(StatusCode, get_plugins_from_fs, (const std::vector<std::filesystem::path>& paths, std::vector<Vertex::Runtime::Plugin>& pluginStates), (override));
        MOCK_METHOD(const std::vector<Vertex::Runtime::Plugin>&, get_plugins, (), (noexcept, override));
        MOCK_METHOD(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>, get_active_plugin, (), (override));
        MOCK_METHOD(Vertex::Runtime::IRegistry&, get_registry, (), (override));
        MOCK_METHOD(const Vertex::Runtime::IRegistry&, get_registry, (), (const, override));
        MOCK_METHOD(Vertex::Runtime::IUIRegistry&, get_ui_registry, (), (override));
        MOCK_METHOD(const Vertex::Runtime::IUIRegistry&, get_ui_registry, (), (const, override));
        MOCK_METHOD(StatusCode, dispatch_event, (VertexEvent event, const void* data), (override));
    };

    struct PointerLayout final
    {
        std::uint64_t address{};
        std::uint64_t value{};
    };

    extern "C" StatusCode test_get_process_pointer_size(std::uint64_t* size)
    {
        *size = 8;
        return StatusCode::STATUS_OK;
    }

    class FakeMemoryReader final : public Vertex::Scanner::IMemoryReader
    {
    public:
        explicit FakeMemoryReader(std::vector<PointerLayout> layout, const std::uint64_t ptrSize = 8)
            : m_layout{std::move(layout)}, m_ptrSize{ptrSize}
        {
        }

        StatusCode read_memory(const std::uint64_t address, const std::uint64_t size, void* buffer) override
        {
            std::memset(buffer, 0, size);

            for (const auto& [ptrAddr, ptrVal] : m_layout)
            {
                if (ptrAddr >= address && ptrAddr + m_ptrSize <= address + size)
                {
                    const auto offset = ptrAddr - address;
                    std::memcpy(static_cast<char*>(buffer) + offset, &ptrVal, m_ptrSize);
                }
            }

            return StatusCode::STATUS_OK;
        }

    private:
        std::vector<PointerLayout> m_layout;
        std::uint64_t m_ptrSize;
    };
} // namespace

class PointerScannerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_mockSettings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        m_mockLog = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        m_mockLoader = std::make_unique<NiceMock<MockLoader>>();
        m_dispatcher = std::make_unique<Vertex::Thread::ThreadDispatcher>();

        ON_CALL(*m_mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(2));
        ON_CALL(*m_mockSettings, get_int(::testing::HasSubstr("threadBufferSizeMB"), _)).WillByDefault(Return(1));

        setup_mock_loader_with_plugin();

        ASSERT_EQ(StatusCode::STATUS_OK, m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_STANDARD));
        ASSERT_EQ(StatusCode::STATUS_OK, m_dispatcher->start());

        m_scanner = std::make_unique<Vertex::Scanner::PointerScanner>(*m_mockSettings, *m_mockLog, *m_dispatcher, *m_mockLoader);
    }

    void TearDown() override
    {
        m_scanner->stop_scan();
        m_scanner->finalize_scan();
        m_scanner.reset();
        static_cast<void>(m_dispatcher->stop());
    }

    std::vector<Vertex::Scanner::ScanRegion> make_regions(
        const std::uint64_t base, const std::uint64_t size, const std::string& moduleName = "test.so")
    {
        return {{.moduleName = moduleName, .baseAddress = base, .size = size}};
    }

    void setup_mock_loader_with_plugin()
    {
        m_testPlugin = std::make_unique<Vertex::Runtime::Plugin>(*m_mockLog);
        m_testPlugin->internal_vertex_memory_get_process_pointer_size = test_get_process_pointer_size;

        ON_CALL(*m_mockLoader, get_active_plugin())
            .WillByDefault(::testing::Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(*m_testPlugin)));
    }

    void run_scan_to_completion(const Vertex::Scanner::PointerScanConfig& config,
                                const std::vector<Vertex::Scanner::ScanRegion>& regions)
    {
        const StatusCode status = m_scanner->initialize_scan(config, regions);
        ASSERT_EQ(StatusCode::STATUS_OK, status);
        m_scanner->wait_for_scan_completion();
    }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> m_mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_mockLog;
    std::unique_ptr<NiceMock<MockLoader>> m_mockLoader;
    std::unique_ptr<Vertex::Runtime::Plugin> m_testPlugin;
    std::unique_ptr<Vertex::Thread::ThreadDispatcher> m_dispatcher;
    std::unique_ptr<Vertex::Scanner::PointerScanner> m_scanner;
};

// ==================== Memory Reader Tests ====================

TEST_F(PointerScannerTest, HasMemoryReader_NoReaderSet_ReturnsFalse)
{
    EXPECT_FALSE(m_scanner->has_memory_reader());
}

TEST_F(PointerScannerTest, HasMemoryReader_ReaderSet_ReturnsTrue)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    EXPECT_TRUE(m_scanner->has_memory_reader());
}

// ==================== Config Validation Tests ====================

TEST_F(PointerScannerTest, InitializeScan_EmptyRegions_ReturnsError)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    Vertex::Scanner::PointerScanConfig config{.targetAddress = 0x1000};
    const StatusCode result = m_scanner->initialize_scan(config, {});

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(PointerScannerTest, InitializeScan_NoMemoryReader_ReturnsError)
{
    Vertex::Scanner::PointerScanConfig config{.targetAddress = 0x1000};
    auto regions = make_regions(0x1000, 4096);

    const StatusCode result = m_scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}

TEST_F(PointerScannerTest, InitializeScan_ZeroTargetAddress_ReturnsError)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    Vertex::Scanner::PointerScanConfig config{.targetAddress = 0};
    auto regions = make_regions(0x1000, 4096);

    const StatusCode result = m_scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(PointerScannerTest, InitializeScan_ZeroAlignment_ReturnsError)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    Vertex::Scanner::PointerScanConfig config{.targetAddress = 0x1000, .alignment = 0};
    auto regions = make_regions(0x1000, 4096);

    const StatusCode result = m_scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(PointerScannerTest, InitializeScan_ZeroWorkerChunkSize_ReturnsError)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    Vertex::Scanner::PointerScanConfig config{.targetAddress = 0x1000, .workerChunkSize = 0};
    auto regions = make_regions(0x1000, 4096);

    const StatusCode result = m_scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(PointerScannerTest, InitializeScan_ZeroMaxDepth_ReturnsError)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    Vertex::Scanner::PointerScanConfig config{.targetAddress = 0x1000, .maxDepth = 0};
    auto regions = make_regions(0x1000, 4096);

    const StatusCode result = m_scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(PointerScannerTest, InitializeScan_MaxOffsetExceedsInt32_ReturnsError)
{
    m_scanner->set_memory_reader(std::make_shared<NiceMock<MockMemoryReader>>());

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = 0x1000,
        .maxOffset = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1};
    auto regions = make_regions(0x1000, 4096);

    const StatusCode result = m_scanner->initialize_scan(config, regions);

    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

// ==================== Lifecycle Tests ====================

TEST_F(PointerScannerTest, IsScanComplete_NoScanStarted_ReturnsTrue)
{
    EXPECT_TRUE(m_scanner->is_scan_complete());
}

TEST_F(PointerScannerTest, StopScan_NoScanRunning_ReturnsOK)
{
    EXPECT_EQ(StatusCode::STATUS_OK, m_scanner->stop_scan());
}

TEST_F(PointerScannerTest, GetProgress_Idle_ReturnsIdlePhase)
{
    const auto progress = m_scanner->get_progress();

    EXPECT_EQ(Vertex::Scanner::PointerScanPhase::Idle, progress.phase);
}

TEST_F(PointerScannerTest, GetNodeCount_NoScan_ReturnsZero)
{
    EXPECT_EQ(0, m_scanner->get_node_count());
}

TEST_F(PointerScannerTest, GetEdgeCount_NoScan_ReturnsZero)
{
    EXPECT_EQ(0, m_scanner->get_edge_count());
}

TEST_F(PointerScannerTest, GetRootsRange_NoScanRun_ReturnsOK)
{
    std::vector<Vertex::Scanner::PointerNodeRecord> roots;
    const StatusCode result = m_scanner->get_roots_range(roots, 0, 10);

    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

// ==================== Algorithm: Empty Index ====================

TEST_F(PointerScannerTest, Scan_EmptyMemory_CompletesWithNoResults)
{
    auto reader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*reader, read_memory(_, _, _)).WillByDefault(Return(StatusCode::STATUS_OK));
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = 0x50000,
        .maxDepth = 3,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 4096);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(0, m_scanner->get_node_count());
    EXPECT_EQ(0, m_scanner->get_edge_count());
}

// ==================== Algorithm: Simple Pointer Chain ====================

TEST_F(PointerScannerTest, Scan_SinglePointerToTarget_FindsOneEdge)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, TARGET}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(2, m_scanner->get_node_count());
    EXPECT_GE(m_scanner->get_edge_count(), 1);
}

TEST_F(PointerScannerTest, Scan_TwoDepthChain_FindsBothLevels)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t MID = 0x18000;
    constexpr std::uint64_t ROOT_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {MID, TARGET},
        {ROOT_ADDR, MID}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 2,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(3, m_scanner->get_node_count());
    EXPECT_GE(m_scanner->get_edge_count(), 2);
}

// ==================== Algorithm: Depth Bounds ====================

TEST_F(PointerScannerTest, Scan_ChainExceedsMaxDepth_StopsAtMaxDepth)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t MID = 0x18000;
    constexpr std::uint64_t ROOT_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {MID, TARGET},
        {ROOT_ADDR, MID}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_EQ(2, m_scanner->get_node_count());
}

// ==================== Algorithm: Offset Semantics ====================

TEST_F(PointerScannerTest, Scan_PointerWithPositiveOffset_FindsEdge)
{
    constexpr std::uint64_t TARGET = 0x20100;
    constexpr std::uint64_t PTR_ADDR = 0x10100;
    constexpr std::uint64_t PTR_VALUE = 0x20000;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, PTR_VALUE}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x200,
        .alignment = 8,
        .allowNegativeOffsets = false};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_EQ(2, m_scanner->get_node_count());
    EXPECT_GE(m_scanner->get_edge_count(), 1);
}

TEST_F(PointerScannerTest, Scan_NegativeOffsetDisallowed_SkipsNegativeOffset)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;
    constexpr std::uint64_t PTR_VALUE = 0x20100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, PTR_VALUE}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x200,
        .alignment = 8,
        .allowNegativeOffsets = false};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_EQ(1, m_scanner->get_node_count());
    EXPECT_EQ(0, m_scanner->get_edge_count());
}

TEST_F(PointerScannerTest, Scan_NegativeOffsetAllowed_FindsNegativeOffset)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;
    constexpr std::uint64_t PTR_VALUE = 0x20100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, PTR_VALUE}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x200,
        .alignment = 8,
        .allowNegativeOffsets = true};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_EQ(2, m_scanner->get_node_count());
    EXPECT_GE(m_scanner->get_edge_count(), 1);
}

// ==================== Algorithm: Offset Out of Range ====================

TEST_F(PointerScannerTest, Scan_PointerBeyondMaxOffset_NotFound)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;
    constexpr std::uint64_t PTR_VALUE = 0x1E000;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, PTR_VALUE}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x100,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_EQ(1, m_scanner->get_node_count());
    EXPECT_EQ(0, m_scanner->get_edge_count());
}

// ==================== Algorithm: Cycle Handling ====================

TEST_F(PointerScannerTest, Scan_CyclicPointers_DoesNotHang)
{
    constexpr std::uint64_t A = 0x10100;
    constexpr std::uint64_t B = 0x10200;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {A, B},
        {B, A}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = A,
        .maxDepth = 5,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x1000);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_LE(m_scanner->get_node_count(), 3);
}

// ==================== Algorithm: Static Root Filtering ====================

TEST_F(PointerScannerTest, Scan_StaticRootsOnly_FiltersNonModuleRoots)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, TARGET}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8,
        .staticRootsOnly = true};

    std::vector<Vertex::Scanner::ScanRegion> regions{
        {.moduleName = "test.so", .baseAddress = 0x10000, .size = 0x10000},
        {.moduleName = "", .baseAddress = 0x20000, .size = 0x10000}};

    run_scan_to_completion(config, regions);

    std::vector<Vertex::Scanner::PointerNodeRecord> roots;
    ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_roots_range(roots, 0, 100));

    constexpr std::uint32_t NO_MODULE_ID = std::numeric_limits<std::uint32_t>::max();
    for (const auto& root : roots)
    {
        EXPECT_NE(NO_MODULE_ID, root.moduleId);
    }
}

TEST_F(PointerScannerTest, Scan_StaticRootsDisabled_IncludesNonModuleRoots)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x30100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, TARGET}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8,
        .staticRootsOnly = false};

    std::vector<Vertex::Scanner::ScanRegion> regions{
        {.moduleName = "", .baseAddress = 0x20000, .size = 0x10000},
        {.moduleName = "", .baseAddress = 0x30000, .size = 0x10000}};

    run_scan_to_completion(config, regions);

    std::vector<Vertex::Scanner::PointerNodeRecord> roots;
    ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_roots_range(roots, 0, 100));

    EXPECT_GE(roots.size(), 1);
}

// ==================== Algorithm: Max Nodes Cap ====================

TEST_F(PointerScannerTest, Scan_MaxNodesCap_ReturnsCapExceededStatus)
{
    std::vector<PointerLayout> layout;
    constexpr std::uint64_t TARGET = 0x20000;
    for (std::uint64_t i{}; i < 20; ++i)
    {
        layout.push_back({0x10000 + i * 8, TARGET});
    }

    auto reader = std::make_shared<FakeMemoryReader>(layout);
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 2,
        .maxOffset = 0x1000,
        .alignment = 8,
        .maxNodes = 5};
    auto regions = make_regions(0x10000, 0x20000);

    const StatusCode status = m_scanner->initialize_scan(config, regions);
    ASSERT_EQ(StatusCode::STATUS_OK, status);
    m_scanner->wait_for_scan_completion();

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_NODES_EXCEEDED, m_scanner->get_scan_status());
    EXPECT_EQ(config.maxNodes, m_scanner->get_node_count());

    for (std::uint32_t nodeId{}; nodeId < static_cast<std::uint32_t>(m_scanner->get_node_count()); ++nodeId)
    {
        Vertex::Scanner::PointerNodeRecord node{};
        ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_node(nodeId, node));
        EXPECT_NE(0ULL, node.address);
    }
}

// ==================== Algorithm: Max Edges Cap ====================

TEST_F(PointerScannerTest, Scan_MaxEdgesCap_ReturnsCapExceededStatus)
{
    std::vector<PointerLayout> layout;
    constexpr std::uint64_t TARGET = 0x20000;
    for (std::uint64_t i{}; i < 20; ++i)
    {
        layout.push_back({0x10000 + i * 8, TARGET});
    }

    auto reader = std::make_shared<FakeMemoryReader>(layout);
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 2,
        .maxOffset = 0x1000,
        .alignment = 8,
        .maxEdges = 3};
    auto regions = make_regions(0x10000, 0x20000);

    const StatusCode status = m_scanner->initialize_scan(config, regions);
    ASSERT_EQ(StatusCode::STATUS_OK, status);
    m_scanner->wait_for_scan_completion();

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_EDGES_EXCEEDED, m_scanner->get_scan_status());
    EXPECT_GE(m_scanner->get_edge_count(), 3);
}

// ==================== Query API Tests ====================

TEST_F(PointerScannerTest, GetNode_ValidNodeId_ReturnsNode)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, TARGET}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    Vertex::Scanner::PointerNodeRecord node{};
    const StatusCode result = m_scanner->get_node(0, node);

    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_EQ(TARGET, node.address);
}

TEST_F(PointerScannerTest, GetNode_OutOfBounds_ReturnsError)
{
    auto reader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*reader, read_memory(_, _, _)).WillByDefault(Return(StatusCode::STATUS_OK));
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = 0x50000,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 4096);

    run_scan_to_completion(config, regions);

    Vertex::Scanner::PointerNodeRecord node{};
    const StatusCode result = m_scanner->get_node(9999, node);

    EXPECT_EQ(StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS, result);
}

TEST_F(PointerScannerTest, GetChildren_NoChildren_ReturnsEmptyVector)
{
    auto reader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*reader, read_memory(_, _, _)).WillByDefault(Return(StatusCode::STATUS_OK));
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = 0x50000,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 4096);

    run_scan_to_completion(config, regions);

    std::vector<Vertex::Scanner::PointerEdgeRecord> edges;
    const StatusCode result = m_scanner->get_children(0, edges, 0, 100);

    EXPECT_EQ(StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS, result);
}

TEST_F(PointerScannerTest, GetChildren_TwoDepthChain_ReturnsCorrectEdges)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t MID = 0x18000;
    constexpr std::uint64_t ROOT_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {MID, TARGET},
        {ROOT_ADDR, MID}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 2,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    ASSERT_EQ(3, m_scanner->get_node_count());

    Vertex::Scanner::PointerNodeRecord targetNode{};
    ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_node(0, targetNode));
    EXPECT_EQ(TARGET, targetNode.address);

    std::vector<Vertex::Scanner::PointerNodeRecord> roots;
    ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_roots_range(roots, 0, 100));
    ASSERT_FALSE(roots.empty());

    std::uint32_t midNodeId{};
    bool foundMid{};
    for (std::uint32_t i = 1; i < m_scanner->get_node_count(); ++i)
    {
        Vertex::Scanner::PointerNodeRecord node{};
        ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_node(i, node));
        if (node.address == MID)
        {
            midNodeId = i;
            foundMid = true;
        }
    }
    ASSERT_TRUE(foundMid);

    std::vector<Vertex::Scanner::PointerEdgeRecord> midEdges;
    ASSERT_EQ(StatusCode::STATUS_OK, m_scanner->get_children(midNodeId, midEdges, 0, 100));
    ASSERT_EQ(1, midEdges.size());
    EXPECT_EQ(midNodeId, midEdges[0].parentNodeId);
    EXPECT_EQ(0, midEdges[0].childNodeId);
}

// ==================== Module Table Tests ====================

TEST_F(PointerScannerTest, GetModules_MultipleModuleRegions_BuildsCorrectTable)
{
    auto reader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*reader, read_memory(_, _, _)).WillByDefault(Return(StatusCode::STATUS_OK));
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = 0x50000,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};

    std::vector<Vertex::Scanner::ScanRegion> regions{
        {.moduleName = "libA.so", .baseAddress = 0x10000, .size = 0x5000},
        {.moduleName = "libA.so", .baseAddress = 0x20000, .size = 0x5000},
        {.moduleName = "libB.so", .baseAddress = 0x30000, .size = 0x5000},
        {.moduleName = "", .baseAddress = 0x40000, .size = 0x10000}};

    run_scan_to_completion(config, regions);

    const auto& modules = m_scanner->get_modules();
    EXPECT_EQ(2, modules.size());

    bool foundA{};
    for (const auto& mod : modules)
    {
        if (mod.moduleName == "libA.so")
        {
            EXPECT_EQ(0x10000, mod.moduleBase);
            EXPECT_EQ(0x15000, mod.moduleSpan);
            foundA = true;
        }
    }
    EXPECT_TRUE(foundA);
}

// ==================== Cancellation Tests ====================

TEST_F(PointerScannerTest, StopScan_DuringScan_AbortsScan)
{
    std::vector<PointerLayout> layout;
    constexpr std::uint64_t TARGET = 0x200000;
    for (std::uint64_t i{}; i < 1000; ++i)
    {
        layout.push_back({0x100000 + i * 8, TARGET + (i % 100) * 8});
    }

    auto reader = std::make_shared<FakeMemoryReader>(layout);
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 10,
        .maxOffset = 0x10000,
        .alignment = 8};
    auto regions = make_regions(0x100000, 0x200000);

    m_scanner->initialize_scan(config, regions);
    m_scanner->stop_scan();
    m_scanner->wait_for_scan_completion();

    EXPECT_TRUE(m_scanner->is_scan_complete());
}

// ==================== Concurrency: Deterministic Results ====================

TEST_F(PointerScannerTest, Scan_SameInputTwice_ProducesSameNodeCount)
{
    constexpr std::uint64_t TARGET = 0x20000;

    std::vector<PointerLayout> layout{
        {0x10100, TARGET},
        {0x10200, TARGET},
        {0x10300, TARGET}};

    auto reader1 = std::make_shared<FakeMemoryReader>(layout);
    m_scanner->set_memory_reader(reader1);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    const auto firstNodeCount = m_scanner->get_node_count();
    const auto firstEdgeCount = m_scanner->get_edge_count();

    auto reader2 = std::make_shared<FakeMemoryReader>(layout);
    m_scanner->set_memory_reader(reader2);

    run_scan_to_completion(config, regions);

    EXPECT_EQ(firstNodeCount, m_scanner->get_node_count());
    EXPECT_EQ(firstEdgeCount, m_scanner->get_edge_count());
}

// ==================== Single-Threaded Mode Regression Tests ====================

class PointerScannerSingleThreadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_mockSettings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        m_mockLog = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        m_mockLoader = std::make_unique<NiceMock<MockLoader>>();
        m_dispatcher = std::make_unique<Vertex::Thread::ThreadDispatcher>();

        ON_CALL(*m_mockSettings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(1));
        ON_CALL(*m_mockSettings, get_int(::testing::HasSubstr("threadBufferSizeMB"), _)).WillByDefault(Return(1));

        setup_mock_loader_with_plugin();

        ASSERT_EQ(StatusCode::STATUS_OK, m_dispatcher->configure(VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED));
        ASSERT_EQ(StatusCode::STATUS_OK, m_dispatcher->start());

        m_scanner = std::make_unique<Vertex::Scanner::PointerScanner>(*m_mockSettings, *m_mockLog, *m_dispatcher, *m_mockLoader);
    }

    void TearDown() override
    {
        m_scanner->stop_scan();
        m_scanner->finalize_scan();
        m_scanner.reset();
        static_cast<void>(m_dispatcher->stop());
    }

    std::vector<Vertex::Scanner::ScanRegion> make_regions(
        const std::uint64_t base, const std::uint64_t size, const std::string& moduleName = "test.so")
    {
        return {{.moduleName = moduleName, .baseAddress = base, .size = size}};
    }

    void setup_mock_loader_with_plugin()
    {
        m_testPlugin = std::make_unique<Vertex::Runtime::Plugin>(*m_mockLog);
        m_testPlugin->internal_vertex_memory_get_process_pointer_size = test_get_process_pointer_size;

        ON_CALL(*m_mockLoader, get_active_plugin())
            .WillByDefault(::testing::Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(*m_testPlugin)));
    }

    void run_scan_to_completion(const Vertex::Scanner::PointerScanConfig& config,
                                const std::vector<Vertex::Scanner::ScanRegion>& regions)
    {
        const StatusCode status = m_scanner->initialize_scan(config, regions);
        ASSERT_EQ(StatusCode::STATUS_OK, status);
        m_scanner->wait_for_scan_completion();
    }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> m_mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_mockLog;
    std::unique_ptr<NiceMock<MockLoader>> m_mockLoader;
    std::unique_ptr<Vertex::Runtime::Plugin> m_testPlugin;
    std::unique_ptr<Vertex::Thread::ThreadDispatcher> m_dispatcher;
    std::unique_ptr<Vertex::Scanner::PointerScanner> m_scanner;
};

TEST_F(PointerScannerSingleThreadTest, Scan_EmptyMemory_CompletesWithoutDeadlock)
{
    auto reader = std::make_shared<NiceMock<MockMemoryReader>>();
    ON_CALL(*reader, read_memory(_, _, _)).WillByDefault(Return(StatusCode::STATUS_OK));
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = 0x50000,
        .maxDepth = 3,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 4096);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_OK, m_scanner->get_scan_status());
    EXPECT_EQ(0, m_scanner->get_node_count());
}

TEST_F(PointerScannerSingleThreadTest, Scan_SimpleChain_FindsPointerCorrectly)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR_ADDR, TARGET}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 1,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_OK, m_scanner->get_scan_status());
    EXPECT_EQ(2, m_scanner->get_node_count());
    EXPECT_GE(m_scanner->get_edge_count(), 1);
}

TEST_F(PointerScannerSingleThreadTest, Scan_TwoDepthChain_CompletesSuccessfully)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t MID = 0x18000;
    constexpr std::uint64_t ROOT_ADDR = 0x10100;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {MID, TARGET},
        {ROOT_ADDR, MID}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 2,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_OK, m_scanner->get_scan_status());
    EXPECT_EQ(3, m_scanner->get_node_count());
    EXPECT_GE(m_scanner->get_edge_count(), 2);
}

TEST_F(PointerScannerSingleThreadTest, Scan_CyclicPointers_DoesNotHang)
{
    constexpr std::uint64_t A = 0x10100;
    constexpr std::uint64_t B = 0x10200;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {A, B},
        {B, A}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = A,
        .maxDepth = 5,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x1000);

    run_scan_to_completion(config, regions);

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_OK, m_scanner->get_scan_status());
    EXPECT_LE(m_scanner->get_node_count(), 3);
}

TEST_F(PointerScannerSingleThreadTest, Scan_SmallMemory_CompleteWithinLimits)
{
    constexpr std::uint64_t TARGET = 0x20000;
    constexpr std::uint64_t PTR1_ADDR = 0x1f100;
    constexpr std::uint64_t PTR2_ADDR = 0x1f200;

    auto reader = std::make_shared<FakeMemoryReader>(std::vector<PointerLayout>{
        {PTR1_ADDR, TARGET},
        {PTR2_ADDR, TARGET}});
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 2,
        .maxOffset = 0x1000,
        .alignment = 8};
    auto regions = make_regions(0x10000, 0x20000);

    const StatusCode status = m_scanner->initialize_scan(config, regions);
    ASSERT_EQ(StatusCode::STATUS_OK, status);
    m_scanner->wait_for_scan_completion();

    EXPECT_TRUE(m_scanner->is_scan_complete());
    EXPECT_EQ(StatusCode::STATUS_OK, m_scanner->get_scan_status());
}

TEST_F(PointerScannerSingleThreadTest, Scan_StopDuringScan_AbortsScan)
{
    std::vector<PointerLayout> layout;
    constexpr std::uint64_t TARGET = 0x200000;
    for (std::uint64_t i{}; i < 1000; ++i)
    {
        layout.push_back({0x100000 + i * 8, TARGET + (i % 100) * 8});
    }

    auto reader = std::make_shared<FakeMemoryReader>(layout);
    m_scanner->set_memory_reader(reader);

    Vertex::Scanner::PointerScanConfig config{
        .targetAddress = TARGET,
        .maxDepth = 10,
        .maxOffset = 0x10000,
        .alignment = 8};
    auto regions = make_regions(0x100000, 0x200000);

    m_scanner->initialize_scan(config, regions);
    m_scanner->stop_scan();
    m_scanner->wait_for_scan_completion();

    EXPECT_TRUE(m_scanner->is_scan_complete());
}
