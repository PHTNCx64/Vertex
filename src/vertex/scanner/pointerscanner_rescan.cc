//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscanner.hh>

#include <algorithm>
#include <cassert>
#include <future>
#include <memory>
#include <ranges>
#include <fmt/format.h>

namespace Vertex::Scanner
{
    StatusCode PointerScanner::run_rescan_coordinator()
    {
        m_currentPhase.store(PointerScanPhase::Rescan, std::memory_order_release);

        StatusCode status = validate_edges_parallel();

        if (status != StatusCode::STATUS_OK && !m_scanAbort.load(std::memory_order_acquire))
        {
            m_logService.log_error(fmt::format("[PointerScanner] Rescan validation failed: {}", static_cast<int>(status)));
        }

        if (m_scanAbort.load(std::memory_order_acquire))
        {
            status = StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
        }

        m_currentPhase.store(PointerScanPhase::FinalizeGraph, std::memory_order_release);

        if (status == StatusCode::STATUS_OK)
        {
            status = sort_edges();
        }

        if (status == StatusCode::STATUS_OK)
        {
            status = build_parent_edge_ranges();
        }

        if (status == StatusCode::STATUS_OK)
        {
            identify_roots();
        }

        m_nodesDiscovered.store(m_nodes.size(), std::memory_order_release);
        m_edgesCreated.store(edge_count(), std::memory_order_release);

        m_logService.log_info(fmt::format("[PointerScanner] Rescan complete: nodes={}, edges={}, roots={}",
                                          m_nodes.size(), edge_count(), m_rootNodeIds.size()));

        m_currentPhase.store(PointerScanPhase::Idle, std::memory_order_release);
        m_scanStatus.store(status, std::memory_order_release);
        m_scanComplete.store(true, std::memory_order_release);
        std::scoped_lock lock{m_completionMutex};
        m_completionCondition.notify_all();

        return status;
    }

    StatusCode PointerScanner::validate_edges_parallel()
    {
        const std::size_t edgeCount = edge_count();
        if (edgeCount == 0)
        {
            return StatusCode::STATUS_OK;
        }

        auto invalidMask = std::make_shared<std::vector<std::uint8_t>>(edgeCount, 0);
        const std::size_t chunkSize = m_config.workerChunkSize > 0 ? m_config.workerChunkSize : 8192;
        const std::size_t totalChunks = (edgeCount + chunkSize - 1) / chunkSize;

        m_nextChunkIndex.store(0, std::memory_order_release);

        const auto remoteWorkerCount = static_cast<int>(m_workerCount > 1 ? m_workerCount - 1 : 0);
        m_activeWorkers.store(remoteWorkerCount, std::memory_order_release);

        for (std::size_t i = 1; i < m_workerCount; ++i)
        {
            std::packaged_task<StatusCode()> task{
                [this, invalidMask, edgeCount, chunkSize, totalChunks]() -> StatusCode
                {
                    while (true)
                    {
                        if (m_scanAbort.load(std::memory_order_acquire))
                        {
                            break;
                        }

                        const auto chunkIdx = m_nextChunkIndex.fetch_add(1, std::memory_order_acq_rel);
                        if (chunkIdx >= totalChunks)
                        {
                            break;
                        }

                        const std::size_t chunkStart = chunkIdx * chunkSize;
                        const std::size_t chunkEnd = std::min(chunkStart + chunkSize, edgeCount);

                        const StatusCode chunkStatus = validate_edge_chunk(chunkStart, chunkEnd, *invalidMask);
                        if (chunkStatus != StatusCode::STATUS_OK)
                        {
                            report_worker_error(chunkStatus);
                        }
                    }

                    if (m_activeWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
                        std::scoped_lock lock{m_completionMutex};
                        m_completionCondition.notify_all();
                    }

                    return StatusCode::STATUS_OK;
                }};

            const StatusCode status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::PointerScanner, i, std::move(task));
            if (status != StatusCode::STATUS_OK)
            {
                report_worker_error(status);
                if (m_activeWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    std::scoped_lock lock{m_completionMutex};
                    m_completionCondition.notify_all();
                }
            }
        }

