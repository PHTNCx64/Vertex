//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/macrohelp.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/scanner/pointerscanner/ipointerscanner.hh>
#include <vertex/io/scanresultstore.hh>
#include <vertex/log/ilog.hh>
#include <vertex/runtime/iloader.hh>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Vertex::Scanner
{
    class AddressToNodeFlatMap final
    {
    public:
        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_size;
        }

        void clear() noexcept
        {
            if (!m_occupied.empty())
            {
                std::ranges::fill(m_occupied, static_cast<std::uint8_t>(0));
            }
            m_size = 0;
        }

        void reserve(const std::size_t desiredSize)
        {
            const std::size_t minCapacity = (desiredSize * MAX_LOAD_DENOMINATOR + (MAX_LOAD_NUMERATOR - 1)) / MAX_LOAD_NUMERATOR;
            if (m_slots.empty() || minCapacity > m_slots.size())
            {
                rehash(minCapacity);
            }
        }

        [[nodiscard]] const std::uint32_t* find(const std::uint64_t key) const noexcept
        {
            if (m_slots.empty())
            {
                return nullptr;
            }

            std::size_t idx = bucket_index(key);
            while (m_occupied[idx] != 0)
            {
                if (m_slots[idx].key == key)
                {
                    return &m_slots[idx].value;
                }
                idx = (idx + 1) & m_mask;
            }

            return nullptr;
        }

        [[nodiscard]] std::pair<std::uint32_t, bool> find_or_insert(const std::uint64_t key, const std::uint32_t insertedValue)
        {
            maybe_grow_for_insert();

            std::size_t idx = bucket_index(key);
            while (m_occupied[idx] != 0)
            {
                if (m_slots[idx].key == key)
                {
                    return {m_slots[idx].value, false};
                }
                idx = (idx + 1) & m_mask;
            }

            m_occupied[idx] = 1;
            m_slots[idx].key = key;
            m_slots[idx].value = insertedValue;
            ++m_size;
            return {insertedValue, true};
        }

        void insert_or_assign(const std::uint64_t key, const std::uint32_t value)
        {
            maybe_grow_for_insert();

            std::size_t idx = bucket_index(key);
            while (m_occupied[idx] != 0)
            {
                if (m_slots[idx].key == key)
                {
                    m_slots[idx].value = value;
                    return;
                }
                idx = (idx + 1) & m_mask;
            }

            m_occupied[idx] = 1;
            m_slots[idx].key = key;
            m_slots[idx].value = value;
            ++m_size;
        }

    private:
        struct Slot final
        {
            std::uint64_t key{};
            std::uint32_t value{};
        };

        static constexpr std::size_t MIN_CAPACITY = 16;
        static constexpr std::size_t MAX_LOAD_NUMERATOR = 7;
        static constexpr std::size_t MAX_LOAD_DENOMINATOR = 10;

        [[nodiscard]] static std::size_t next_power_of_two(std::size_t value) noexcept
        {
            if (value <= 1)
            {
                return 1;
            }

            std::size_t power = 1;
            while (power < value)
            {
                power <<= 1;
            }
            return power;
        }

        [[nodiscard]] static std::uint64_t mix_hash(std::uint64_t value) noexcept
        {
            value ^= value >> 33;
            value *= 0xff51afd7ed558ccdULL;
            value ^= value >> 33;
            value *= 0xc4ceb9fe1a85ec53ULL;
            value ^= value >> 33;
            return value;
        }

        [[nodiscard]] std::size_t bucket_index(const std::uint64_t key) const noexcept
        {
            return mix_hash(key) & m_mask;
        }

        void maybe_grow_for_insert()
        {
            if (m_slots.empty())
            {
                rehash(MIN_CAPACITY);
                return;
            }

            const bool requiresGrowth =
                (m_size + 1) * MAX_LOAD_DENOMINATOR > m_slots.size() * MAX_LOAD_NUMERATOR;
            if (requiresGrowth)
            {
                rehash(m_slots.size() * 2);
            }
        }

        void rehash(const std::size_t requestedCapacity)
        {
            const std::size_t newCapacity = std::max(MIN_CAPACITY, next_power_of_two(requestedCapacity));
            std::vector<Slot> newSlots(newCapacity);
            std::vector<std::uint8_t> newOccupied(newCapacity, 0);
            const std::size_t newMask = newCapacity - 1;

            for (std::size_t i{}; i < m_slots.size(); ++i)
            {
                if (i >= m_occupied.size() || m_occupied[i] == 0)
                {
                    continue;
                }

                std::size_t idx = static_cast<std::size_t>(mix_hash(m_slots[i].key)) & newMask;
                while (newOccupied[idx] != 0)
                {
                    idx = (idx + 1) & newMask;
                }

                newOccupied[idx] = 1;
                newSlots[idx] = m_slots[i];
            }

            m_slots = std::move(newSlots);
            m_occupied = std::move(newOccupied);
            m_mask = newMask;
        }

        std::vector<Slot> m_slots{};
        std::vector<std::uint8_t> m_occupied{};
        std::size_t m_size{};
        std::size_t m_mask{};
    };

    struct IndexRunMetadata final
    {
        IO::ScanResultStore store{};
        std::size_t entryCount{};
    };

    struct WorkerIndexBuffer final
    {
        std::vector<ReverseIndexEntry> entries{};
        std::size_t capacity{};
    };

    struct WorkerEdgeBuffer final
    {
        std::vector<PointerEdgeRecord> edges{};
    };

    struct EdgeRunMetadata final
    {
        IO::ScanResultStore store{};
        std::size_t edgeCount{};
    };

    class PointerScanner final : public IPointerScanner
    {
    public:
        PointerScanner(Configuration::ISettings& settingsService, Log::ILog& logService, Thread::IThreadDispatcher& dispatcher, Runtime::ILoader& loaderService);
        ~PointerScanner() override;

        PointerScanner(const PointerScanner&) = delete;
        PointerScanner& operator=(const PointerScanner&) = delete;
        PointerScanner(PointerScanner&&) = delete;
        PointerScanner& operator=(PointerScanner&&) = delete;

        void set_memory_reader(std::shared_ptr<IMemoryReader> reader) override;
        [[nodiscard]] bool has_memory_reader() const override;

        StatusCode initialize_scan(const PointerScanConfig& config, const std::vector<ScanRegion>& memoryRegions) override;
        StatusCode initialize_rescan() override;
        StatusCode stop_scan() override;
        void finalize_scan() override;

        [[nodiscard]] bool is_scan_complete() override;
        [[nodiscard]] StatusCode get_scan_status() const noexcept override;
        [[nodiscard]] PointerScanProgress get_progress() const noexcept override;

        [[nodiscard]] std::uint64_t get_node_count() const noexcept override;
        [[nodiscard]] std::uint64_t get_edge_count() const noexcept override;

        [[nodiscard]] std::uint64_t get_root_count() const noexcept override;
        [[nodiscard]] StatusCode get_roots_range(std::vector<PointerNodeRecord>& roots, std::size_t startIndex, std::size_t count) const override;
        [[nodiscard]] StatusCode get_root_node_ids(std::vector<std::uint32_t>& nodeIds, std::size_t startIndex, std::size_t count) const override;
        [[nodiscard]] StatusCode get_children(std::uint32_t nodeId, std::vector<PointerEdgeRecord>& edges, std::size_t startIndex, std::size_t count) const override;
        [[nodiscard]] StatusCode get_node(std::uint32_t nodeId, PointerNodeRecord& node) const override;
        [[nodiscard]] StatusCode get_nodes_range(std::vector<PointerNodeRecord>& nodes, std::size_t startIndex, std::size_t count) const override;
        [[nodiscard]] StatusCode get_edges_range(std::vector<PointerEdgeRecord>& edges, std::size_t startIndex, std::size_t count) const override;
        [[nodiscard]] StatusCode get_parent_ranges(std::vector<ParentEdgeRange>& ranges, std::size_t startIndex, std::size_t count) const override;
        [[nodiscard]] const std::vector<ModuleRecord>& get_modules() const noexcept override;

        [[nodiscard]] StatusCode save_graph(const std::filesystem::path& filePath) const override;
        [[nodiscard]] StatusCode load_graph(const std::filesystem::path& filePath) override;
        [[nodiscard]] const PointerScanConfig& get_config() const noexcept override;

        void wait_for_scan_completion();

    private:
        [[nodiscard]] bool drain_active_scan();
        StatusCode create_worker_pool(std::size_t workerCount);
        void reset_state();
        void reset_bfs_state();
        [[nodiscard]] bool can_reuse_index(const IndexSignature& newSignature) const;
        [[nodiscard]] static IndexSignature compute_index_signature(std::uint64_t pointerSize, std::uint32_t alignment, std::uint64_t contextHash, const std::vector<ScanRegion>& regions);
        void build_module_table(const std::vector<ScanRegion>& regions);
        [[nodiscard]] std::uint32_t resolve_module_id(std::string_view moduleName) const;

        StatusCode run_coordinator(const std::vector<ScanRegion>& regions);
        void wait_for_remote_workers();
        void report_worker_error(StatusCode status);
        [[nodiscard]] StatusCode collect_worker_error();

        StatusCode build_reverse_index(const std::vector<ScanRegion>& regions);
        StatusCode scan_region_for_pointers(const ScanRegion& region, std::size_t workerIndex);
        StatusCode flush_index_buffer(std::size_t workerIndex);
        StatusCode merge_index_runs();

        StatusCode run_bfs();
        StatusCode process_bfs_depth(std::uint16_t depth, std::uint64_t depthStartNodeCount);
        StatusCode process_frontier_chunk(std::size_t chunkStart, std::size_t chunkEnd, std::uint16_t depth, std::size_t workerIndex, std::uint64_t depthStartNodeCount);
        [[nodiscard]] std::uint64_t get_estimated_edge_count() const noexcept;
        StatusCode flush_edge_buffer(std::size_t workerIndex);
        StatusCode merge_edge_runs();
        [[nodiscard]] const PointerEdgeRecord* edge_data() const noexcept;
        [[nodiscard]] std::size_t edge_count() const noexcept;
        [[nodiscard]] bool edges_empty() const noexcept;
        [[nodiscard]] const ParentEdgeRange* parent_range_data() const noexcept;
        [[nodiscard]] std::size_t parent_range_count() const noexcept;
        [[nodiscard]] static bool edge_less(const PointerEdgeRecord& a, const PointerEdgeRecord& b) noexcept;
        [[nodiscard]] static bool edge_equal(const PointerEdgeRecord& a, const PointerEdgeRecord& b) noexcept;

        template <class Visitor>
        void range_query_index(std::uint64_t low, std::uint64_t high, Visitor&& visitor) const;

        StatusCode finalize_graph();
        StatusCode sort_edges();
        StatusCode prune_back_edges();
        StatusCode build_parent_edge_ranges();
        void identify_roots();

        StatusCode run_rescan_coordinator();
        StatusCode validate_edges_parallel();
        StatusCode validate_edge_chunk(std::size_t chunkStart, std::size_t chunkEnd, std::vector<std::uint8_t>& invalidMask);
        StatusCode prune_invalid_edges(const std::vector<std::uint8_t>& invalidMask);
        StatusCode remove_orphan_nodes();

        static constexpr std::uint32_t NO_MODULE = std::numeric_limits<std::uint32_t>::max();
        static constexpr std::size_t VISITED_SHARD_COUNT = 64;
        static constexpr std::uint64_t MIN_VALID_POINTER = 0x10000;
        static constexpr std::size_t INITIAL_NODE_RESERVE = 1000000;

        START_PADDING_WARNING_SUPPRESSION

        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_scanAbort{};
        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_scanComplete{true};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_regionsScanned{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_totalRegions{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_nodesDiscovered{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_edgesCreated{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_frontierNodesProcessed{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_frontierSize{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_nextChunkIndex{};
        alignas(std::hardware_destructive_interference_size) std::atomic<int> m_activeWorkers{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::int32_t> m_workerError{StatusCode::STATUS_OK};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_edgesValidated{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_totalEdgesToValidate{};
        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_nodeCapExceeded{};

        END_PADDING_WARNING_SUPPRESSION

        std::atomic<PointerScanPhase> m_currentPhase{PointerScanPhase::Idle};
        std::atomic<std::uint16_t> m_currentDepth{};

        PointerScanConfig m_config{};
        std::uint64_t m_pointerSize{};
        std::atomic<StatusCode> m_scanStatus{StatusCode::STATUS_OK};

        std::vector<ModuleRecord> m_modules{};
        std::unordered_map<std::string, std::uint32_t> m_moduleNameToId{};

        std::vector<std::vector<IndexRunMetadata>> m_workerIndexRuns{};
        std::vector<WorkerIndexBuffer> m_workerIndexBuffers{};
        IO::ScanResultStore m_mergedIndex{};
        std::size_t m_mergedIndexEntryCount{};

        std::optional<IndexSignature> m_cachedIndexSignature{};
        IndexSignature m_pendingIndexSignature{};
        bool m_indexValid{};

        std::vector<PointerNodeRecord> m_nodes{};
        std::vector<EdgeRunMetadata> m_edgeRuns{};
        IO::ScanResultStore m_edgesStore{};
        std::size_t m_edgeCount{};
        IO::ScanResultStore m_parentRangesStore{};
        std::size_t m_parentRangeCount{};
        std::vector<std::uint32_t> m_rootNodeIds{};

        struct VisitedShard final
        {
            std::mutex mutex{};
            AddressToNodeFlatMap addressToNodeId{};
            std::vector<std::uint32_t> newNodeIds{};
            std::vector<PointerNodeRecord> pendingNodes{};
        };

        std::array<VisitedShard, VISITED_SHARD_COUNT> m_visitedShards{};

        std::vector<WorkerEdgeBuffer> m_workerEdgeBuffers{};
        std::atomic<std::uint64_t> m_totalEdgeCountDuringBFS{};
        std::size_t m_edgeFlushThresholdEdges{};
        std::mutex m_edgeRunMutex{};

        std::vector<std::uint32_t> m_currentFrontier{};
        std::vector<std::uint32_t> m_nextFrontier{};

        std::condition_variable m_completionCondition{};
        std::mutex m_completionMutex{};
        std::mutex m_scanLifecycleMutex{};

        std::shared_ptr<IMemoryReader> m_memoryReader{};
        mutable std::mutex m_memoryReaderMutex{};

        std::size_t m_workerCount{};

        Configuration::ISettings& m_settingsService;
        Log::ILog& m_logService;
        Thread::IThreadDispatcher& m_dispatcher;
        Runtime::ILoader& m_loaderService;
    };
} // namespace Vertex::Scanner
