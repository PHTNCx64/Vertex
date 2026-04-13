//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscanner.hh>

#include <algorithm>
#include <future>
#include <ranges>
#include <fmt/format.h>

namespace Vertex::Scanner
{
    template <class Visitor>
    void PointerScanner::range_query_index(const std::uint64_t low, const std::uint64_t high, Visitor&& visitor) const
    {
        if (m_mergedIndexEntryCount == 0 || m_mergedIndex.base() == nullptr)
        {
            return;
        }

        const auto* entries = static_cast<const ReverseIndexEntry*>(m_mergedIndex.base());
        const auto* begin = entries;
        const auto* end = entries + m_mergedIndexEntryCount;

        const auto* it = std::lower_bound(begin, end, low,
                                          [](const ReverseIndexEntry& entry, const std::uint64_t value)
                                          {
                                              return entry.pointerValue < value;
                                          });

        while (it != end && it->pointerValue <= high)
        {
            visitor(*it);
            ++it;
        }
    }

    StatusCode PointerScanner::run_bfs()
    {
        m_currentPhase.store(PointerScanPhase::BFSDepth, std::memory_order_release);

        if (m_mergedIndexEntryCount == 0)
        {
            m_logService.log_info("[PointerScanner] Empty index, skipping BFS");
            return StatusCode::STATUS_OK;
        }

        const auto reserveCount = std::min(m_config.maxNodes, INITIAL_NODE_RESERVE);
        m_nodes.reserve(reserveCount);

        const std::uint32_t targetModuleId = [this]() -> std::uint32_t
        {
            std::uint32_t bestId = NO_MODULE;
            std::uint64_t bestBase{};

            for (const auto& mod : m_modules)
            {
                if (m_config.targetAddress >= mod.moduleBase && mod.moduleBase >= bestBase)
                {
                    bestBase = mod.moduleBase;
                    bestId = mod.moduleId;
                }
            }

            return bestId;
        }();

        m_nodes.emplace_back(PointerNodeRecord{
            .address = m_config.targetAddress,
            .moduleId = targetModuleId,
            .minDepthDiscovered = 0,
            .flags = 0});
        m_nodesDiscovered.store(1, std::memory_order_release);

        {
            const std::size_t shardIdx = (m_config.targetAddress >> 12) % VISITED_SHARD_COUNT;
            std::scoped_lock lock{m_visitedShards[shardIdx].mutex};
            m_visitedShards[shardIdx].addressToNodeId.insert_or_assign(m_config.targetAddress, 0);
        }

        m_currentFrontier = {0};

        m_workerEdgeBuffers.resize(m_workerCount);
        m_edgeRuns.clear();
        m_edgesStore = {};
        m_edgeCount = 0;
        m_totalEdgeCountDuringBFS.store(0, std::memory_order_relaxed);
        const std::size_t flushThresholdMB = static_cast<std::size_t>(std::max(
            1, m_settingsService.get_int("pointerScan.edgeBufferFlushMB", 32)));
        m_edgeFlushThresholdEdges = std::max<std::size_t>(
            1, (flushThresholdMB * 1024ULL * 1024ULL) / sizeof(PointerEdgeRecord));

        for (std::uint32_t depth = 1; depth <= m_config.maxDepth; ++depth)
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
            }

            if (m_currentFrontier.empty())
            {
                m_logService.log_info(fmt::format("[PointerScanner] BFS: frontier empty at depth {}", depth));
                break;
            }

            m_currentDepth.store(static_cast<std::uint16_t>(depth), std::memory_order_relaxed);
            m_frontierSize.store(m_currentFrontier.size(), std::memory_order_relaxed);
            m_frontierNodesProcessed.store(0, std::memory_order_relaxed);

            m_logService.log_info(fmt::format("[PointerScanner] BFS depth {}/{}: frontier size {}",
                                              depth, m_config.maxDepth, m_currentFrontier.size()));

            const auto depthStartNodeCount = m_nodesDiscovered.load(std::memory_order_acquire);
            const StatusCode depthStatus = process_bfs_depth(static_cast<std::uint16_t>(depth), depthStartNodeCount);
            if (depthStatus != StatusCode::STATUS_OK)
            {
                return depthStatus;
            }