        while (true)
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                break;
            }

            const auto chunkIdx = m_nextChunkIndex.fetch_add(1, std::memory_order_acq_rel);
            if (chunkIdx >= totalChunks)
            {
                break;
            }

            const std::size_t chunkStart = chunkIdx * chunkSize;
            const std::size_t chunkEnd = std::min(chunkStart + chunkSize, edgeCount);

            const StatusCode inlineStatus = validate_edge_chunk(chunkStart, chunkEnd, *invalidMask);
            if (inlineStatus != StatusCode::STATUS_OK)
            {
                report_worker_error(inlineStatus);
            }
        }

        if (remoteWorkerCount > 0)
        {
            wait_for_remote_workers();
        }

        if (m_scanAbort.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
        }

        const StatusCode workerStatus = collect_worker_error();
        if (workerStatus != StatusCode::STATUS_OK)
        {
            return workerStatus;
        }

        StatusCode status = prune_invalid_edges(*invalidMask);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = remove_orphan_nodes();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::validate_edge_chunk(const std::size_t chunkStart, const std::size_t chunkEnd,
                                                    std::vector<std::uint8_t>& invalidMask)
    {
        std::shared_ptr<IMemoryReader> reader{};
        {
            std::scoped_lock lock{m_memoryReaderMutex};
            reader = m_memoryReader;
        }

        if (!reader)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto* allEdges = edge_data();
        if (allEdges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        const auto nodeCount = m_nodesDiscovered.load(std::memory_order_acquire);

        const auto validate_single_edge = [&](const std::size_t edgeIndex)
        {
            const auto& edge = allEdges[edgeIndex];

            if (edge.parentNodeId >= nodeCount || edge.childNodeId >= nodeCount)
            {
                invalidMask[edgeIndex] = true;
                m_edgesValidated.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            const auto& parentNode = m_nodes[edge.parentNodeId];
            const auto& childNode = m_nodes[edge.childNodeId];

            std::uint64_t pointerValue{};
            const StatusCode readStatus = reader->read_memory(parentNode.address, m_pointerSize, &pointerValue);
            if (readStatus != StatusCode::STATUS_OK)
            {
                invalidMask[edgeIndex] = true;
                m_edgesValidated.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            if (m_pointerSize == 4)
            {
                pointerValue &= 0xFFFFFFFF;
            }

            const auto expectedChild = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(pointerValue) + static_cast<std::int64_t>(edge.offset));
            if (expectedChild != childNode.address)
            {
                invalidMask[edgeIndex] = true;
            }

            m_edgesValidated.fetch_add(1, std::memory_order_relaxed);
        };

        if (!reader->supports_bulk_read())
        {
            for (std::size_t i = chunkStart; i < chunkEnd; ++i)
            {
                if (m_scanAbort.load(std::memory_order_acquire))
                {
                    return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
                }
                validate_single_edge(i);
            }

            return StatusCode::STATUS_OK;
        }

        std::size_t maxBulkRequests = static_cast<std::size_t>(std::max(1, m_settingsService.get_int("bulk.maxRequestSize", 4096)));
        const std::uint32_t readerLimit = reader->bulk_request_limit();
        if (readerLimit > 0)
        {
            maxBulkRequests = std::min(maxBulkRequests, static_cast<std::size_t>(readerLimit));
        }
        maxBulkRequests = std::max<std::size_t>(1, maxBulkRequests);

        std::vector<std::size_t> edgeIndices{};
        std::vector<std::uint64_t> pointerValues{};
        std::vector<BulkReadRequest> requests{};
        std::vector<BulkReadResult> results{};
        edgeIndices.reserve(maxBulkRequests);
        pointerValues.reserve(maxBulkRequests);
        requests.reserve(maxBulkRequests);
        results.reserve(maxBulkRequests);

        for (std::size_t subChunkStart = chunkStart; subChunkStart < chunkEnd; subChunkStart += maxBulkRequests)
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
            }

            const std::size_t subChunkEnd = std::min(subChunkStart + maxBulkRequests, chunkEnd);

            edgeIndices.clear();
            pointerValues.clear();
            requests.clear();
            results.clear();

            for (std::size_t i = subChunkStart; i < subChunkEnd; ++i)
            {
                if (m_scanAbort.load(std::memory_order_acquire))
                {
                    return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
                }

                const auto& edge = allEdges[i];
                if (edge.parentNodeId >= nodeCount || edge.childNodeId >= nodeCount)
                {
                    invalidMask[i] = true;
                    m_edgesValidated.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                edgeIndices.push_back(i);
                pointerValues.push_back(0);
            }

            if (edgeIndices.empty())
            {
                continue;
            }

            requests.resize(edgeIndices.size());
            results.resize(edgeIndices.size());

            for (std::size_t i{}; i < edgeIndices.size(); ++i)
            {
                const auto& edge = allEdges[edgeIndices[i]];
                const auto& parentNode = m_nodes[edge.parentNodeId];

                requests[i] = {
                    parentNode.address,
                    m_pointerSize,
                    &pointerValues[i]
                };
                results[i].status = StatusCode::STATUS_OK;
            }

            const StatusCode bulkStatus = reader->read_memory_bulk(requests, results);
            if (bulkStatus != StatusCode::STATUS_OK)
            {
                for (const std::size_t edgeIndex : edgeIndices)
                {
                    if (m_scanAbort.load(std::memory_order_acquire))
                    {
                        return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
                    }
                    validate_single_edge(edgeIndex);
                }
                continue;
            }

            for (std::size_t i{}; i < edgeIndices.size(); ++i)
            {
                const std::size_t edgeIndex = edgeIndices[i];
                const auto& edge = allEdges[edgeIndex];
                const auto& childNode = m_nodes[edge.childNodeId];

                if (results[i].status != StatusCode::STATUS_OK)
                {
                    invalidMask[edgeIndex] = true;
                    m_edgesValidated.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                std::uint64_t pointerValue = pointerValues[i];
                if (m_pointerSize == 4)
                {
                    pointerValue &= 0xFFFFFFFF;
                }

                const auto expectedChild = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(pointerValue) + static_cast<std::int64_t>(edge.offset));
                if (expectedChild != childNode.address)
                {
                    invalidMask[edgeIndex] = true;
                }

                m_edgesValidated.fetch_add(1, std::memory_order_relaxed);
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::prune_invalid_edges(const std::vector<std::uint8_t>& invalidMask)
    {
        const std::size_t originalCount = edge_count();
        const auto* allEdges = edge_data();
        if (edge_count() > 0 && allEdges == nullptr)
        {
            m_logService.log_error("[PointerScanner] Edge storage unavailable while pruning invalid edges");
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        IO::ScanResultStore prunedStore{};
        StatusCode status = prunedStore.open();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to open edge store while pruning invalid edges");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        constexpr std::size_t FILTER_BATCH_SIZE = 16384;
        std::vector<PointerEdgeRecord> keptBatch{};
        keptBatch.reserve(FILTER_BATCH_SIZE);

        std::size_t keptCount{};
        for (std::size_t i{}; i < originalCount; ++i)
        {
            if (i < invalidMask.size() && invalidMask[i] != 0)
            {
                continue;
            }

            keptBatch.push_back(allEdges[i]);
            ++keptCount;

            if (keptBatch.size() >= FILTER_BATCH_SIZE)
            {
                status = prunedStore.append(keptBatch.data(), keptBatch.size() * sizeof(PointerEdgeRecord));
                if (status != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append pruned edge batch");
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
                m_logService.log_error("[PointerScanner] Failed to append final pruned edge batch");
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
            }
        }

        status = prunedStore.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to finalize pruned edge store");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        m_edgesStore = std::move(prunedStore);
        m_edgeCount = keptCount;
        m_edgesCreated.store(m_edgeCount, std::memory_order_release);

        m_logService.log_info(fmt::format("[PointerScanner] Rescan pruned {} invalid edges ({} -> {})",
                                          originalCount - m_edgeCount, originalCount, m_edgeCount));
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::remove_orphan_nodes()
    {
        const auto nodeCount = static_cast<std::uint32_t>(m_nodesDiscovered.load(std::memory_order_acquire));
        if (nodeCount == 0)
        {
            return StatusCode::STATUS_OK;
        }

        std::vector<bool> referenced(nodeCount, false);

        referenced[0] = true;

        const auto* allEdges = edge_data();
        if (edge_count() > 0 && allEdges == nullptr)
        {
            m_logService.log_error("[PointerScanner] Edge storage unavailable while removing orphan nodes");
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        for (std::size_t i{}; i < edge_count(); ++i)
        {
            const auto& edge = allEdges[i];
            if (edge.parentNodeId < nodeCount)
            {
                referenced[edge.parentNodeId] = true;
            }
            if (edge.childNodeId < nodeCount)
            {
                referenced[edge.childNodeId] = true;
            }
        }

        std::vector<std::uint32_t> oldToNew(nodeCount, std::numeric_limits<std::uint32_t>::max());
        std::vector<PointerNodeRecord> compactedNodes{};
        compactedNodes.reserve(nodeCount);

        std::uint32_t newId{};
        for (std::uint32_t i{}; i < nodeCount; ++i)
        {
            if (referenced[i])
            {
                oldToNew[i] = newId;
                compactedNodes.push_back(m_nodes[i]);
                ++newId;
            }
        }

        if (newId == nodeCount)
        {
            m_logService.log_info("[PointerScanner] No orphan nodes to remove");
            return StatusCode::STATUS_OK;
        }

        IO::ScanResultStore remappedStore{};
        StatusCode status = remappedStore.open();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to open edge store for orphan node remap");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        constexpr std::size_t REMAP_BATCH_SIZE = 16384;
        std::vector<PointerEdgeRecord> remappedBatch{};
        remappedBatch.reserve(REMAP_BATCH_SIZE);
        constexpr auto INVALID_NODE_ID = std::numeric_limits<std::uint32_t>::max();

        for (std::size_t i{}; i < edge_count(); ++i)
        {
            PointerEdgeRecord edge = allEdges[i];
            if (edge.parentNodeId >= nodeCount || edge.childNodeId >= nodeCount)
            {
                m_logService.log_error(fmt::format(
                    "[PointerScanner] Edge {} references out-of-bounds node ids while remapping (parent={}, child={}, nodeCount={})",
                    i, edge.parentNodeId, edge.childNodeId, nodeCount));
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            const auto remappedParent = oldToNew[edge.parentNodeId];
            const auto remappedChild = oldToNew[edge.childNodeId];
            if (remappedParent == INVALID_NODE_ID || remappedChild == INVALID_NODE_ID)
            {
                m_logService.log_error(fmt::format(
                    "[PointerScanner] Edge {} references orphan node during remap (parent={}, child={})",
                    i, edge.parentNodeId, edge.childNodeId));
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            assert(remappedParent != INVALID_NODE_ID);
            assert(remappedChild != INVALID_NODE_ID);

            edge.parentNodeId = remappedParent;
            edge.childNodeId = remappedChild;
            remappedBatch.push_back(edge);

            if (remappedBatch.size() >= REMAP_BATCH_SIZE)
            {
                status = remappedStore.append(remappedBatch.data(), remappedBatch.size() * sizeof(PointerEdgeRecord));
                if (status != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append remapped edge batch");
                    return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
                }
                remappedBatch.clear();
            }
        }

        if (!remappedBatch.empty())
        {
            status = remappedStore.append(remappedBatch.data(), remappedBatch.size() * sizeof(PointerEdgeRecord));
            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_error("[PointerScanner] Failed to append final remapped edge batch");
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
            }
        }

        status = remappedStore.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to finalize remapped edge store");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        const std::size_t removedCount = nodeCount - newId;
        m_nodes = std::move(compactedNodes);
        m_edgesStore = std::move(remappedStore);
        m_nodesDiscovered.store(m_nodes.size(), std::memory_order_release);
        m_edgesCreated.store(m_edgeCount, std::memory_order_release);

        for (auto& shard : m_visitedShards)
        {
            std::scoped_lock lock{shard.mutex};
            shard.addressToNodeId.clear();
            shard.newNodeIds.clear();
            shard.pendingNodes.clear();
        }

        for (std::uint32_t i{}; i < m_nodes.size(); ++i)
        {
            const std::size_t shardIdx = (m_nodes[i].address >> 12) % VISITED_SHARD_COUNT;
            std::scoped_lock lock{m_visitedShards[shardIdx].mutex};
            m_visitedShards[shardIdx].addressToNodeId.insert_or_assign(m_nodes[i].address, i);
        }

        m_logService.log_info(fmt::format("[PointerScanner] Removed {} orphan nodes ({} -> {})",
                                          removedCount, nodeCount, m_nodes.size()));
        return StatusCode::STATUS_OK;
    }
} // namespace Vertex::Scanner
