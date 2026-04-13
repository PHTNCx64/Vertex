//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscanner.hh>

#include <algorithm>
#include <chrono>
#include <future>
#include <ranges>
#include <thread>
#include <fmt/format.h>
#include <vertex/runtime/caller.hh>

namespace Vertex::Scanner
{
    PointerScanner::PointerScanner(Configuration::ISettings& settingsService, Log::ILog& logService, Thread::IThreadDispatcher& dispatcher, Runtime::ILoader& loaderService)
        : m_settingsService{settingsService},
          m_logService{logService},
          m_dispatcher{dispatcher},
          m_loaderService{loaderService}
    {
    }

    PointerScanner::~PointerScanner()
    {
        m_scanAbort.store(true, std::memory_order_seq_cst);

        {
            std::unique_lock lock{m_completionMutex};
            m_completionCondition.wait_for(lock, std::chrono::milliseconds{10000},
                                           [this]
                                           {
                                               return m_scanComplete.load(std::memory_order_acquire);
                                           });
        }

        [[maybe_unused]] const StatusCode status = m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::PointerScanner);
    }

    void PointerScanner::set_memory_reader(std::shared_ptr<IMemoryReader> reader)
    {
        std::scoped_lock lock{m_memoryReaderMutex};
        m_memoryReader = std::move(reader);
    }

    bool PointerScanner::has_memory_reader() const
    {
        std::scoped_lock lock{m_memoryReaderMutex};
        return m_memoryReader != nullptr;
    }

    StatusCode PointerScanner::initialize_scan(const PointerScanConfig& config, const std::vector<ScanRegion>& memoryRegions)
    {
        std::scoped_lock lifecycleLock{m_scanLifecycleMutex};

        if (!drain_active_scan())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        if (memoryRegions.empty())
        {
            m_logService.log_error("[PointerScanner] No memory regions provided");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (!has_memory_reader())
        {
            m_logService.log_error("[PointerScanner] No memory reader available");
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        if (config.targetAddress == 0)
        {
            m_logService.log_error("[PointerScanner] Target address is zero");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (config.alignment == 0 || config.workerChunkSize == 0)
        {
            m_logService.log_error("[PointerScanner] alignment and workerChunkSize must be non-zero");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (config.maxDepth == 0 || config.maxOffset == 0)
        {
            m_logService.log_error("[PointerScanner] maxDepth and maxOffset must be non-zero");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (config.maxOffset > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            m_logService.log_error("[PointerScanner] maxOffset exceeds int32 range for edge storage");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            m_logService.log_error("[PointerScanner] No active plugin to retrieve pointer size");
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        std::uint64_t pointerSize{};
        const auto& pluginRef = pluginOpt.value().get();
        const auto pointerSizeStatus = Runtime::safe_call(pluginRef.internal_vertex_memory_get_process_pointer_size, &pointerSize);
        const StatusCode resolvedStatus = Runtime::get_status(pointerSizeStatus);
        if (resolvedStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to retrieve pointer size: {}", static_cast<int>(resolvedStatus)));
            return resolvedStatus;
        }

        if (pointerSize != 4 && pointerSize != 8)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Invalid pointer size: {}. Expected 4 (32-bit) or 8 (64-bit)", pointerSize));
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto pendingSignature = compute_index_signature(pointerSize, config.alignment, config.scanContextHash, memoryRegions);

        if (can_reuse_index(pendingSignature))
        {
            reset_bfs_state();
            m_logService.log_info("[PointerScanner] Reusing cached reverse index");
        }
        else
        {
            reset_state();
            build_module_table(memoryRegions);
            m_pendingIndexSignature = pendingSignature;
        }

        m_config = config;
        m_pointerSize = pointerSize;

        const int hwDefault = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
        const int configuredThreads = m_settingsService.get_int("pointerScan.readerThreads", hwDefault);
        const int readerThreads = m_dispatcher.is_single_threaded() ? 1 : configuredThreads;
        const auto workerCount = static_cast<std::size_t>(std::max(1, readerThreads));

        m_logService.log_info(fmt::format("[PointerScanner] Starting scan: target=0x{:X}, maxDepth={}, maxOffset=0x{:X}, workers={}, indexReuse={}",
                                          config.targetAddress, config.maxDepth, config.maxOffset, workerCount, m_indexValid));

        StatusCode status = create_worker_pool(workerCount);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to create worker pool: {}", static_cast<int>(status)));
            return status;
        }

        m_scanComplete.store(false, std::memory_order_release);

        auto regionsCopy = memoryRegions;
        std::packaged_task<StatusCode()> coordinatorTask{
            [this, regions = std::move(regionsCopy)]() -> StatusCode
            {
                return run_coordinator(regions);
            }};

        status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::PointerScanner, 0, std::move(coordinatorTask));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to enqueue coordinator task");
            m_scanComplete.store(true, std::memory_order_release);
            return status;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::initialize_rescan()
    {
        std::scoped_lock lifecycleLock{m_scanLifecycleMutex};

        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        if (m_nodes.empty() || edges_empty())
        {
            m_logService.log_error("[PointerScanner] No graph data available for rescan");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NO_GRAPH;
        }

        if (!has_memory_reader())
        {
            m_logService.log_error("[PointerScanner] No memory reader available for rescan");
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        m_scanAbort.store(false, std::memory_order_seq_cst);
        m_workerError.store(static_cast<std::int32_t>(StatusCode::STATUS_OK), std::memory_order_relaxed);
        m_edgesValidated.store(0, std::memory_order_relaxed);
        m_totalEdgesToValidate.store(edge_count(), std::memory_order_relaxed);
        m_scanStatus.store(StatusCode::STATUS_OK, std::memory_order_relaxed);

        const int rescanHwDefault = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
        const int rescanConfiguredThreads = m_settingsService.get_int("pointerScan.readerThreads", rescanHwDefault);
        const int rescanReaderThreads = m_dispatcher.is_single_threaded() ? 1 : rescanConfiguredThreads;
        const auto workerCount = static_cast<std::size_t>(std::max(1, rescanReaderThreads));

        StatusCode status = create_worker_pool(workerCount);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to create worker pool for rescan: {}", static_cast<int>(status)));
            return status;
        }

        m_logService.log_info(fmt::format("[PointerScanner] Starting rescan: {} edges to validate, {} workers",
                                          edge_count(), workerCount));

        m_scanComplete.store(false, std::memory_order_release);

        std::packaged_task<StatusCode()> coordinatorTask{
            [this]() -> StatusCode
            {
                return run_rescan_coordinator();
            }};

        status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::PointerScanner, 0, std::move(coordinatorTask));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to enqueue rescan coordinator task");
            m_scanComplete.store(true, std::memory_order_release);
            return status;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::run_coordinator(const std::vector<ScanRegion>& regions)
    {
        StatusCode status{StatusCode::STATUS_OK};

        if (m_indexValid)
        {
            m_logService.log_info(fmt::format("[PointerScanner] Skipping index build (cached: {} entries)",
                                              m_mergedIndexEntryCount));
        }
        else
        {
            status = build_reverse_index(regions);
            if (status != StatusCode::STATUS_OK || m_scanAbort.load(std::memory_order_acquire))
            {
                m_logService.log_info(fmt::format("[PointerScanner] Index build ended: status={}, aborted={}",
                                                  static_cast<int>(status), m_scanAbort.load(std::memory_order_relaxed)));
                m_currentPhase.store(PointerScanPhase::Idle, std::memory_order_release);
                m_scanStatus.store(status, std::memory_order_release);
                m_scanComplete.store(true, std::memory_order_release);
                std::scoped_lock lock{m_completionMutex};
                m_completionCondition.notify_all();
                return status;
            }

            status = merge_index_runs();
            if (status != StatusCode::STATUS_OK || m_scanAbort.load(std::memory_order_acquire))
            {
                m_logService.log_info(fmt::format("[PointerScanner] Index merge ended: status={}, aborted={}",
                                                  static_cast<int>(status), m_scanAbort.load(std::memory_order_relaxed)));
                m_currentPhase.store(PointerScanPhase::Idle, std::memory_order_release);
                m_scanStatus.store(status, std::memory_order_release);
                m_scanComplete.store(true, std::memory_order_release);
                std::scoped_lock lock{m_completionMutex};
                m_completionCondition.notify_all();
                return status;
            }

            m_cachedIndexSignature = m_pendingIndexSignature;
            m_indexValid = true;

            m_logService.log_info(fmt::format("[PointerScanner] Index built and cached: {} entries",
                                              m_mergedIndexEntryCount));
        }

        status = run_bfs();
        const bool capExceeded = status == StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_NODES_EXCEEDED ||
                                 status == StatusCode::STATUS_ERROR_POINTER_SCAN_MAX_EDGES_EXCEEDED;
        if (!capExceeded && (status != StatusCode::STATUS_OK || m_scanAbort.load(std::memory_order_acquire)))
        {
            m_logService.log_info(fmt::format("[PointerScanner] BFS ended: status={}, aborted={}",
                                              static_cast<int>(status), m_scanAbort.load(std::memory_order_relaxed)));
            m_currentPhase.store(PointerScanPhase::Idle, std::memory_order_release);
            m_scanStatus.store(status, std::memory_order_release);
            m_scanComplete.store(true, std::memory_order_release);
            std::scoped_lock lock{m_completionMutex};
            m_completionCondition.notify_all();
            return status;
        }

        const StatusCode bfsStatus = status;
        status = finalize_graph();

        m_nodesDiscovered.store(m_nodes.size(), std::memory_order_release);
        m_edgesCreated.store(edge_count(), std::memory_order_release);

        m_logService.log_info(fmt::format("[PointerScanner] Scan complete: nodes={}, edges={}, roots={}",
                                          m_nodes.size(), edge_count(), m_rootNodeIds.size()));

        m_currentPhase.store(PointerScanPhase::Idle, std::memory_order_release);
        const StatusCode finalStatus = status != StatusCode::STATUS_OK ? status : bfsStatus;
        m_scanStatus.store(finalStatus, std::memory_order_release);
        m_scanComplete.store(true, std::memory_order_release);
        std::scoped_lock lock{m_completionMutex};
        m_completionCondition.notify_all();

        return finalStatus;
    }

    StatusCode PointerScanner::stop_scan()
    {
        m_scanAbort.store(true, std::memory_order_release);
        return StatusCode::STATUS_OK;
    }

    void PointerScanner::finalize_scan()
    {
        std::scoped_lock lifecycleLock{m_scanLifecycleMutex};
        if (!drain_active_scan())
        {
            m_logService.log_error("[PointerScanner] finalize_scan aborted: coordinator did not drain in time");
            return;
        }
        StatusCode status = m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::PointerScanner);
        m_workerCount = 0;
    }

    bool PointerScanner::is_scan_complete()
    {
        return m_scanComplete.load(std::memory_order_acquire);
    }

    StatusCode PointerScanner::get_scan_status() const noexcept
    {
        return m_scanStatus.load(std::memory_order_acquire);
    }

    PointerScanProgress PointerScanner::get_progress() const noexcept
    {
        const auto phase = m_currentPhase.load(std::memory_order_acquire);

        PointerScanProgress progress{.phase = phase, .maxDepth = m_config.maxDepth};

        switch (phase)
        {
            case PointerScanPhase::IndexBuild:
                progress.current = m_regionsScanned.load(std::memory_order_relaxed);
                progress.total = m_totalRegions.load(std::memory_order_relaxed);
                break;
            case PointerScanPhase::IndexMergeSort:
                progress.current = m_regionsScanned.load(std::memory_order_relaxed);
                progress.total = m_totalRegions.load(std::memory_order_relaxed);
                break;
            case PointerScanPhase::BFSDepth:
                progress.current = m_frontierNodesProcessed.load(std::memory_order_relaxed);
                progress.total = m_frontierSize.load(std::memory_order_relaxed);
                progress.currentDepth = m_currentDepth.load(std::memory_order_relaxed);
                break;
            case PointerScanPhase::FinalizeGraph:
                progress.current = m_edgesCreated.load(std::memory_order_relaxed);
                progress.total = m_edgesCreated.load(std::memory_order_relaxed);
                break;
            case PointerScanPhase::Rescan:
                progress.current = m_edgesValidated.load(std::memory_order_relaxed);
                progress.total = m_totalEdgesToValidate.load(std::memory_order_relaxed);
                break;
            case PointerScanPhase::Idle:
                break;
        }

        return progress;
    }

    std::uint64_t PointerScanner::get_node_count() const noexcept
    {
        return m_nodesDiscovered.load(std::memory_order_acquire);
    }

    std::uint64_t PointerScanner::get_edge_count() const noexcept
    {
        return m_edgesCreated.load(std::memory_order_acquire);
    }

    const PointerEdgeRecord* PointerScanner::edge_data() const noexcept
    {
        if (m_edgeCount == 0)
        {
            return nullptr;
        }

        return static_cast<const PointerEdgeRecord*>(m_edgesStore.base());
    }

    std::size_t PointerScanner::edge_count() const noexcept
    {
        return m_edgeCount;
    }

    bool PointerScanner::edges_empty() const noexcept
    {
        return m_edgeCount == 0;
    }

    const ParentEdgeRange* PointerScanner::parent_range_data() const noexcept
    {
        if (m_parentRangeCount == 0)
        {
            return nullptr;
        }

        return static_cast<const ParentEdgeRange*>(m_parentRangesStore.base());
    }

    std::size_t PointerScanner::parent_range_count() const noexcept
    {
        return m_parentRangeCount;
    }

    bool PointerScanner::edge_less(const PointerEdgeRecord& a, const PointerEdgeRecord& b) noexcept
    {
        if (a.parentNodeId != b.parentNodeId)
        {
            return a.parentNodeId < b.parentNodeId;
        }
        if (a.childNodeId != b.childNodeId)
        {
            return a.childNodeId < b.childNodeId;
        }
        return a.offset < b.offset;
    }

    bool PointerScanner::edge_equal(const PointerEdgeRecord& a, const PointerEdgeRecord& b) noexcept
    {
        return a.parentNodeId == b.parentNodeId &&
               a.childNodeId == b.childNodeId &&
               a.offset == b.offset;
    }

    std::uint64_t PointerScanner::get_root_count() const noexcept
    {
        return m_rootNodeIds.size();
    }

    StatusCode PointerScanner::get_root_node_ids(std::vector<std::uint32_t>& nodeIds, const std::size_t startIndex, const std::size_t count) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        if (startIndex >= m_rootNodeIds.size())
        {
            nodeIds.clear();
            return StatusCode::STATUS_OK;
        }

        const std::size_t available = std::min(count, m_rootNodeIds.size() - startIndex);
        nodeIds.assign(m_rootNodeIds.begin() + static_cast<std::ptrdiff_t>(startIndex),
                       m_rootNodeIds.begin() + static_cast<std::ptrdiff_t>(startIndex + available));
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::get_roots_range(std::vector<PointerNodeRecord>& roots, const std::size_t startIndex, const std::size_t count) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        if (startIndex >= m_rootNodeIds.size())
        {
            roots.clear();
            return StatusCode::STATUS_OK;
        }

        const std::size_t available = std::min(count, m_rootNodeIds.size() - startIndex);
        roots.resize(available);

        for (std::size_t i{}; i < available; ++i)
        {
            roots[i] = m_nodes[m_rootNodeIds[startIndex + i]];
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::get_children(const std::uint32_t nodeId, std::vector<PointerEdgeRecord>& edges,
                                            const std::size_t startIndex, const std::size_t count) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        if (nodeId >= parent_range_count())
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        const auto* ranges = parent_range_data();
        if (ranges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        const auto& [firstEdgeIndex, edgeCount] = ranges[nodeId];

        if (startIndex >= edgeCount)
        {
            edges.clear();
            return StatusCode::STATUS_OK;
        }

        const std::size_t available = std::min(count, static_cast<std::size_t>(edgeCount) - startIndex);

        const std::size_t requestedEnd = static_cast<std::size_t>(firstEdgeIndex) + startIndex + available;
        if (requestedEnd > edge_count())
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        edges.resize(available);
        const auto* allEdges = edge_data();
        if (allEdges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        for (std::size_t i{}; i < available; ++i)
        {
            edges[i] = allEdges[static_cast<std::size_t>(firstEdgeIndex) + startIndex + i];
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::get_node(const std::uint32_t nodeId, PointerNodeRecord& node) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        if (nodeId >= m_nodes.size())
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        node = m_nodes[nodeId];
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::get_nodes_range(std::vector<PointerNodeRecord>& nodes,
                                               const std::size_t startIndex,
                                               const std::size_t count) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        if (startIndex >= m_nodes.size() || count == 0)
        {
            nodes.clear();
            return StatusCode::STATUS_OK;
        }

        const auto available = std::min(count, m_nodes.size() - startIndex);
        nodes.assign(
            m_nodes.begin() + static_cast<std::ptrdiff_t>(startIndex),
            m_nodes.begin() + static_cast<std::ptrdiff_t>(startIndex + available));
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::get_edges_range(std::vector<PointerEdgeRecord>& edges,
                                               const std::size_t startIndex,
                                               const std::size_t count) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        const auto totalEdges = edge_count();
        if (startIndex >= totalEdges || count == 0)
        {
            edges.clear();
            return StatusCode::STATUS_OK;
        }

        const auto* allEdges = edge_data();
        if (allEdges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        const auto available = std::min(count, totalEdges - startIndex);
        edges.resize(available);
        std::copy_n(allEdges + startIndex, available, edges.begin());
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::get_parent_ranges(std::vector<ParentEdgeRange>& ranges,
                                                 const std::size_t startIndex,
                                                 const std::size_t count) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        const auto totalRanges = parent_range_count();
        if (startIndex >= totalRanges || count == 0)
        {
            ranges.clear();
            return StatusCode::STATUS_OK;
        }

        const auto* allRanges = parent_range_data();
        if (allRanges == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        const auto available = std::min(count, totalRanges - startIndex);
        ranges.resize(available);
        std::copy_n(allRanges + startIndex, available, ranges.begin());
        return StatusCode::STATUS_OK;
    }

    const std::vector<ModuleRecord>& PointerScanner::get_modules() const noexcept
    {
        return m_modules;
    }

    void PointerScanner::wait_for_scan_completion()
    {
        std::unique_lock lock{m_completionMutex};
        m_completionCondition.wait_for(lock, std::chrono::milliseconds{30000},
                                       [this]
                                       {
                                           return m_scanComplete.load(std::memory_order_acquire);
                                       });
    }

    bool PointerScanner::drain_active_scan()
    {
        if (m_scanComplete.load(std::memory_order_acquire))
        {
            return true;
        }

        m_scanAbort.store(true, std::memory_order_seq_cst);
        std::unique_lock lock{m_completionMutex};
        const bool drained = m_completionCondition.wait_for(lock, std::chrono::milliseconds{10000},
                                                            [this]
                                                            {
                                                                return m_scanComplete.load(std::memory_order_acquire);
                                                            });

        if (!drained)
        {
            m_logService.log_error("[PointerScanner] drain_active_scan timed out");
        }

        return drained;
    }

    void PointerScanner::wait_for_remote_workers()
    {
        std::unique_lock lock{m_completionMutex};
        m_completionCondition.wait(lock,
                                   [this]
                                   {
                                       return m_activeWorkers.load(std::memory_order_acquire) == 0;
                                   });
    }

    void PointerScanner::report_worker_error(const StatusCode status)
    {
        auto expected = static_cast<std::int32_t>(StatusCode::STATUS_OK);
        m_workerError.compare_exchange_strong(expected, static_cast<std::int32_t>(status), std::memory_order_acq_rel);
    }

    StatusCode PointerScanner::collect_worker_error()
    {
        return static_cast<StatusCode>(m_workerError.exchange(static_cast<std::int32_t>(StatusCode::STATUS_OK), std::memory_order_acq_rel));
    }

    StatusCode PointerScanner::create_worker_pool(const std::size_t workerCount)
    {
        StatusCode destroyStatus = m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::PointerScanner);
        if (destroyStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to destroy existing worker pool: {}", static_cast<int>(destroyStatus)));
            return destroyStatus;
        }
        m_workerCount = workerCount;

        const StatusCode status = m_dispatcher.create_worker_pool(Thread::ThreadChannel::PointerScanner, workerCount);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to create worker pool: {}", static_cast<int>(status)));
            m_workerCount = 0;
        }

        return status;
    }

    void PointerScanner::reset_bfs_state()
    {
        m_scanAbort.store(false, std::memory_order_seq_cst);
        m_scanComplete.store(true, std::memory_order_seq_cst);
        m_regionsScanned.store(0, std::memory_order_relaxed);
        m_totalRegions.store(0, std::memory_order_relaxed);
        m_nodesDiscovered.store(0, std::memory_order_relaxed);
        m_edgesCreated.store(0, std::memory_order_relaxed);
        m_frontierNodesProcessed.store(0, std::memory_order_relaxed);
        m_frontierSize.store(0, std::memory_order_relaxed);
        m_nextChunkIndex.store(0, std::memory_order_relaxed);
        m_activeWorkers.store(0, std::memory_order_relaxed);
        m_workerError.store(static_cast<std::int32_t>(StatusCode::STATUS_OK), std::memory_order_relaxed);
        m_edgesValidated.store(0, std::memory_order_relaxed);
        m_totalEdgesToValidate.store(0, std::memory_order_relaxed);
        m_nodeCapExceeded.store(false, std::memory_order_relaxed);
        m_currentPhase.store(PointerScanPhase::Idle, std::memory_order_relaxed);
        m_currentDepth.store(0, std::memory_order_relaxed);
        m_scanStatus.store(StatusCode::STATUS_OK, std::memory_order_relaxed);

        m_config = {};

        m_nodes.clear();
        m_edgeRuns.clear();
        m_edgesStore = {};
        m_edgeCount = 0;
        m_parentRangesStore = {};
        m_parentRangeCount = 0;
        m_rootNodeIds.clear();

        for (auto& shard : m_visitedShards)
        {
            std::scoped_lock lock{shard.mutex};
            shard.addressToNodeId.clear();
            shard.newNodeIds.clear();
            shard.pendingNodes.clear();
        }

        m_workerEdgeBuffers.clear();
        m_totalEdgeCountDuringBFS.store(0, std::memory_order_relaxed);
        m_edgeFlushThresholdEdges = 0;
        m_currentFrontier.clear();
        m_nextFrontier.clear();
    }

    void PointerScanner::reset_state()
    {
        reset_bfs_state();

        m_modules.clear();
        m_moduleNameToId.clear();

        m_workerIndexRuns.clear();
        m_workerIndexBuffers.clear();
        m_mergedIndex = {};
        m_mergedIndexEntryCount = 0;

        m_cachedIndexSignature = std::nullopt;
        m_indexValid = false;
    }

    bool PointerScanner::can_reuse_index(const IndexSignature& newSignature) const
    {
        return m_indexValid &&
               m_cachedIndexSignature.has_value() &&
               *m_cachedIndexSignature == newSignature;
    }

    IndexSignature PointerScanner::compute_index_signature(
        const std::uint64_t pointerSize,
        const std::uint32_t alignment,
        const std::uint64_t contextHash,
        const std::vector<ScanRegion>& regions)
    {
        constexpr std::uint64_t FNV_OFFSET_BASIS{14695981039346656037ULL};
        constexpr std::uint64_t FNV_PRIME{1099511628211ULL};

        const auto fnv_mix = [](std::uint64_t hash, const std::uint64_t value) -> std::uint64_t
        {
            hash ^= value;
            hash *= FNV_PRIME;
            return hash;
        };

        std::vector<std::uint64_t> regionHashes{};
        regionHashes.reserve(regions.size());

        for (const auto& region : regions)
        {
            std::uint64_t h{FNV_OFFSET_BASIS};
            h = fnv_mix(h, region.baseAddress);
            h = fnv_mix(h, region.size);

            for (const auto ch : region.moduleName)
            {
                h = fnv_mix(h, static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
            }

            regionHashes.push_back(h);
        }

        std::ranges::sort(regionHashes);

        std::uint64_t combined{FNV_OFFSET_BASIS};
        for (const auto rh : regionHashes)
        {
            combined = fnv_mix(combined, rh);
        }

        return IndexSignature{
            .pointerSize = pointerSize,
            .alignment = alignment,
            .regionHash = combined,
            .contextHash = contextHash};
    }

    void PointerScanner::build_module_table(const std::vector<ScanRegion>& regions)
    {
        m_modules.clear();
        m_moduleNameToId.clear();

        const auto saturated_end = [](const std::uint64_t base, const std::uint64_t size) -> std::uint64_t
        {
            if (size > std::numeric_limits<std::uint64_t>::max() - base)
            {
                return std::numeric_limits<std::uint64_t>::max();
            }
            return base + size;
        };

        for (const auto& region : regions)
        {
            if (region.moduleName.empty())
            {
                continue;
            }

            auto [it, inserted] = m_moduleNameToId.try_emplace(region.moduleName, static_cast<std::uint32_t>(m_modules.size()));

            if (inserted)
            {
                m_modules.emplace_back(ModuleRecord{
                    .moduleId = it->second,
                    .moduleBase = region.baseAddress,
                    .moduleSpan = region.size,
                    .moduleName = region.moduleName});
            }
            else
            {
                auto& existing = m_modules[it->second];
                const auto currentEnd = saturated_end(existing.moduleBase, existing.moduleSpan);
                const auto regionEnd = saturated_end(region.baseAddress, region.size);
                const auto mergedBase = std::min(existing.moduleBase, region.baseAddress);
                const auto mergedEnd = std::max(currentEnd, regionEnd);

                existing.moduleBase = mergedBase;
                existing.moduleSpan = mergedEnd - mergedBase;
            }
        }

        m_logService.log_info(fmt::format("[PointerScanner] Module table: {} modules", m_modules.size()));
    }

    std::uint32_t PointerScanner::resolve_module_id(const std::string_view moduleName) const
    {
        if (moduleName.empty())
        {
            return NO_MODULE;
        }

        const auto it = m_moduleNameToId.find(std::string{moduleName});
        if (it == m_moduleNameToId.end())
        {
            return NO_MODULE;
        }

        return it->second;
    }
} // namespace Vertex::Scanner
