//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscanner.hh>

#include <algorithm>
#include <queue>
#include <ranges>
#include <fmt/format.h>

namespace Vertex::Scanner
{
    StatusCode PointerScanner::finalize_graph()
    {
        m_currentPhase.store(PointerScanPhase::FinalizeGraph, std::memory_order_release);

        StatusCode status = sort_edges();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = prune_back_edges();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = build_parent_edge_ranges();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        identify_roots();

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::sort_edges()
    {
        if (edges_empty())
        {
            return StatusCode::STATUS_OK;
        }

        if (!m_edgeRuns.empty())
        {
            return merge_edge_runs();
        }

        const auto* allEdges = edge_data();
        if (allEdges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        IO::ScanResultStore dedupedStore{};
        StatusCode status = dedupedStore.open();
        if (status != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        constexpr std::size_t DEDUP_BATCH_SIZE = 16384;
        std::vector<PointerEdgeRecord> dedupBatch{};
        dedupBatch.reserve(DEDUP_BATCH_SIZE);

        PointerEdgeRecord previous{};
        bool hasPrevious{};
        std::size_t uniqueCount{};

        for (std::size_t i{}; i < edge_count(); ++i)
        {
            const auto& edge = allEdges[i];
            if (hasPrevious && edge_equal(edge, previous))
            {
                continue;
            }

            dedupBatch.push_back(edge);
            previous = edge;
            hasPrevious = true;
            ++uniqueCount;

            if (dedupBatch.size() >= DEDUP_BATCH_SIZE)
            {
                status = dedupedStore.append(dedupBatch.data(), dedupBatch.size() * sizeof(PointerEdgeRecord));
                if (status != StatusCode::STATUS_OK)
                {
                    return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
                }
                dedupBatch.clear();
            }
        }

        if (!dedupBatch.empty())
        {
            status = dedupedStore.append(dedupBatch.data(), dedupBatch.size() * sizeof(PointerEdgeRecord));
            if (status != StatusCode::STATUS_OK)
            {
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
            }
        }

        status = dedupedStore.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        m_edgesStore = std::move(dedupedStore);
        m_edgeCount = uniqueCount;
        m_edgesCreated.store(m_edgeCount, std::memory_order_release);
        m_logService.log_info(fmt::format("[PointerScanner] Edges after dedup: {}", m_edgeCount));

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::merge_edge_runs()
    {
        if (m_edgeRuns.empty())
        {
            m_logService.log_info("[PointerScanner] No edge runs to merge");
            return StatusCode::STATUS_OK;
        }

        if (m_edgeRuns.size() == 1)
        {
            m_edgesStore = std::move(m_edgeRuns[0].store);
            m_edgeCount = m_edgeRuns[0].edgeCount;
            m_edgeRuns.clear();

            m_edgesCreated.store(m_edgeCount, std::memory_order_release);
            m_logService.log_info(fmt::format("[PointerScanner] Single edge run, no merge needed: {} edges", m_edgeCount));
            return StatusCode::STATUS_OK;
        }

        struct MergeEntry final
        {
            PointerEdgeRecord edge{};
            std::size_t runIndex{};
            std::size_t position{};
        };

        const auto mergeEntryGreater = [](const MergeEntry& a, const MergeEntry& b)
        {
            return PointerScanner::edge_less(b.edge, a.edge);
        };

        std::priority_queue<MergeEntry, std::vector<MergeEntry>, decltype(mergeEntryGreater)> heap{mergeEntryGreater};

        std::vector<const PointerEdgeRecord*> runBases(m_edgeRuns.size(), nullptr);
        std::vector<std::size_t> runSizes(m_edgeRuns.size(), 0);

        for (std::size_t i{}; i < m_edgeRuns.size(); ++i)
        {
            runBases[i] = static_cast<const PointerEdgeRecord*>(m_edgeRuns[i].store.base());
            runSizes[i] = m_edgeRuns[i].edgeCount;

            if (runSizes[i] > 0 && runBases[i] != nullptr)
            {
                heap.push(MergeEntry{
                    .edge = runBases[i][0],
                    .runIndex = i,
                    .position = 0});
            }
        }

        IO::ScanResultStore outputStore{};
        StatusCode status = outputStore.open();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to open merged edge store");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        constexpr std::size_t MERGE_BATCH_SIZE = 16384;
        std::vector<PointerEdgeRecord> mergeBatch{};
        mergeBatch.reserve(MERGE_BATCH_SIZE);

        PointerEdgeRecord previous{};
        bool hasPrevious{};
        std::size_t totalMerged{};

        while (!heap.empty())
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
            }

            auto [edge, runIndex, position] = heap.top();
            heap.pop();

            if (!hasPrevious || !edge_equal(edge, previous))
            {
                mergeBatch.push_back(edge);
                previous = edge;
                hasPrevious = true;
                ++totalMerged;
            }

            if (mergeBatch.size() >= MERGE_BATCH_SIZE)
            {
                status = outputStore.append(mergeBatch.data(), mergeBatch.size() * sizeof(PointerEdgeRecord));
                if (status != StatusCode::STATUS_OK)
                {
                    return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
                }
                mergeBatch.clear();
            }

            const std::size_t nextPos = position + 1;
            if (nextPos < runSizes[runIndex])
            {
                heap.push(MergeEntry{
                    .edge = runBases[runIndex][nextPos],
                    .runIndex = runIndex,
                    .position = nextPos});
            }
        }

        if (!mergeBatch.empty())
        {
            status = outputStore.append(mergeBatch.data(), mergeBatch.size() * sizeof(PointerEdgeRecord));
            if (status != StatusCode::STATUS_OK)
            {
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
            }
        }

        status = outputStore.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        m_edgesStore = std::move(outputStore);
        m_edgeCount = totalMerged;
        m_edgeRuns.clear();

        m_edgesCreated.store(m_edgeCount, std::memory_order_release);
        m_logService.log_info(fmt::format("[PointerScanner] Edges after dedup: {}", m_edgeCount));

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::prune_back_edges()
    {
        if (edges_empty())
        {
            return StatusCode::STATUS_OK;
        }

        const auto nodeCount = m_nodes.size();
        const auto previousSize = edge_count();
        const auto* allEdges = edge_data();
        if (allEdges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        IO::ScanResultStore prunedStore{};
        StatusCode status = prunedStore.open();
        if (status != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        constexpr std::size_t FILTER_BATCH_SIZE = 16384;
        std::vector<PointerEdgeRecord> keptBatch{};
        keptBatch.reserve(FILTER_BATCH_SIZE);

        std::size_t keptCount{};
        for (std::size_t i{}; i < previousSize; ++i)
        {
            const auto& edge = allEdges[i];
            if (edge.parentNodeId >= nodeCount || edge.childNodeId >= nodeCount)
            {
                continue;
            }

            if (m_nodes[edge.parentNodeId].minDepthDiscovered <=
                m_nodes[edge.childNodeId].minDepthDiscovered)
            {
                continue;
            }

            keptBatch.push_back(edge);
            ++keptCount;

            if (keptBatch.size() >= FILTER_BATCH_SIZE)
            {
                status = prunedStore.append(keptBatch.data(), keptBatch.size() * sizeof(PointerEdgeRecord));
                if (status != StatusCode::STATUS_OK)
                {
                    return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
                }
                keptBatch.clear();
            }
        }

        if (!keptBatch.empty())
        {
            status = prunedStore.append(keptBatch.data(), keptBatch.size() * sizeof(PointerEdgeRecord));
            if (status != StatusCode::STATUS_OK)
            {
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
            }
        }

        status = prunedStore.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        m_edgesStore = std::move(prunedStore);
        m_edgeCount = keptCount;

        if (m_edgeCount != previousSize)
        {
            m_logService.log_info(fmt::format("[PointerScanner] Pruned {} back-edges (DAG invariant)",
                                              previousSize - m_edgeCount));
            m_edgesCreated.store(m_edgeCount, std::memory_order_release);
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::build_parent_edge_ranges()
    {
        const auto nodeCount = m_nodes.size();
        if (nodeCount > std::numeric_limits<std::uint32_t>::max())
        {
            m_logService.log_error(fmt::format("[PointerScanner] Node count {} exceeds uint32_t max", nodeCount));
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        if (edge_count() > std::numeric_limits<std::uint32_t>::max())
        {
            m_logService.log_error(fmt::format("[PointerScanner] Edge count {} exceeds uint32_t max", edge_count()));
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        IO::ScanResultStore parentRangesStore{};
        if (const StatusCode openStatus = parentRangesStore.open(); openStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to open parent-range store");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        constexpr std::size_t RANGE_BATCH_SIZE = 16384;
        std::vector<ParentEdgeRange> rangeBatch{};
        rangeBatch.reserve(RANGE_BATCH_SIZE);

        const auto flushBatch = [&]() -> StatusCode
        {
            if (rangeBatch.empty())
            {
                return StatusCode::STATUS_OK;
            }

            const StatusCode appendStatus = parentRangesStore.append(
                rangeBatch.data(),
                rangeBatch.size() * sizeof(ParentEdgeRange));
            if (appendStatus != StatusCode::STATUS_OK)
            {
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
            }

            rangeBatch.clear();
            return StatusCode::STATUS_OK;
        };

        const auto appendRange = [&](const ParentEdgeRange range) -> StatusCode
        {
            rangeBatch.push_back(range);
            if (rangeBatch.size() < RANGE_BATCH_SIZE)
            {
                return StatusCode::STATUS_OK;
            }

            return flushBatch();
        };

        const std::uint32_t nodeCount32 = static_cast<std::uint32_t>(nodeCount);
        if (edges_empty())
        {
            for (std::uint32_t i{}; i < nodeCount32; ++i)
            {
                if (const StatusCode appendStatus = appendRange(ParentEdgeRange{.firstEdgeIndex = 0, .count = 0});
                    appendStatus != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append empty parent ranges");
                    return appendStatus;
                }
            }
        }
        else
        {
            const auto* allEdges = edge_data();
            if (allEdges == nullptr)
            {
                m_logService.log_error("[PointerScanner] Edge storage unavailable while building parent ranges");
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            std::uint32_t nextNodeToEmit{};
            std::uint32_t currentParent = allEdges[0].parentNodeId;
            if (currentParent >= nodeCount32)
            {
                m_logService.log_error(fmt::format("[PointerScanner] First edge parent {} exceeds node count {}", currentParent, nodeCount32));
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            while (nextNodeToEmit < currentParent)
            {
                if (const StatusCode appendStatus = appendRange(ParentEdgeRange{.firstEdgeIndex = 0, .count = 0});
                    appendStatus != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append leading empty parent ranges");
                    return appendStatus;
                }
                ++nextNodeToEmit;
            }

            std::uint32_t rangeStart{};
            const std::uint32_t totalEdges = static_cast<std::uint32_t>(edge_count());

            for (std::uint32_t i{}; i < totalEdges; ++i)
            {
                const std::uint32_t parent = allEdges[i].parentNodeId;
                if (parent == currentParent)
                {
                    continue;
                }

                if (parent < currentParent || parent >= nodeCount32)
                {
                    m_logService.log_error(fmt::format("[PointerScanner] Invalid parent ordering/index while building ranges: {} -> {}", currentParent, parent));
                    return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
                }

                if (const StatusCode appendStatus = appendRange(ParentEdgeRange{
                        .firstEdgeIndex = rangeStart,
                        .count = i - rangeStart});
                    appendStatus != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append parent range");
                    return appendStatus;
                }
                ++nextNodeToEmit;

                while (nextNodeToEmit < parent)
                {
                    if (const StatusCode appendStatus = appendRange(ParentEdgeRange{.firstEdgeIndex = 0, .count = 0});
                        appendStatus != StatusCode::STATUS_OK)
                    {
                        m_logService.log_error("[PointerScanner] Failed to append gap parent range");
                        return appendStatus;
                    }
                    ++nextNodeToEmit;
                }

                currentParent = parent;
                rangeStart = i;
            }

            if (const StatusCode appendStatus = appendRange(ParentEdgeRange{
                    .firstEdgeIndex = rangeStart,
                    .count = totalEdges - rangeStart});
                appendStatus != StatusCode::STATUS_OK)
            {
                m_logService.log_error("[PointerScanner] Failed to append final parent range");
                return appendStatus;
            }
            ++nextNodeToEmit;

            while (nextNodeToEmit < nodeCount32)
            {
                if (const StatusCode appendStatus = appendRange(ParentEdgeRange{.firstEdgeIndex = 0, .count = 0});
                    appendStatus != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append trailing empty parent ranges");
                    return appendStatus;
                }
                ++nextNodeToEmit;
            }
        }

        if (const StatusCode flushStatus = flushBatch(); flushStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to flush parent-range batch");
            return flushStatus;
        }

        if (const StatusCode finalizeStatus = parentRangesStore.finalize(); finalizeStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to finalize parent-range store");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        m_parentRangesStore = std::move(parentRangesStore);
        m_parentRangeCount = nodeCount;
        return StatusCode::STATUS_OK;
    }

    void PointerScanner::identify_roots()
    {
        const auto nodeCount = m_nodes.size();

        if (nodeCount == 0 || nodeCount > std::numeric_limits<std::uint32_t>::max())
        {
            return;
        }

        const std::uint32_t safeNodeCount = static_cast<std::uint32_t>(nodeCount);
        std::vector<bool> isChild(safeNodeCount, false);

        const auto* allEdges = edge_data();
        if (allEdges != nullptr)
        {
            for (std::size_t i{}; i < edge_count(); ++i)
            {
                if (allEdges[i].childNodeId < safeNodeCount)
                {
                    isChild[allEdges[i].childNodeId] = true;
                }
            }
        }

        m_rootNodeIds.clear();

        for (std::uint32_t i{}; i < safeNodeCount; ++i)
        {
            if (i == 0)
            {
                continue;
            }

            if (isChild[i])
            {
                continue;
            }

            if (m_nodes.size() <= i)
            {
                break;
            }

            if (m_config.staticRootsOnly && m_nodes[i].moduleId == NO_MODULE)
            {
                continue;
            }

            m_rootNodeIds.push_back(i);
        }

        std::ranges::sort(m_rootNodeIds, [this](const std::uint32_t a, const std::uint32_t b)
                          {
                              return m_nodes[a].address < m_nodes[b].address;
                          });

        m_logService.log_info(fmt::format("[PointerScanner] Identified {} roots (staticRootsOnly={})",
                                          m_rootNodeIds.size(), m_config.staticRootsOnly));
    }
} // namespace Vertex::Scanner
