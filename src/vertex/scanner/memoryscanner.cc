//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <array>
#include <bit>
#include <future>
#include <chrono>
#include <algorithm>
#include <new>
#include <fmt/format.h>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/comparators.hh>
#include <vertex/memory/scannerallocator.hh>

namespace Vertex::Scanner
{
    MemoryScanner::MemoryScanner(Configuration::ISettings& settingsService, Log::ILog& logService, Thread::IThreadDispatcher& dispatcher)
        : m_settingsService(settingsService),
          m_logService(logService),
          m_dispatcher(dispatcher)
    {
    }

    MemoryScanner::~MemoryScanner()
    {
        m_scanAbort.store(true, std::memory_order_seq_cst);

        {
            std::unique_lock lock(m_mainThreadMutex);
            m_mainThreadWaitCondition.wait_for(lock, std::chrono::milliseconds(10000),
                                               [this]
                                               {
                                                   return is_scan_complete();
                                               });
        }

        if (Memory::has_thread_memory_context())
        {
            Memory::cleanup_thread_memory_context();
        }

        std::ignore = m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::Scanner);

        {
            std::scoped_lock lock(m_writerRegionsMutex);
            cleanup_writer_regions(m_writerRegions);
        }

        {
            std::scoped_lock undoLock(m_undoHistoryMutex);
            for (auto& snapshot : m_undoHistory)
            {
                cleanup_snapshot_regions(snapshot);
            }
            m_undoHistory.clear();
        }
    }

    void MemoryScanner::set_memory_reader(std::shared_ptr<IMemoryReader> reader)
    {
        std::scoped_lock lock(m_memoryReaderMutex);
        m_memoryReader = std::move(reader);
    }

    void MemoryScanner::set_scan_completion_callback(std::move_only_function<void()> callback)
    {
        std::scoped_lock lock(m_scanCompletionCallbackMutex);
        m_scanCompletionCallback = std::move(callback);
    }

    void MemoryScanner::set_scan_progress_callback(std::move_only_function<void()> callback)
    {
        std::scoped_lock lock(m_scanProgressCallbackMutex);
        m_scanProgressCallback = std::move(callback);
    }

    bool MemoryScanner::has_memory_reader() const
    {
        std::scoped_lock lock(m_memoryReaderMutex);
        return m_memoryReader != nullptr;
    }

    StatusCode MemoryScanner::initialize_scan(const ScanConfiguration& configuration, std::shared_ptr<const TypeSchema> schema, const std::vector<ScanRegion>& memoryRegions)
    {
        std::scoped_lock lifecycleLock(m_scanLifecycleMutex);
        if (!drain_active_scan())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        if (!schema)
        {
            m_logService.log_error("[Scanner] initialize_scan called with null TypeSchema");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (schema->kind == TypeKind::PluginDefined)
        {
            if (!schema->sdkType || !schema->sdkType->extractor || schema->valueSize == 0 ||
                schema->sdkType->scanModeCount == 0 || schema->sdkType->scanModes == nullptr)
            {
                m_logService.log_error("[Scanner] PluginDefined schema is malformed");
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (configuration.scanMode >= schema->sdkType->scanModeCount)
            {
                m_logService.log_error("[Scanner] PluginDefined scanMode index out of range");
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (!schema->sdkType->scanModes[configuration.scanMode].comparator)
            {
                m_logService.log_error("[Scanner] PluginDefined comparator is null");
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
        }

        m_activeSchema = schema;

        m_logService.log_info(fmt::format("[Scanner] initialize_scan: {} regions", memoryRegions.size()));

        if (memoryRegions.empty())
        {
            m_logService.log_error("[Scanner] No memory regions provided");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (!has_memory_reader())
        {
            m_logService.log_error("[Scanner] No memory reader available");
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        m_scanAbort.store(false, std::memory_order_seq_cst);
        m_pluginCallStatus.store(StatusCode::STATUS_OK, std::memory_order_release);

        {
            std::scoped_lock undoLock(m_undoHistoryMutex);
            for (auto& snapshot : m_undoHistory)
            {
                cleanup_snapshot_regions(snapshot);
            }
            m_undoHistory.clear();
        }

        m_scanConfig = configuration;

        if (schema->kind == TypeKind::PluginDefined)
        {
            m_scanConfig.dataSize = schema->valueSize;
        }
        else if (is_string_type(m_scanConfig.valueType))
        {
            m_scanConfig.dataSize = m_scanConfig.input.size();
        }
        else
        {
            m_scanConfig.dataSize = get_value_size(m_scanConfig.valueType);
        }

        if (m_scanConfig.dataSize == 0)
        {
            m_logService.log_error("[Scanner] dataSize is 0");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (schema->kind != TypeKind::PluginDefined &&
            !is_string_type(m_scanConfig.valueType) &&
            scan_mode_needs_previous(m_scanConfig.get_numeric_scan_mode()))
        {
            m_logService.log_error("[Scanner] Previous-dependent scan mode requires a prior scan");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        resolve_comparator();

        m_scanIteration = 0;
        // Initialized in distribute_regions_to_readers() using per-chunk work units.
        m_totalRegions.store(0, std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);
        m_resultsCount.store(0, std::memory_order_relaxed);
        m_activeReaders.store(0, std::memory_order_relaxed);
        m_nextChunkIndex.store(0, std::memory_order_relaxed);
        m_totalChunks.store(0, std::memory_order_relaxed);
        m_lastProgressNotifyTick.store(0, std::memory_order_relaxed);
        m_allChunks.clear();
        m_sortedNextScanRecords.clear();
        m_resultsReconciled.store(false, std::memory_order_release);

        const int configuredThreads = m_settingsService.get_int("memoryScan.readerThreads");
        const int readerThreads = m_dispatcher.is_single_threaded() ? 1 : configuredThreads;

        m_logService.log_info(fmt::format("[Scanner] Creating {} reader threads (configured: {})", readerThreads, configuredThreads));

        StatusCode status = create_writer_regions(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create writer regions: {}", static_cast<int>(status)));
            m_resultsReconciled.store(true, std::memory_order_release);
            return status;
        }

        status = create_worker_pool(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create worker pool: {}", static_cast<int>(status)));
            m_resultsReconciled.store(true, std::memory_order_release);
            return status;
        }

        status = distribute_regions_to_readers(memoryRegions);
        if (status != StatusCode::STATUS_OK)
        {
            m_resultsReconciled.store(true, std::memory_order_release);
        }

        return status;
    }

    StatusCode MemoryScanner::initialize_next_scan(const ScanConfiguration& configuration, std::shared_ptr<const TypeSchema> schema)
    {
        std::scoped_lock lifecycleLock(m_scanLifecycleMutex);
        if (!drain_active_scan())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        if (!schema)
        {
            m_logService.log_error("[Scanner] initialize_next_scan called with null TypeSchema");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (schema->kind == TypeKind::PluginDefined)
        {
            if (!schema->sdkType || !schema->sdkType->extractor || schema->valueSize == 0 ||
                schema->sdkType->scanModeCount == 0 || schema->sdkType->scanModes == nullptr)
            {
                m_logService.log_error("[Scanner] PluginDefined schema is malformed");
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (configuration.scanMode >= schema->sdkType->scanModeCount)
            {
                m_logService.log_error("[Scanner] PluginDefined scanMode index out of range");
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (!schema->sdkType->scanModes[configuration.scanMode].comparator)
            {
                m_logService.log_error("[Scanner] PluginDefined comparator is null");
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
        }

        if (m_lastScanTypeId != TypeId::Invalid && m_lastScanTypeId != schema->id)
        {
            m_logService.log_error("[Scanner] initialize_next_scan: schema id differs from prior scan");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        m_activeSchema = schema;

        m_logService.log_info("[Scanner] initialize_next_scan called");

        std::uint64_t previousResultCount = 0;

        if (!has_memory_reader())
        {
            m_logService.log_error("[Scanner] No memory reader available for next scan");
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        m_scanAbort.store(false, std::memory_order_seq_cst);
        m_pluginCallStatus.store(StatusCode::STATUS_OK, std::memory_order_release);
        m_lastProgressNotifyTick.store(0, std::memory_order_relaxed);

        save_snapshot_for_undo();

        std::shared_ptr<std::vector<WriterRegionMetadata>> previousRegions;
        std::size_t previousDataSize = 0;
        std::size_t previousFirstValueSize = 0;
        {
            std::scoped_lock undoLock(m_undoHistoryMutex);
            if (m_undoHistory.empty())
            {
                m_logService.log_error("[Scanner] Undo history is empty, cannot do next scan");
                return StatusCode::STATUS_ERROR_GENERAL;
            }
            previousRegions = m_undoHistory.back().writerRegions;
            previousResultCount = m_undoHistory.back().resultsCount;
            previousDataSize = m_undoHistory.back().config.dataSize;
            previousFirstValueSize = m_undoHistory.back().config.firstValueSize;
        }

        if (previousResultCount == 0)
        {
            m_totalRegions.store(0, std::memory_order_relaxed);
            m_regionsScanned.store(0, std::memory_order_relaxed);
            m_resultsCount.store(0, std::memory_order_relaxed);
            m_activeReaders.store(0, std::memory_order_relaxed);
            m_nextChunkIndex.store(0, std::memory_order_relaxed);
            m_totalChunks.store(0, std::memory_order_relaxed);
            m_allChunks.clear();
            m_sortedNextScanRecords.clear();
            {
                std::scoped_lock regionsLock(m_writerRegionsMutex);
                cleanup_writer_regions(m_writerRegions);
                m_writerRegions.clear();
            }
            m_resultsReconciled.store(true, std::memory_order_release);
            return StatusCode::STATUS_OK;
        }

        m_totalRegions.store(previousResultCount, std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);
        m_resultsCount.store(0, std::memory_order_relaxed);
        m_activeReaders.store(0, std::memory_order_relaxed);
        m_nextChunkIndex.store(0, std::memory_order_relaxed);
        m_totalChunks.store(0, std::memory_order_relaxed);
        m_allChunks.clear();
        m_sortedNextScanRecords.clear();
        m_resultsReconciled.store(false, std::memory_order_release);

        m_scanConfig = configuration;

        if (schema->kind == TypeKind::PluginDefined)
        {
            m_scanConfig.dataSize = schema->valueSize;
        }
        else if (is_string_type(m_scanConfig.valueType))
        {
            m_scanConfig.dataSize = m_scanConfig.input.size();
        }
        else
        {
            m_scanConfig.dataSize = get_value_size(m_scanConfig.valueType);
        }

        if (m_scanConfig.dataSize == 0)
        {
            m_resultsReconciled.store(true, std::memory_order_release);
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (previousFirstValueSize > 0)
        {
            m_scanConfig.firstValueSize = previousFirstValueSize;
        }
        else
        {
            m_scanConfig.firstValueSize = previousDataSize;
        }

        resolve_comparator();

        m_scanIteration++;

        StatusCode status = build_sorted_next_scan_records(*previousRegions, previousDataSize, previousFirstValueSize);
        if (status != StatusCode::STATUS_OK)
        {
            m_resultsReconciled.store(true, std::memory_order_release);
            return status;
        }

        const std::size_t sortedResultCount = m_sortedNextScanRecords.size();
        m_totalRegions.store(static_cast<std::uint64_t>(sortedResultCount), std::memory_order_relaxed);

        const int configuredThreads = m_settingsService.get_int("memoryScan.readerThreads");
        const int readerThreads = m_dispatcher.is_single_threaded() ? 1 : configuredThreads;

        status = create_writer_regions(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_resultsReconciled.store(true, std::memory_order_release);
            return status;
        }

        status = create_worker_pool(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_resultsReconciled.store(true, std::memory_order_release);
            return status;
        }

        const auto readerCount = static_cast<std::size_t>(readerThreads);
        const std::size_t totalNextScanChunks = (sortedResultCount + NEXT_SCAN_CHUNK_SIZE - 1) / NEXT_SCAN_CHUNK_SIZE;
        m_totalChunks.store(totalNextScanChunks, std::memory_order_relaxed);
        m_nextChunkIndex.store(0, std::memory_order_relaxed);

        m_activeReaders.store(static_cast<int>(readerCount) + 1, std::memory_order_release);

        for (std::size_t i = 0; i < readerCount; ++i)
        {
            std::packaged_task<StatusCode()> task(
              [this, previousDataSize, previousFirstValueSize, i, sortedResultCount]() -> StatusCode
              {
                  StatusCode workerStatus = StatusCode::STATUS_OK;
                  const std::size_t chunks = m_totalChunks.load(std::memory_order_relaxed);

                  while (!m_scanAbort.load(std::memory_order_acquire))
                  {
                      const std::size_t chunkIndex = m_nextChunkIndex.fetch_add(1, std::memory_order_relaxed);
                      if (chunkIndex >= chunks)
                      {
                          break;
                      }

                      const std::size_t startIndex = chunkIndex * NEXT_SCAN_CHUNK_SIZE;
                      const std::size_t count = std::min(NEXT_SCAN_CHUNK_SIZE, sortedResultCount - startIndex);

                      const StatusCode chunkStatus = scan_previous_results_from_regions(startIndex, count, previousDataSize, previousFirstValueSize, i);
                      if (chunkStatus != StatusCode::STATUS_OK)
                      {
                          workerStatus = chunkStatus;
                          m_logService.log_error(fmt::format("[Scanner] Next scan worker {} failed on chunk {} (status: {})", i, chunkIndex, static_cast<int>(chunkStatus)));
                          m_scanAbort.store(true, std::memory_order_release);
                          break;
                      }
                  }

                  const StatusCode finalizeStatus = finalize_writer_store(i);
                  if (finalizeStatus != StatusCode::STATUS_OK)
                  {
                      m_logService.log_error(fmt::format("[Scanner] Store finalize failed for writer {} (status: {})", i, static_cast<int>(finalizeStatus)));
                      m_scanAbort.store(true, std::memory_order_release);
                      if (workerStatus == StatusCode::STATUS_OK)
                      {
                          workerStatus = finalizeStatus;
                      }
                  }

                  if (m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) == 1)
                  {
                      reconcile_result_count();
                      m_resultsReconciled.store(true, std::memory_order_release);
                      {
                          std::scoped_lock notifyLock(m_mainThreadMutex);
                          m_mainThreadWaitCondition.notify_one();
                      }
                      notify_scan_completion();
                  }

                  return workerStatus;
              });

            status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(task));

            if (status != StatusCode::STATUS_OK)
            {
                m_scanAbort.store(true, std::memory_order_release);
                m_logService.log_error(fmt::format("[Scanner] Failed to enqueue next scan worker {} (status: {})", i, static_cast<int>(status)));

                const int slotsToRelease = static_cast<int>(readerCount - i) + 1;
                const int remainingReaders = m_activeReaders.fetch_sub(slotsToRelease, std::memory_order_acq_rel) - slotsToRelease;
                if (remainingReaders == 0)
                {
                    reconcile_result_count();
                    m_resultsReconciled.store(true, std::memory_order_release);
                    {
                        std::scoped_lock notifyLock(m_mainThreadMutex);
                        m_mainThreadWaitCondition.notify_one();
                    }
                    notify_scan_completion();
                }
                return status;
            }
        }

        for (std::size_t i = 0; i < m_workerCount; ++i)
        {
            std::packaged_task<StatusCode()> collectTask(
              []() -> StatusCode
              {
                  mi_collect(true);
                  return StatusCode::STATUS_OK;
              });
            status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(collectTask));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Collect task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));
            }
        }

        if (m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            reconcile_result_count();
            m_resultsReconciled.store(true, std::memory_order_release);
            {
                std::scoped_lock notifyLock(m_mainThreadMutex);
                m_mainThreadWaitCondition.notify_one();
            }
            notify_scan_completion();
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::undo_scan()
    {
        std::scoped_lock lifecycleLock(m_scanLifecycleMutex);
        if (!drain_active_scan())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        std::scoped_lock undoLock(m_undoHistoryMutex);

        if (m_undoHistory.empty())
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        auto& [iteration, writerRegions, resultsCount, config] = m_undoHistory.back();

        {
            std::scoped_lock regionsLock(m_writerRegionsMutex);
            cleanup_writer_regions(m_writerRegions);
            if (writerRegions)
            {
                m_writerRegions = std::move(*writerRegions);
            }
        }

        m_resultsCount.store(resultsCount, std::memory_order_relaxed);
        m_scanConfig = config;
        m_scanIteration = iteration;

        m_undoHistory.pop_back();

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::stop_scan()
    {
        m_scanAbort.store(true, std::memory_order_release);
        {
            std::scoped_lock lifecycleLock(m_scanLifecycleMutex);
            if (drain_active_scan())
            {
                release_active_schema();
            }
        }
        return StatusCode::STATUS_OK;
    }

    void MemoryScanner::finalize_scan()
    {
        std::scoped_lock lifecycleLock(m_scanLifecycleMutex);
        if (!drain_active_scan())
        {
            m_logService.log_error("[Scanner] finalize_scan aborted: workers did not drain in time");
            return;
        }

        release_active_schema();

        const StatusCode status = m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::Scanner);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_warn(fmt::format("[Scanner] Failed to destroy worker pool during finalize_scan: {}", static_cast<int>(status)));
            return;
        }

        m_workerCount = 0;
    }

    void MemoryScanner::release_active_schema() noexcept
    {
        if (m_activeSchema)
        {
            m_lastScanTypeId = m_activeSchema->id;
        }
        m_activeSchema.reset();
        m_resolvedIsPluginDefined = false;
        m_resolvedPluginExtractor = nullptr;
        m_resolvedPluginComparator = nullptr;
        m_resolvedPluginValueSize = 0;
    }

    bool MemoryScanner::drain_active_scan()
    {
        if (is_scan_complete())
        {
            return true;
        }

        m_scanAbort.store(true, std::memory_order_seq_cst);
        std::unique_lock lock(m_mainThreadMutex);
        const bool drained = m_mainThreadWaitCondition.wait_for(lock, std::chrono::milliseconds(10000),
                                                                [this]
                                                                {
                                                                    return is_scan_complete();
                                                                });

        if (!drained)
        {
            m_logService.log_error("[Scanner] drain_active_scan timed out, workers still active");
        }

        return drained;
    }

    void MemoryScanner::set_scan_abort_state(bool state) { m_scanAbort.store(state, std::memory_order_release); }

    bool MemoryScanner::is_scan_complete()
    {
        const int activeReaders = m_activeReaders.load(std::memory_order_acquire);
        return (activeReaders == 0) && m_resultsReconciled.load(std::memory_order_acquire);
    }

    bool MemoryScanner::can_undo() const
    {
        std::scoped_lock undoLock(m_undoHistoryMutex);
        return !m_undoHistory.empty();
    }

    StatusCode MemoryScanner::is_scan_active() const
    {
        const int activeReaders = m_activeReaders.load(std::memory_order_acquire);
        const bool reconciled = m_resultsReconciled.load(std::memory_order_acquire);
        return (activeReaders > 0 || !reconciled) ? StatusCode::STATUS_ERROR_THREAD_IS_BUSY : StatusCode::STATUS_OK;
    }

    void MemoryScanner::wait_for_scan_completion()
    {
        std::unique_lock lock(m_mainThreadMutex);
        m_mainThreadWaitCondition.wait_for(lock, std::chrono::milliseconds(5000),
                                           [this]
                                           {
                                               return is_scan_complete();
                                           });
        lock.unlock();
        reconcile_result_count();
    }

    StatusCode MemoryScanner::finalize_writer_store(const std::size_t writerIndex)
    {
        std::shared_lock regionsLock(m_writerRegionsMutex);
        if (writerIndex >= m_writerRegions.size())
        {
            m_logService.log_error(fmt::format("[Scanner] finalize_writer_store out of range: {} >= {}", writerIndex, m_writerRegions.size()));
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return m_writerRegions[writerIndex].store.finalize();
    }

    void MemoryScanner::reconcile_result_count()
    {
        {
            std::shared_lock regionsLock(m_writerRegionsMutex);
            std::uint64_t validCount{};
            for (const auto& writerMeta : m_writerRegions)
            {
                if (writerMeta.store.is_valid())
                {
                    validCount += writerMeta.atomics->resultCount.load(std::memory_order_acquire);
                }
            }
            m_resultsCount.store(validCount, std::memory_order_release);
        }

        decltype(m_sortedNextScanRecords){}.swap(m_sortedNextScanRecords);
        decltype(m_allChunks){}.swap(m_allChunks);
    }

    void MemoryScanner::notify_scan_completion()
    {
        release_active_schema();
        std::scoped_lock lock(m_scanCompletionCallbackMutex);
        if (m_scanCompletionCallback)
        {
            m_scanCompletionCallback();
        }
    }

    void MemoryScanner::notify_scan_progress()
    {
        std::scoped_lock lock(m_scanProgressCallbackMutex);
        if (m_scanProgressCallback)
        {
            m_scanProgressCallback();
        }
    }

    void MemoryScanner::notify_scan_progress_throttled()
    {
        constexpr std::uint64_t PROGRESS_NOTIFY_INTERVAL_NS =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(SCAN_PROGRESS_MIN_INTERVAL).count());
        const auto now = std::chrono::steady_clock::now();
        const auto nowTick = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

        auto lastTick = m_lastProgressNotifyTick.load(std::memory_order_relaxed);
        while (nowTick > lastTick)
        {
            if ((nowTick - lastTick) < PROGRESS_NOTIFY_INTERVAL_NS)
            {
                break;
            }

            if (m_lastProgressNotifyTick.compare_exchange_weak(lastTick, nowTick, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                notify_scan_progress();
                break;
            }
        }
    }

    std::uint64_t MemoryScanner::get_regions_scanned() const noexcept { return m_regionsScanned.load(std::memory_order_relaxed); }

    std::uint64_t MemoryScanner::get_total_regions() const noexcept { return m_totalRegions.load(std::memory_order_relaxed); }

    StatusCode MemoryScanner::get_last_plugin_error() const noexcept
    {
        return m_pluginCallStatus.load(std::memory_order_acquire);
    }

    std::uint64_t MemoryScanner::get_results_count() const
    {
        if (m_resultsReconciled.load(std::memory_order_acquire))
        {
            const std::uint64_t cachedCount = m_resultsCount.load(std::memory_order_acquire);
            if (cachedCount > 0)
            {
                return cachedCount;
            }

            // Defend against stale cached zero by validating against writer atomics.
            std::shared_lock regionsLock(m_writerRegionsMutex);
            std::uint64_t runningCount{};
            for (const auto& writerMeta : m_writerRegions)
            {
                if (writerMeta.store.is_valid() && writerMeta.store.base() != nullptr)
                {
                    runningCount += writerMeta.atomics->resultCount.load(std::memory_order_acquire);
                }
            }

            return runningCount;
        }

        std::shared_lock regionsLock(m_writerRegionsMutex);
        std::uint64_t runningCount{};
        for (const auto& writerMeta : m_writerRegions)
        {
            runningCount += writerMeta.atomics->resultCount.load(std::memory_order_relaxed);
        }
        return runningCount;
    }

    void MemoryScanner::cleanup_snapshot_regions(const ScanSnapshot& snapshot) const
    {
        if (snapshot.writerRegions)
        {
            snapshot.writerRegions->clear();
        }
    }

    void MemoryScanner::save_snapshot_for_undo()
    {
        std::scoped_lock undoLock(m_undoHistoryMutex);
        std::unique_lock regionsLock(m_writerRegionsMutex);

        auto sharedRegions = std::make_shared<std::vector<WriterRegionMetadata>>(std::move(m_writerRegions));
        m_writerRegions.clear();
        regionsLock.unlock();

        ScanSnapshot snapshot{.iteration = m_scanIteration, .writerRegions = std::move(sharedRegions), .resultsCount = m_resultsCount.load(std::memory_order_acquire), .config = m_scanConfig};

        m_undoHistory.push_back(std::move(snapshot));

        const std::size_t maxUndoDepth = static_cast<std::size_t>(
          std::clamp(m_settingsService.get_int("memoryScan.maxUndoDepth", 3), 1, static_cast<int>(MAX_UNDO_DEPTH)));
        while (m_undoHistory.size() > maxUndoDepth)
        {
            cleanup_snapshot_regions(m_undoHistory.front());
            m_undoHistory.pop_front();
        }
    }

    StatusCode MemoryScanner::build_sorted_next_scan_records(const std::vector<WriterRegionMetadata>& previousRegions, const std::size_t previousValueSize, const std::size_t previousFirstValueSize)
    {
        const std::size_t recordSize = sizeof(std::uint64_t) + previousValueSize + previousFirstValueSize;
        m_sortedNextScanRecords.clear();

        std::size_t totalRecords{};
        for (const auto& writerMeta : previousRegions)
        {
            totalRecords += writerMeta.atomics->resultCount.load(std::memory_order_acquire);
        }

        try
        {
            m_sortedNextScanRecords.reserve(totalRecords);
        }
        catch (const std::bad_alloc&)
        {
            m_logService.log_error("[Scanner] Failed to reserve memory for sorted previous-result index");
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        for (const auto& writerMeta : previousRegions)
        {
            const std::size_t writerResultCount = writerMeta.atomics->resultCount.load(std::memory_order_acquire);
            if (writerResultCount == 0)
            {
                continue;
            }

            if (!writerMeta.store.is_valid() || writerMeta.store.base() == nullptr)
            {
                continue;
            }

            const auto* regionBase = static_cast<const std::byte*>(writerMeta.store.base());
            for (std::size_t recordIndex = 0; recordIndex < writerResultCount; ++recordIndex)
            {
                const std::byte* recordPtr = regionBase + (recordIndex * recordSize);
                std::array<std::byte, sizeof(std::uint64_t)> addressBytes{};
                std::copy_n(recordPtr, addressBytes.size(), addressBytes.begin());
                const auto address = std::bit_cast<std::uint64_t>(addressBytes);

                m_sortedNextScanRecords.push_back(SortedRecordRef{.address = address, .recordPtr = recordPtr});
            }
        }

        std::ranges::sort(m_sortedNextScanRecords,
                          [](const SortedRecordRef& lhs, const SortedRecordRef& rhs)
                          {
                              if (lhs.address == rhs.address)
                              {
                                  return lhs.recordPtr < rhs.recordPtr;
                              }
                              return lhs.address < rhs.address;
                          });

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::create_worker_pool(const std::size_t workerCount)
    {
        const StatusCode destroyStatus = m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::Scanner);
        if (destroyStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to destroy existing worker pool: {}", static_cast<int>(destroyStatus)));
            return destroyStatus;
        }

        m_logService.log_info(fmt::format("[Scanner] Creating worker pool with {} workers", workerCount));

        m_workerCount = 0;

        const StatusCode status = m_dispatcher.create_worker_pool(Thread::ThreadChannel::Scanner, workerCount);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create worker pool: {}", static_cast<int>(status)));
            return status;
        }

        m_workerCount = workerCount;
        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::distribute_regions_to_readers(const std::vector<ScanRegion>& memoryRegions)
    {
        if (m_workerCount == 0)
        {
            m_logService.log_error("[Scanner] No workers available for distribution");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        m_logService.log_info(fmt::format("[Scanner] Distributing {} regions across {} workers", memoryRegions.size(), m_workerCount));

        constexpr std::size_t MIB = 1024ULL * 1024ULL;
        const int configuredBufferSizeMB = m_settingsService.get_int("memoryScan.threadBufferSizeMB", 8);
        const int configuredWorkerChunkSizeMB = m_settingsService.get_int("memoryScan.workerChunkSizeMB", 8);
        const std::size_t threadBufferSize = static_cast<std::size_t>(std::max(1, configuredBufferSizeMB)) * MIB;
        const std::size_t requestedWorkerChunkSize = static_cast<std::size_t>(std::max(1, configuredWorkerChunkSizeMB)) * MIB;
        const std::size_t workerChunkSize = std::min(threadBufferSize, requestedWorkerChunkSize);

        std::vector<ScanRegion> sortedRegions = memoryRegions;
        std::ranges::sort(sortedRegions,
                          [](const ScanRegion& lhs, const ScanRegion& rhs)
                          {
                              if (lhs.size == rhs.size)
                              {
                                  return lhs.baseAddress < rhs.baseAddress;
                              }
                              return lhs.size > rhs.size;
                          });

        m_allChunks.clear();
        std::uint64_t totalWorkUnits{};
        for (const auto& region : sortedRegions)
        {
            if (region.size == 0)
            {
                continue;
            }

            const std::uint64_t regionChunks = (region.size + workerChunkSize - 1) / workerChunkSize;
            totalWorkUnits += regionChunks;
        }
        m_allChunks.reserve(static_cast<std::size_t>(totalWorkUnits));

        for (const auto& region : sortedRegions)
        {
            for (std::uint64_t chunkOffset = 0; chunkOffset < region.size; chunkOffset += workerChunkSize)
            {
                const std::uint64_t remaining = region.size - chunkOffset;
                const auto chunkSize = static_cast<std::size_t>(std::min<std::uint64_t>(workerChunkSize, remaining));
                m_allChunks.push_back(ChunkDescriptor{.region = region, .chunkOffset = static_cast<std::size_t>(chunkOffset), .chunkSize = chunkSize});
            }
        }

        m_totalChunks.store(m_allChunks.size(), std::memory_order_relaxed);
        m_nextChunkIndex.store(0, std::memory_order_relaxed);
        m_totalRegions.store(static_cast<std::uint64_t>(m_allChunks.size()), std::memory_order_relaxed);
        m_resultsReconciled.store(false, std::memory_order_release);

        StatusCode status = StatusCode::STATUS_OK;
        for (std::size_t i = 0; i < m_workerCount; ++i)
        {
            std::packaged_task<StatusCode()> task(
              [this, i, threadBufferSize]() -> StatusCode
              {
                  thread_local Memory::AlignedByteVector regionBuffer{};
                  if (regionBuffer.size() < threadBufferSize)
                  {
                      regionBuffer.resize(threadBufferSize);
                  }

                  const std::size_t myWriterIndex = i;

                  std::shared_ptr<IMemoryReader> reader;
                  {
                      std::scoped_lock lock(m_memoryReaderMutex);
                      reader = m_memoryReader;
                  }

                  StatusCode workerStatus = StatusCode::STATUS_OK;
                  if (!reader)
                  {
                      workerStatus = StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                      m_scanAbort.store(true, std::memory_order_release);
                  }
                  else
                  {
                      const std::size_t totalChunks = m_totalChunks.load(std::memory_order_relaxed);

                      while (!m_scanAbort.load(std::memory_order_acquire))
                      {
                          const std::size_t chunkIndex = m_nextChunkIndex.fetch_add(1, std::memory_order_relaxed);
                          if (chunkIndex >= totalChunks)
                          {
                              break;
                          }

                          const ChunkDescriptor& chunk = m_allChunks[chunkIndex];
                          ScanRegion chunkRegion = chunk.region;
                          chunkRegion.baseAddress += static_cast<std::uint64_t>(chunk.chunkOffset);
                          chunkRegion.size = chunk.chunkSize;

                          const StatusCode chunkStatus = scan_memory_region(chunkRegion, myWriterIndex, *reader, regionBuffer);
                          if (chunkStatus != StatusCode::STATUS_OK)
                          {
                              workerStatus = chunkStatus;
                              m_scanAbort.store(true, std::memory_order_release);
                              break;
                          }
                      }
                  }

                  regionBuffer.clear();
                  regionBuffer.shrink_to_fit();

                  const StatusCode finalizeStatus = finalize_writer_store(myWriterIndex);
                  if (finalizeStatus != StatusCode::STATUS_OK)
                  {
                      m_logService.log_error(fmt::format("[Scanner] Store finalize failed for writer {} (status: {})", myWriterIndex, static_cast<int>(finalizeStatus)));
                      m_scanAbort.store(true, std::memory_order_release);
                      if (workerStatus == StatusCode::STATUS_OK)
                      {
                          workerStatus = finalizeStatus;
                      }
                  }

                  if (m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) == 1)
                  {
                      reconcile_result_count();
                      m_resultsReconciled.store(true, std::memory_order_release);
                      {
                          std::scoped_lock notifyLock(m_mainThreadMutex);
                          m_mainThreadWaitCondition.notify_one();
                      }
                      notify_scan_completion();
                  }

                  return workerStatus;
              });

            m_activeReaders.fetch_add(1, std::memory_order_release);
            status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(task));

            if (status != StatusCode::STATUS_OK)
            {
                const int remainingReaders = m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) - 1;
                m_scanAbort.store(true, std::memory_order_release);
                m_logService.log_error(fmt::format("[Scanner] Failed to enqueue worker {} (status: {})", i, static_cast<int>(status)));
                if (remainingReaders == 0)
                {
                    reconcile_result_count();
                    m_resultsReconciled.store(true, std::memory_order_release);
                    {
                        std::scoped_lock notifyLock(m_mainThreadMutex);
                        m_mainThreadWaitCondition.notify_one();
                    }
                    notify_scan_completion();
                }
                return status;
            }
        }

        for (std::size_t i = 0; i < m_workerCount; ++i)
        {
            std::packaged_task<StatusCode()> collectTask(
              []() -> StatusCode
              {
                  mi_collect(true);
                  return StatusCode::STATUS_OK;
              });
            const StatusCode statusCode = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(collectTask));

            if (statusCode != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Collect task for thread {} could not be enqueued (status: {})", i, static_cast<int>(statusCode)));
            }
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