            for (std::size_t i{}; i < m_workerEdgeBuffers.size(); ++i)
            {
                const StatusCode flushStatus = flush_edge_buffer(i);
                if (flushStatus != StatusCode::STATUS_OK)
                {
                    return flushStatus;
                }
            }

            m_totalEdgeCountDuringBFS.store(0, std::memory_order_relaxed);

            const auto nodeCount = m_nodesDiscovered.load(std::memory_order_acquire);
            if (nodeCount > m_nodes.size())
            {
                m_nodes.resize(std::min(nodeCount, static_cast<std::uint64_t>(m_config.maxNodes)));
            }

            m_nextFrontier.clear();
            for (auto& shard : m_visitedShards)
            {
                std::scoped_lock lock{shard.mutex};
                for (std::size_t ni{}; ni < shard.newNodeIds.size(); ++ni)
                {
                    if (shard.newNodeIds[ni] < m_nodes.size())
                    {
                        m_nodes[shard.newNodeIds[ni]] = shard.pendingNodes[ni];
                    }
                }
                m_nextFrontier.insert(m_nextFrontier.end(), shard.newNodeIds.begin(), shard.newNodeIds.end());
                shard.newNodeIds.clear();
                shard.pendingNodes.clear();
            }

            m_currentFrontier = std::move(m_nextFrontier);
            m_nextFrontier.clear();

            m_edgesCreated.store(edge_count(), std::memory_order_release);

            if (m_nodeCapExceeded.exchange(false, std::memory_order_acq_rel))
            {
                m_logService.log_info(fmt::format("[PointerScanner] Max nodes exceeded: {} >= {}",
                                                  nodeCount, m_config.maxNodes));
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_NODES_EXCEEDED;
            }

            m_logService.log_info(fmt::format("[PointerScanner] Depth {} committed: nodes={}, edges={}, frontier_size={}",
                                              depth, nodeCount, edge_count(), m_currentFrontier.size()));

            if (nodeCount > m_config.maxNodes)
            {
                m_logService.log_info(fmt::format("[PointerScanner] Max nodes exceeded: {} > {}", nodeCount, m_config.maxNodes));
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_NODES_EXCEEDED;
            }

