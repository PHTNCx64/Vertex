#include <gtest/gtest.h>

#include <vertex/scanner/pointerscanner/ipointerscanner.hh>
#include <vertex/scanner/pointerscanner/pointerscancomparator.hh>
#include <vertex/scanner/pointerscanner/pointerscanfileformat.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    using namespace Vertex::Scanner;

    class FailingNodeScanner final : public IPointerScanner
    {
    public:
        FailingNodeScanner()
        {
            m_config = PointerScanConfig{
                .targetAddress = 0x5000,
                .maxDepth = 2,
                .maxOffset = 0x1000,
                .alignment = 8,
                .maxParentsPerNode = 64,
                .maxNodes = 1024,
                .maxEdges = 1024,
                .workerChunkSize = 8192};

            m_modules.push_back(ModuleRecord{
                .moduleId = 0,
                .moduleBase = 0x4000,
                .moduleSpan = 0x2000,
                .moduleName = "testmod"});
        }

        void set_memory_reader(std::shared_ptr<IMemoryReader> /*reader*/) override {}
        [[nodiscard]] bool has_memory_reader() const override { return true; }

        StatusCode initialize_scan(const PointerScanConfig& /*config*/, const std::vector<ScanRegion>& /*memoryRegions*/) override
        {
            return StatusCode::STATUS_OK;
        }
        StatusCode initialize_rescan() override { return StatusCode::STATUS_OK; }
        StatusCode stop_scan() override { return StatusCode::STATUS_OK; }
        void finalize_scan() override {}

        [[nodiscard]] bool is_scan_complete() override { return true; }
        [[nodiscard]] StatusCode get_scan_status() const noexcept override { return StatusCode::STATUS_OK; }
        [[nodiscard]] PointerScanProgress get_progress() const noexcept override { return {}; }

        [[nodiscard]] std::uint64_t get_node_count() const noexcept override { return 2; }
        [[nodiscard]] std::uint64_t get_edge_count() const noexcept override { return 0; }

        [[nodiscard]] std::uint64_t get_root_count() const noexcept override { return 1; }
        [[nodiscard]] StatusCode get_roots_range(std::vector<PointerNodeRecord>& roots, std::size_t startIndex, std::size_t count) const override
        {
            roots.clear();
            if (startIndex > 0 || count == 0)
            {
                return StatusCode::STATUS_OK;
            }

            roots.push_back(PointerNodeRecord{
                .address = 0x6000,
                .moduleId = 0,
                .minDepthDiscovered = 1,
                .flags = 0});
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode get_root_node_ids(std::vector<std::uint32_t>& nodeIds, std::size_t startIndex, std::size_t count) const override
        {
            nodeIds.clear();
            if (startIndex > 0 || count == 0)
            {
                return StatusCode::STATUS_OK;
            }

            nodeIds.push_back(1);
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode get_children(std::uint32_t /*nodeId*/, std::vector<PointerEdgeRecord>& edges, std::size_t /*startIndex*/, std::size_t /*count*/) const override
        {
            edges.clear();
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode get_node(std::uint32_t nodeId, PointerNodeRecord& node) const override
        {
            ++m_getNodeCalls;

            if (nodeId == 0)
            {
                node = PointerNodeRecord{
                    .address = 0x5000,
                    .moduleId = 0,
                    .minDepthDiscovered = 0,
                    .flags = 0};
                return StatusCode::STATUS_OK;
            }

            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }
        [[nodiscard]] const std::vector<ModuleRecord>& get_modules() const noexcept override { return m_modules; }

        [[nodiscard]] StatusCode get_nodes_range(std::vector<PointerNodeRecord>& nodes, std::size_t startIndex, std::size_t count) const override
        {
            nodes.clear();
            if (count == 0)
            {
                return StatusCode::STATUS_OK;
            }
            const auto totalNodes = get_node_count();
            if (startIndex >= totalNodes)
            {
                return StatusCode::STATUS_OK;
            }
            const auto available = std::min(count, static_cast<std::size_t>(totalNodes - startIndex));
            nodes.resize(available);
            for (std::size_t i{}; i < available; ++i)
            {
                const auto status = get_node(static_cast<std::uint32_t>(startIndex + i), nodes[i]);
                if (status != StatusCode::STATUS_OK)
                {
                    nodes.clear();
                    return status;
                }
            }
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode get_edges_range(std::vector<PointerEdgeRecord>& edges, std::size_t /*startIndex*/, std::size_t /*count*/) const override
        {
            edges.clear();
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode get_parent_ranges(std::vector<ParentEdgeRange>& ranges, std::size_t /*startIndex*/, std::size_t /*count*/) const override
        {
            ranges.clear();
            return StatusCode::STATUS_OK;
        }

        [[nodiscard]] StatusCode save_graph(const std::filesystem::path& /*filePath*/) const override
        {
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode load_graph(const std::filesystem::path& /*filePath*/) override
        {
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] const PointerScanConfig& get_config() const noexcept override { return m_config; }

        [[nodiscard]] std::size_t get_node_call_count() const noexcept
        {
            return m_getNodeCalls;
        }

    private:
        PointerScanConfig m_config{};
        std::vector<ModuleRecord> m_modules{};
        mutable std::size_t m_getNodeCalls{};
    };

    class BulkRangeScanner final : public IPointerScanner
    {
    public:
        BulkRangeScanner()
        {
            m_config = PointerScanConfig{
                .targetAddress = 0x5000,
                .maxDepth = 3,
                .maxOffset = 0x1000,
                .alignment = 8,
                .maxParentsPerNode = 64,
                .maxNodes = 1024,
                .maxEdges = 1024,
                .workerChunkSize = 8192};

            m_modules.push_back(ModuleRecord{
                .moduleId = 0,
                .moduleBase = 0x4000,
                .moduleSpan = 0x2000,
                .moduleName = "testmod"});

            m_nodes = {
                {.address = 0x5000, .moduleId = 0, .minDepthDiscovered = 0, .flags = 0},
                {.address = 0x6000, .moduleId = 0, .minDepthDiscovered = 1, .flags = 0}};

            m_edges = {
                {.parentNodeId = 1, .childNodeId = 0, .offset = 0x10, .flags = 0}};

            m_parentRanges = {
                {.firstEdgeIndex = 0, .count = 0},
                {.firstEdgeIndex = 0, .count = 1}};

            m_rootNodeIds = {1};
        }

        void set_memory_reader(std::shared_ptr<IMemoryReader> /*reader*/) override {}
        [[nodiscard]] bool has_memory_reader() const override { return true; }

        StatusCode initialize_scan(const PointerScanConfig& /*config*/, const std::vector<ScanRegion>& /*memoryRegions*/) override
        {
            return StatusCode::STATUS_OK;
        }
        StatusCode initialize_rescan() override { return StatusCode::STATUS_OK; }
        StatusCode stop_scan() override { return StatusCode::STATUS_OK; }
        void finalize_scan() override {}

        [[nodiscard]] bool is_scan_complete() override { return true; }
        [[nodiscard]] StatusCode get_scan_status() const noexcept override { return StatusCode::STATUS_OK; }
        [[nodiscard]] PointerScanProgress get_progress() const noexcept override { return {}; }

        [[nodiscard]] std::uint64_t get_node_count() const noexcept override { return m_nodes.size(); }
        [[nodiscard]] std::uint64_t get_edge_count() const noexcept override { return m_edges.size(); }

        [[nodiscard]] std::uint64_t get_root_count() const noexcept override { return m_rootNodeIds.size(); }
        [[nodiscard]] StatusCode get_roots_range(std::vector<PointerNodeRecord>& roots, std::size_t startIndex, std::size_t count) const override
        {
            roots.clear();
            if (startIndex >= m_rootNodeIds.size() || count == 0)
            {
                return StatusCode::STATUS_OK;
            }

            const auto available = std::min(count, m_rootNodeIds.size() - startIndex);
            roots.reserve(available);
            for (std::size_t i{}; i < available; ++i)
            {
                roots.push_back(m_nodes[m_rootNodeIds[startIndex + i]]);
            }
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode get_root_node_ids(std::vector<std::uint32_t>& nodeIds, std::size_t startIndex, std::size_t count) const override
        {
            return copy_range(m_rootNodeIds, nodeIds, startIndex, count);
        }
        [[nodiscard]] StatusCode get_children(std::uint32_t /*nodeId*/, std::vector<PointerEdgeRecord>& edges, std::size_t /*startIndex*/, std::size_t /*count*/) const override
        {
            ++m_getChildrenCalls;
            edges.clear();
            return StatusCode::STATUS_ERROR_GENERAL;
        }
        [[nodiscard]] StatusCode get_node(std::uint32_t /*nodeId*/, PointerNodeRecord& /*node*/) const override
        {
            ++m_getNodeCalls;
            return StatusCode::STATUS_ERROR_GENERAL;
        }
        [[nodiscard]] const std::vector<ModuleRecord>& get_modules() const noexcept override { return m_modules; }

        [[nodiscard]] StatusCode get_nodes_range(
            std::vector<PointerNodeRecord>& nodes,
            std::size_t startIndex,
            std::size_t count) const override
        {
            ++m_getNodesRangeCalls;
            return copy_range(m_nodes, nodes, startIndex, count);
        }

        [[nodiscard]] StatusCode get_edges_range(
            std::vector<PointerEdgeRecord>& edges,
            std::size_t startIndex,
            std::size_t count) const override
        {
            ++m_getEdgesRangeCalls;
            return copy_range(m_edges, edges, startIndex, count);
        }

        [[nodiscard]] StatusCode get_parent_ranges(
            std::vector<ParentEdgeRange>& ranges,
            std::size_t startIndex,
            std::size_t count) const override
        {
            ++m_getParentRangesCalls;
            return copy_range(m_parentRanges, ranges, startIndex, count);
        }

        [[nodiscard]] StatusCode save_graph(const std::filesystem::path& /*filePath*/) const override
        {
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] StatusCode load_graph(const std::filesystem::path& /*filePath*/) override
        {
            return StatusCode::STATUS_OK;
        }
        [[nodiscard]] const PointerScanConfig& get_config() const noexcept override { return m_config; }

        [[nodiscard]] std::size_t get_node_call_count() const noexcept { return m_getNodeCalls; }
        [[nodiscard]] std::size_t get_children_call_count() const noexcept { return m_getChildrenCalls; }
        [[nodiscard]] std::size_t get_nodes_range_call_count() const noexcept { return m_getNodesRangeCalls; }
        [[nodiscard]] std::size_t get_edges_range_call_count() const noexcept { return m_getEdgesRangeCalls; }
        [[nodiscard]] std::size_t get_parent_ranges_call_count() const noexcept { return m_getParentRangesCalls; }

    private:
        template <class T>
        static StatusCode copy_range(const std::vector<T>& source,
                                     std::vector<T>& destination,
                                     const std::size_t startIndex,
                                     const std::size_t count)
        {
            destination.clear();

            if (startIndex >= source.size() || count == 0)
            {
                return StatusCode::STATUS_OK;
            }

            const auto available = std::min(count, source.size() - startIndex);
            destination.assign(
                source.begin() + static_cast<std::ptrdiff_t>(startIndex),
                source.begin() + static_cast<std::ptrdiff_t>(startIndex + available));
            return StatusCode::STATUS_OK;
        }

        PointerScanConfig m_config{};
        std::vector<ModuleRecord> m_modules{};
        std::vector<PointerNodeRecord> m_nodes{};
        std::vector<PointerEdgeRecord> m_edges{};
        std::vector<ParentEdgeRange> m_parentRanges{};
        std::vector<std::uint32_t> m_rootNodeIds{};
        mutable std::size_t m_getNodeCalls{};
        mutable std::size_t m_getChildrenCalls{};
        mutable std::size_t m_getNodesRangeCalls{};
        mutable std::size_t m_getEdgesRangeCalls{};
        mutable std::size_t m_getParentRangesCalls{};
    };

    TEST(PointerScanComparatorTest, LoadSnapshot_DoesNotTreatTargetNodeAsRoot)
    {
        const auto uniqueSuffix = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const auto filePath = std::filesystem::temp_directory_path() /
                              ("vertex_pointerscancomparator_test_" + std::to_string(uniqueSuffix) + ".vxp");

        PointerScanConfig config{
            .targetAddress = 0x5000,
            .maxDepth = 2,
            .maxOffset = 0x1000,
            .alignment = 8,
            .maxParentsPerNode = 64,
            .maxNodes = 1024,
            .maxEdges = 1024,
            .workerChunkSize = 8192};

        const std::string moduleName{"testmod"};
        std::vector<PointerNodeRecord> nodes{
            {.address = 0x5000, .moduleId = 0, .minDepthDiscovered = 0, .flags = 0},
            {.address = 0x6000, .moduleId = 0, .minDepthDiscovered = 1, .flags = 0}};
        std::vector<PointerEdgeRecord> edges{
            {.parentNodeId = 0, .childNodeId = 1, .offset = 0x10, .flags = 0}};
        std::vector<ParentEdgeRange> parentRanges{
            {.firstEdgeIndex = 0, .count = 1},
            {.firstEdgeIndex = 0, .count = 0}};

        {
            std::ofstream file{filePath, std::ios::binary | std::ios::trunc};
            ASSERT_TRUE(file.is_open());

            VxpFileHeader header{};
            header.version = VXP_CURRENT_VERSION;
            header.endianness = native_vxp_endianness();
            header.pointerSize = 8;
            header.configSnapshotHash = compute_config_hash(config);
            header.moduleCount = 1;
            header.moduleNameBytes = static_cast<std::uint32_t>(moduleName.size());
            header.nodeCount = static_cast<std::uint64_t>(nodes.size());
            header.edgeCount = static_cast<std::uint64_t>(edges.size());
            header.config = config_to_snapshot(config);

            ASSERT_TRUE(write_vxp_file_header(file, header));

            VxpModuleEntryHeader moduleHeader{
                .moduleId = 0,
                .moduleBase = 0x4000,
                .moduleSpan = 0x2000,
                .nameLength = static_cast<std::uint16_t>(moduleName.size())};

            ASSERT_TRUE(write_vxp_module_entry_header(file, header.version, moduleHeader));
            file.write(moduleName.data(), static_cast<std::streamsize>(moduleName.size()));
            file.write(reinterpret_cast<const char*>(nodes.data()),
                       static_cast<std::streamsize>(nodes.size() * sizeof(PointerNodeRecord)));
            file.write(reinterpret_cast<const char*>(edges.data()),
                       static_cast<std::streamsize>(edges.size() * sizeof(PointerEdgeRecord)));
            file.write(reinterpret_cast<const char*>(parentRanges.data()),
                       static_cast<std::streamsize>(parentRanges.size() * sizeof(ParentEdgeRange)));
            ASSERT_TRUE(file.good());
        }

        GraphSnapshot snapshot{};
        ASSERT_EQ(StatusCode::STATUS_OK, PointerScanComparator::load_snapshot(filePath, snapshot));
        EXPECT_TRUE(snapshot.rootNodeIds.empty());

        const auto paths = PointerScanComparator::extract_paths(snapshot);
        EXPECT_TRUE(paths.empty());

        std::error_code ec{};
        std::filesystem::remove(filePath, ec);
    }

    TEST(PointerScanComparatorTest, ExtractPaths_LiveScannerReturnsEmptyWhenGetNodeFails)
    {
        FailingNodeScanner scanner{};

        const auto paths = PointerScanComparator::extract_paths(scanner);

        EXPECT_TRUE(paths.empty());
        EXPECT_EQ(2U, scanner.get_node_call_count());
    }

    TEST(PointerScanComparatorTest, ExtractPaths_LiveScannerUsesBulkRangeAccess)
    {
        BulkRangeScanner scanner{};

        const auto paths = PointerScanComparator::extract_paths(scanner);

        ASSERT_EQ(1U, paths.size());
        EXPECT_EQ("testmod", paths[0].rootModuleName);
        EXPECT_EQ(0x2000U, paths[0].rootModuleOffset);
        ASSERT_EQ(1U, paths[0].offsets.size());
        EXPECT_EQ(0x10, paths[0].offsets[0]);

        EXPECT_EQ(0U, scanner.get_node_call_count());
        EXPECT_EQ(0U, scanner.get_children_call_count());
        EXPECT_EQ(1U, scanner.get_nodes_range_call_count());
        EXPECT_EQ(1U, scanner.get_edges_range_call_count());
        EXPECT_EQ(1U, scanner.get_parent_ranges_call_count());
    }

    TEST(PointerScanComparatorTest, ExtractPaths_DeduplicatesEquivalentSignatures)
    {
        GraphSnapshot snapshot{};
        snapshot.config = PointerScanConfig{
            .targetAddress = 0x5000,
            .maxDepth = 3,
            .maxOffset = 0x1000,
            .alignment = 8,
            .maxParentsPerNode = 64,
            .maxNodes = 1024,
            .maxEdges = 1024,
            .workerChunkSize = 8192};

        snapshot.modules = {
            {.moduleId = 0, .moduleBase = 0x1000, .moduleSpan = 0x8000, .moduleName = "testmod"}};

        snapshot.nodes = {
            {.address = 0x5000, .moduleId = 0, .minDepthDiscovered = 0, .flags = 0},
            {.address = 0x2000, .moduleId = 0, .minDepthDiscovered = 1, .flags = 0},
            {.address = 0x3000, .moduleId = 0, .minDepthDiscovered = 2, .flags = 0},
            {.address = 0x3100, .moduleId = 0, .minDepthDiscovered = 2, .flags = 0}};

        snapshot.edges = {
            {.parentNodeId = 1, .childNodeId = 2, .offset = 0x10, .flags = 0},
            {.parentNodeId = 1, .childNodeId = 3, .offset = 0x10, .flags = 0},
            {.parentNodeId = 2, .childNodeId = 0, .offset = 0x20, .flags = 0},
            {.parentNodeId = 3, .childNodeId = 0, .offset = 0x20, .flags = 0}};

        snapshot.parentRanges = {
            {.firstEdgeIndex = 0, .count = 0},
            {.firstEdgeIndex = 0, .count = 2},
            {.firstEdgeIndex = 2, .count = 1},
            {.firstEdgeIndex = 3, .count = 1}};

        snapshot.rootNodeIds = {1};

        const auto paths = PointerScanComparator::extract_paths(snapshot);

        ASSERT_EQ(1U, paths.size());
        EXPECT_EQ("testmod", paths[0].rootModuleName);
        EXPECT_EQ(0x1000U, paths[0].rootModuleOffset);
        ASSERT_EQ(2U, paths[0].offsets.size());
        EXPECT_EQ(0x10, paths[0].offsets[0]);
        EXPECT_EQ(0x20, paths[0].offsets[1]);
    }

    TEST(PointerScanComparatorTest, FindStablePaths_IntersectsAllSetsUsingFullSignatureEquality)
    {
        const PointerPathSignature sigA{
            .rootModuleName = "game.exe",
            .rootModuleOffset = 0x1000,
            .offsets = {0x10, 0x20}};
        const PointerPathSignature sigB{
            .rootModuleName = "engine.dll",
            .rootModuleOffset = 0x2000,
            .offsets = {0x18, 0x28}};
        const PointerPathSignature sigC{
            .rootModuleName = "render.dll",
            .rootModuleOffset = 0x3000,
            .offsets = {0x30, 0x40}};

        const std::array<std::vector<PointerPathSignature>, 3> allPathSets{
            std::vector<PointerPathSignature>{sigA, sigB, sigB},
            std::vector<PointerPathSignature>{sigB, sigC},
            std::vector<PointerPathSignature>{sigB}};

        const auto stable = PointerScanComparator::find_stable_paths(allPathSets);

        ASSERT_EQ(1U, stable.size());
        EXPECT_EQ(sigB, stable.front());
    }

    TEST(PointerScanComparatorTest, FindStablePaths_RanksByLengthThenModuleThenOffsets)
    {
        const PointerPathSignature shortPathB{
            .rootModuleName = "b.dll",
            .rootModuleOffset = 0x2000,
            .offsets = {0x10}};
        const PointerPathSignature shortPathA{
            .rootModuleName = "a.dll",
            .rootModuleOffset = 0x1000,
            .offsets = {0x20}};
        const PointerPathSignature longPath{
            .rootModuleName = "a.dll",
            .rootModuleOffset = 0x1000,
            .offsets = {0x20, 0x30}};

        const std::array<std::vector<PointerPathSignature>, 2> allPathSets{
            std::vector<PointerPathSignature>{longPath, shortPathB, shortPathA},
            std::vector<PointerPathSignature>{shortPathA, longPath, shortPathB}};

        const auto stable = PointerScanComparator::find_stable_paths(allPathSets);

        ASSERT_EQ(3U, stable.size());
        EXPECT_EQ(shortPathA, stable[0]);
        EXPECT_EQ(shortPathB, stable[1]);
        EXPECT_EQ(longPath, stable[2]);
    }

    TEST(PointerScanComparatorTest, LoadSnapshot_RespectsStaticRootsOnlyConfig)
    {
        const auto uniqueSuffix = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const auto filePath = std::filesystem::temp_directory_path() /
                              ("vertex_pointerscancomparator_staticroots_test_" + std::to_string(uniqueSuffix) + ".vxp");

        PointerScanConfig config{
            .targetAddress = 0x5000,
            .maxDepth = 3,
            .maxOffset = 0x1000,
            .alignment = 8,
            .staticRootsOnly = true,
            .maxParentsPerNode = 64,
            .maxNodes = 1024,
            .maxEdges = 1024,
            .workerChunkSize = 8192};

        const std::string moduleName{"testmod"};
        std::vector<PointerNodeRecord> nodes{
            {.address = 0x5000, .moduleId = 0, .minDepthDiscovered = 0, .flags = 0},
            {.address = 0x7000, .moduleId = 0, .minDepthDiscovered = 1, .flags = 0},
            {.address = 0x9000, .moduleId = PointerScanComparator::NO_MODULE, .minDepthDiscovered = 1, .flags = 0}};
        std::vector<PointerEdgeRecord> edges{
            {.parentNodeId = 1, .childNodeId = 0, .offset = 0x10, .flags = 0},
            {.parentNodeId = 2, .childNodeId = 0, .offset = 0x20, .flags = 0}};
        std::vector<ParentEdgeRange> parentRanges{
            {.firstEdgeIndex = 0, .count = 0},
            {.firstEdgeIndex = 0, .count = 1},
            {.firstEdgeIndex = 1, .count = 1}};

        {
            std::ofstream file{filePath, std::ios::binary | std::ios::trunc};
            ASSERT_TRUE(file.is_open());

            VxpFileHeader header{};
            header.version = VXP_CURRENT_VERSION;
            header.endianness = native_vxp_endianness();
            header.pointerSize = 8;
            header.configSnapshotHash = compute_config_hash(config);
            header.moduleCount = 1;
            header.moduleNameBytes = static_cast<std::uint32_t>(moduleName.size());
            header.nodeCount = static_cast<std::uint64_t>(nodes.size());
            header.edgeCount = static_cast<std::uint64_t>(edges.size());
            header.config = config_to_snapshot(config);

            ASSERT_TRUE(write_vxp_file_header(file, header));

            VxpModuleEntryHeader moduleHeader{
                .moduleId = 0,
                .moduleBase = 0x4000,
                .moduleSpan = 0x3000,
                .nameLength = static_cast<std::uint16_t>(moduleName.size())};

            ASSERT_TRUE(write_vxp_module_entry_header(file, header.version, moduleHeader));
            file.write(moduleName.data(), static_cast<std::streamsize>(moduleName.size()));
            file.write(reinterpret_cast<const char*>(nodes.data()),
                       static_cast<std::streamsize>(nodes.size() * sizeof(PointerNodeRecord)));
            file.write(reinterpret_cast<const char*>(edges.data()),
                       static_cast<std::streamsize>(edges.size() * sizeof(PointerEdgeRecord)));
            file.write(reinterpret_cast<const char*>(parentRanges.data()),
                       static_cast<std::streamsize>(parentRanges.size() * sizeof(ParentEdgeRange)));
            ASSERT_TRUE(file.good());
        }

        GraphSnapshot snapshot{};
        ASSERT_EQ(StatusCode::STATUS_OK, PointerScanComparator::load_snapshot(filePath, snapshot));
        ASSERT_EQ(1U, snapshot.rootNodeIds.size());
        EXPECT_EQ(1U, snapshot.rootNodeIds[0]);

        std::error_code ec{};
        std::filesystem::remove(filePath, ec);
    }

    TEST(PointerScanComparatorTest, ModulesCompatible_ReturnsFalseWhenModuleNamesDiffer)
    {
        const std::vector<ModuleRecord> lhs{
            {.moduleId = 0, .moduleBase = 0x1000, .moduleSpan = 0x4000, .moduleName = "game.exe"}};
        const std::vector<ModuleRecord> rhs{
            {.moduleId = 0, .moduleBase = 0x2000, .moduleSpan = 0x4000, .moduleName = "engine.dll"}};

        EXPECT_FALSE(PointerScanComparator::modules_compatible(lhs, rhs));
    }

    TEST(PointerScanComparatorTest, ModulesCompatible_ReturnsFalseWhenModuleSpansDiffer)
    {
        const std::vector<ModuleRecord> lhs{
            {.moduleId = 0, .moduleBase = 0x1000, .moduleSpan = 0x4000, .moduleName = "game.exe"}};
        const std::vector<ModuleRecord> rhs{
            {.moduleId = 0, .moduleBase = 0x2000, .moduleSpan = 0x5000, .moduleName = "game.exe"}};

        EXPECT_FALSE(PointerScanComparator::modules_compatible(lhs, rhs));
    }

    TEST(PointerScanComparatorTest, ModulesCompatible_ReturnsFalseWhenModuleSpanMissing)
    {
        const std::vector<ModuleRecord> liveModules{
            {.moduleId = 0, .moduleBase = 0x1000, .moduleSpan = 0x4000, .moduleName = "game.exe"}};
        const std::vector<ModuleRecord> legacySnapshotModules{
            {.moduleId = 0, .moduleBase = 0x3000, .moduleSpan = 0, .moduleName = "game.exe"}};

        EXPECT_FALSE(PointerScanComparator::modules_compatible(liveModules, legacySnapshotModules));
    }
} // namespace