            if (edge_count() >= m_config.maxEdges)
            {
                m_logService.log_info(fmt::format("[PointerScanner] Max edges exceeded during depth {}: {} >= {}", depth, edge_count(), m_config.maxEdges));
                return StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_EDGES_EXCEEDED;
            }
        }

        m_edgesCreated.store(edge_count(), std::memory_order_release);
        m_logService.log_info(fmt::format("[PointerScanner] BFS complete: {} nodes, {} edges",
                                          m_nodesDiscovered.load(std::memory_order_relaxed), edge_count()));

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::process_bfs_depth(const std::uint16_t depth, const std::uint64_t depthStartNodeCount)
    {
        m_nextChunkIndex.store(0, std::memory_order_release);

        const std::size_t frontierSize = m_currentFrontier.size();
        const std::size_t chunkSize = m_config.workerChunkSize;
        const std::size_t totalChunks = (frontierSize + chunkSize - 1) / chunkSize;

        const auto remoteWorkerCount = static_cast<int>(m_workerCount > 1 ? m_workerCount - 1 : 0);
        m_activeWorkers.store(remoteWorkerCount, std::memory_order_release);

        for (std::size_t i = 1; i < m_workerCount; ++i)
        {
            std::packaged_task<StatusCode()> task{
                [this, depth, frontierSize, chunkSize, totalChunks, i, depthStartNodeCount]() -> StatusCode
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
                        const std::size_t chunkEnd = std::min(chunkStart + chunkSize, frontierSize);

                        const StatusCode chunkStatus = process_frontier_chunk(chunkStart, chunkEnd, depth, i, depthStartNodeCount);
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
            const std::size_t chunkEnd = std::min(chunkStart + chunkSize, frontierSize);

            const StatusCode inlineStatus = process_frontier_chunk(chunkStart, chunkEnd, depth, 0, depthStartNodeCount);
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

        return collect_worker_error();
    }

    std::uint64_t PointerScanner::get_estimated_edge_count() const noexcept
    {
        const auto committed = m_edgesCreated.load(std::memory_order_relaxed);
        const auto pending = m_totalEdgeCountDuringBFS.load(std::memory_order_relaxed);

        if (committed > std::numeric_limits<std::uint64_t>::max() - pending)
        {
            return std::numeric_limits<std::uint64_t>::max();
        }

        return committed + pending;
    }

    StatusCode PointerScanner::flush_edge_buffer(const std::size_t workerIndex)
    {
        if (workerIndex >= m_workerEdgeBuffers.size())
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        auto& edges = m_workerEdgeBuffers[workerIndex].edges;
        if (edges.empty())
        {
            return StatusCode::STATUS_OK;
        }

        const std::size_t bufferedEdgeCount = edges.size();
        std::ranges::sort(edges, [](const PointerEdgeRecord& a, const PointerEdgeRecord& b)
                          {
                              return PointerScanner::edge_less(a, b);
                          });
        const auto [removeBegin, removeEnd] = std::ranges::unique(edges, [](const PointerEdgeRecord& a, const PointerEdgeRecord& b)
                                                                  {
                                                                      return PointerScanner::edge_equal(a, b);
                                                                  });
        edges.erase(removeBegin, removeEnd);

        IO::ScanResultStore store{};
        StatusCode status = store.open();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to open edge run store for worker {}", workerIndex));
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        status = store.append(edges.data(), edges.size() * sizeof(PointerEdgeRecord));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to append edge run data for worker {}", workerIndex));
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        status = store.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to finalize edge run store for worker {}", workerIndex));
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        const std::size_t committedUniqueCount = edges.size();
        {
            std::scoped_lock lock{m_edgeRunMutex};
            if (m_edgeCount > std::numeric_limits<std::size_t>::max() - edges.size())
            {
                m_logService.log_error("[PointerScanner] Edge count overflow while flushing worker edge buffer");
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            m_edgeCount += edges.size();
            m_edgeRuns.emplace_back(EdgeRunMetadata{
                .store = std::move(store),
                .edgeCount = edges.size()});
        }

        m_edgesCreated.fetch_add(committedUniqueCount, std::memory_order_relaxed);
        m_totalEdgeCountDuringBFS.fetch_sub(static_cast<std::uint64_t>(bufferedEdgeCount), std::memory_order_relaxed);

        edges.clear();
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::process_frontier_chunk(const std::size_t chunkStart, const std::size_t chunkEnd,
                                                      const std::uint16_t depth, const std::size_t workerIndex,
                                                      const std::uint64_t depthStartNodeCount)
    {
        if (workerIndex >= m_workerEdgeBuffers.size())
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        auto& [edges] = m_workerEdgeBuffers[workerIndex];
        const std::size_t flushThreshold = std::max<std::size_t>(1, m_edgeFlushThresholdEdges);

        const auto nodeCount = m_nodesDiscovered.load(std::memory_order_acquire);
        constexpr std::uint64_t MAX_TRACKABLE_NODES =
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + std::uint64_t{1};
        const std::uint64_t maxNodeCount = std::min<std::uint64_t>(m_config.maxNodes, MAX_TRACKABLE_NODES);

        for (std::size_t idx = chunkStart; idx < chunkEnd; ++idx)
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
            }

            if (idx >= m_currentFrontier.size())
            {
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            const std::uint32_t childNodeId = m_currentFrontier[idx];
            if (childNodeId >= nodeCount)
            {
                return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
            }

            const std::uint64_t childAddress = m_nodes[childNodeId].address;

            std::uint64_t queryLow{};
            std::uint64_t queryHigh{};

            if (m_config.allowNegativeOffsets)
            {
                queryLow = childAddress > m_config.maxOffset ? childAddress - m_config.maxOffset : 0;
                queryHigh = childAddress > std::numeric_limits<std::uint64_t>::max() - m_config.maxOffset
                            ? std::numeric_limits<std::uint64_t>::max()
                            : childAddress + m_config.maxOffset;
            }
            else
            {
                queryLow = childAddress > m_config.maxOffset ? childAddress - m_config.maxOffset : 0;
                queryHigh = childAddress;
            }

            std::uint32_t parentsForChild{};
            StatusCode flushStatus{StatusCode::STATUS_OK};

            range_query_index(queryLow, queryHigh, [&](const ReverseIndexEntry& entry)
                              {
                                  if (flushStatus != StatusCode::STATUS_OK)
                                  {
                                      return;
                                  }

                                  if (parentsForChild >= m_config.maxParentsPerNode)
                                  {
                                      return;
                                  }

                                  if (get_estimated_edge_count() >= m_config.maxEdges)
                                  {
                                      return;
                                  }

                                  const auto offset = static_cast<std::int64_t>(childAddress) - static_cast<std::int64_t>(entry.pointerValue);

                                  if (!m_config.allowNegativeOffsets && offset < 0)
                                  {
                                      return;
                                  }

                                  if (offset > static_cast<std::int64_t>(m_config.maxOffset) ||
                                      offset < -static_cast<std::int64_t>(m_config.maxOffset))
                                  {
                                      return;
                                  }

                                  if (offset > std::numeric_limits<std::int32_t>::max() ||
                                      offset < std::numeric_limits<std::int32_t>::min())
                                  {
                                      return;
                                  }

                                  const std::size_t shard = (entry.sourceAddress >> 12) % VISITED_SHARD_COUNT;

                                  std::uint32_t parentNodeId{};
                                  bool newlyInserted{};
                                  bool capExceeded{};

                                  {
                                      auto& [mutex, addressToNodeId, newNodeIds, pendingNodes] = m_visitedShards[shard];
                                      std::scoped_lock lock{mutex};

                                      if (const auto* existing = addressToNodeId.find(entry.sourceAddress);
                                          existing != nullptr)
                                      {
                                          parentNodeId = *existing;
                                      }
                                      else
                                      {
                                          auto discovered = m_nodesDiscovered.load(std::memory_order_relaxed);
                                          while (true)
                                          {
                                              if (discovered >= maxNodeCount)
                                              {
                                                  m_nodeCapExceeded.store(true, std::memory_order_release);
                                                  capExceeded = true;
                                                  break;
                                              }

                                              if (m_nodesDiscovered.compare_exchange_weak(
                                                      discovered, discovered + 1,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_relaxed))
                                              {
                                                  parentNodeId = static_cast<std::uint32_t>(discovered);
                                                  addressToNodeId.insert_or_assign(entry.sourceAddress, parentNodeId);
                                                  newNodeIds.push_back(parentNodeId);
                                                  pendingNodes.emplace_back(PointerNodeRecord{
                                                      .address = entry.sourceAddress,
                                                      .moduleId = entry.moduleId,
                                                      .minDepthDiscovered = depth,
                                                      .flags = 0});
                                                  newlyInserted = true;
                                                  break;
                                              }
                                          }

                                      }
                                  }

                                  if (capExceeded)
                                  {
                                      return;
                                  }

                                  if (static_cast<std::uint64_t>(parentNodeId) >= maxNodeCount)
                                  {
                                      return;
                                  }

                                  if (!newlyInserted && parentNodeId < depthStartNodeCount)
                                  {
                                      return;
                                  }

                                  if (!m_config.offsetEndingFilters.empty())
                                  {
                                      const auto absOffset = static_cast<std::int32_t>(
                                          offset < 0 ? -offset : offset);
                                      const bool matches = std::ranges::any_of(
                                          m_config.offsetEndingFilters,
                                          [absOffset](const OffsetEndingFilter& filter)
                                          {
                                              return (absOffset & filter.mask) == filter.value;
                                          });
                                      if (!matches)
                                      {
                                          return;
                                      }
                                  }

                                  edges.emplace_back(PointerEdgeRecord{
                                      .parentNodeId = parentNodeId,
                                      .childNodeId = childNodeId,
                                      .offset = static_cast<std::int32_t>(offset),
                                      .flags = 0});

                                  m_totalEdgeCountDuringBFS.fetch_add(1, std::memory_order_relaxed);
                                  if (edges.size() >= flushThreshold)
                                  {
                                      flushStatus = flush_edge_buffer(workerIndex);
                                      if (flushStatus != StatusCode::STATUS_OK)
                                      {
                                          return;
                                      }
                                  }
                                  ++parentsForChild;
                              });

            if (flushStatus != StatusCode::STATUS_OK)
            {
                return flushStatus;
            }

            m_frontierNodesProcessed.fetch_add(1, std::memory_order_relaxed);
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
