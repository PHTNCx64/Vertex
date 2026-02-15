//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <future>
#include <chrono>
#include <algorithm>
#include <fmt/format.h>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/comparators.hh>
#include <vertex/memory/scannerallocator.hh>

namespace Vertex::Scanner
{
    MemoryScanner::MemoryScanner(Configuration::ISettings& settingsService, Log::ILog& logService)
        : m_settingsService(settingsService),
          m_logService(logService)
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

        clear_thread_pools();

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

    bool MemoryScanner::has_memory_reader() const
    {
        std::scoped_lock lock(m_memoryReaderMutex);
        return m_memoryReader && m_memoryReader->is_valid();
    }

    StatusCode MemoryScanner::initialize_scan(const ScanConfiguration& configuration, const std::vector<ScanRegion>& memoryRegions)
    {
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

        m_scanAbort.store(true, std::memory_order_seq_cst);

        {
            std::unique_lock lock(m_mainThreadMutex);
            m_mainThreadWaitCondition.wait_for(lock, std::chrono::milliseconds(10000),
                                               [this]
                                               {
                                                   return is_scan_complete();
                                               });
        }

        clear_thread_pools();

        std::atomic_thread_fence(std::memory_order_seq_cst);

        {
            std::scoped_lock undoLock(m_undoHistoryMutex);
            for (auto& snapshot : m_undoHistory)
            {
                cleanup_snapshot_regions(snapshot);
            }
            m_undoHistory.clear();
        }

        m_scanAbort.store(false, std::memory_order_seq_cst);
        m_scanConfig = configuration;

        if (is_string_type(m_scanConfig.valueType))
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

        resolve_comparator();

        m_scanIteration = 0;
        m_totalRegions.store(memoryRegions.size(), std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);
        m_resultsCount.store(0, std::memory_order_relaxed);
        m_activeReaders.store(0, std::memory_order_relaxed);
        m_activeWriters.store(0, std::memory_order_relaxed);
        m_pendingWriterTasks.store(0, std::memory_order_relaxed);

        const int readerThreads = m_settingsService.get_int("memoryScan.readerThreads");

        m_logService.log_info(fmt::format("[Scanner] Creating {} reader threads", readerThreads));

        StatusCode status = create_writer_regions(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create writer regions: {}", static_cast<int>(status)));
            return status;
        }

        status = create_threads(readerThreads);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create threads: {}", static_cast<int>(status)));
            return status;
        }

        m_activeReaders.store(static_cast<int>(memoryRegions.size()), std::memory_order_release);

        status = distribute_regions_to_readers(memoryRegions);

        return status;
    }

    StatusCode MemoryScanner::initialize_next_scan(const ScanConfiguration& configuration)
    {
        m_logService.log_info("[Scanner] initialize_next_scan called");

        std::uint64_t previousResultCount = 0;

        if (!has_memory_reader())
        {
            m_logService.log_error("[Scanner] No memory reader available for next scan");
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        m_scanAbort.store(true, std::memory_order_seq_cst);

        {
            std::unique_lock lock(m_mainThreadMutex);
            m_mainThreadWaitCondition.wait_for(lock, std::chrono::milliseconds(10000),
                                               [this]
                                               {
                                                   return is_scan_complete();
                                               });
        }

        clear_thread_pools();

        std::atomic_thread_fence(std::memory_order_seq_cst);

        m_scanAbort.store(false, std::memory_order_seq_cst);

        save_snapshot_for_undo();

        const std::vector<WriterRegionMetadata>* previousRegionsPtr = nullptr;
        std::size_t previousDataSize = 0;
        std::size_t previousFirstValueSize = 0;
        ValueType previousValueType{};
        {
            std::scoped_lock undoLock(m_undoHistoryMutex);
            if (m_undoHistory.empty())
            {
                m_logService.log_error("[Scanner] Undo history is empty, cannot do next scan");
                return StatusCode::STATUS_ERROR_GENERAL;
            }
            previousRegionsPtr = &m_undoHistory.back().writerRegions;
            previousResultCount = m_undoHistory.back().resultsCount;
            previousDataSize = m_undoHistory.back().config.dataSize;
            previousFirstValueSize = m_undoHistory.back().config.firstValueSize;
            previousValueType = m_undoHistory.back().config.valueType;
        }

        if (configuration.needs_previous_value() && configuration.valueType != previousValueType)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (previousResultCount == 0)
        {
            m_totalRegions.store(0, std::memory_order_relaxed);
            m_regionsScanned.store(0, std::memory_order_relaxed);
            m_resultsCount.store(0, std::memory_order_relaxed);
            m_activeReaders.store(0, std::memory_order_relaxed);
            m_activeWriters.store(0, std::memory_order_relaxed);
            m_pendingWriterTasks.store(0, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        }

        m_totalRegions.store(previousResultCount, std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);
        m_resultsCount.store(0, std::memory_order_relaxed);
        m_activeReaders.store(0, std::memory_order_relaxed);
        m_activeWriters.store(0, std::memory_order_relaxed);
        m_pendingWriterTasks.store(0, std::memory_order_relaxed);

        m_scanConfig = configuration;

        if (is_string_type(m_scanConfig.valueType))
        {
            m_scanConfig.dataSize = m_scanConfig.input.size();
        }
        else
        {
            m_scanConfig.dataSize = get_value_size(m_scanConfig.valueType);
        }

        if (m_scanConfig.dataSize == 0)
        {
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

        const int readerThreads = m_settingsService.get_int("memoryScan.readerThreads");

        StatusCode status = create_writer_regions(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = create_threads(readerThreads);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        const auto readerCount = static_cast<std::size_t>(readerThreads);
        const std::size_t recordsPerReader = (previousResultCount + readerCount - 1) / readerCount;

        int actualReaderTasks = 0;
        for (std::size_t i = 0; i < readerCount; ++i)
        {
            const std::size_t startIndex = i * recordsPerReader;
            if (startIndex >= previousResultCount)
                break;
            const std::size_t count = std::min<std::size_t>(recordsPerReader, previousResultCount - startIndex);
            if (count > 0)
                actualReaderTasks++;
        }

        m_pendingWriterTasks.store(static_cast<int>(readerCount), std::memory_order_release);
        m_activeReaders.store(actualReaderTasks, std::memory_order_release);

        for (std::size_t i = 0; i < readerCount; ++i)
        {
            const std::size_t startIndex = i * recordsPerReader;
            if (startIndex >= previousResultCount)
            {
                break;
            }

            const std::size_t count = std::min<std::size_t>(recordsPerReader, previousResultCount - startIndex);
            if (count == 0)
            {
                break;
            }

            std::packaged_task<StatusCode()> task(
              [this, previousRegionsPtr, startIndex, count, previousDataSize, previousFirstValueSize, i]() -> StatusCode
              {
                  return scan_previous_results_from_regions(*previousRegionsPtr, startIndex, count, previousDataSize, previousFirstValueSize, i);
              });

            status = enqueue_task_with_fallback(std::move(task), i, fmt::format("next_scan_chunk[{}]", i));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("[Scanner] Failed to enqueue next scan chunk {} after all recovery attempts (status: {})", i, static_cast<int>(status)));
                return status;
            }
        }

        for (std::size_t i = 0; i < readerCount; ++i)
        {
            std::packaged_task<StatusCode()> finalizeTask(
              [this, i]() -> StatusCode
              {
                  const StatusCode finalizeStatus = m_writerRegions[i].store.finalize();

                  if (m_pendingWriterTasks.fetch_sub(1, std::memory_order_acq_rel) == 1)
                  {
                      std::scoped_lock notifyLock(m_mainThreadMutex);
                      m_mainThreadWaitCondition.notify_one();
                  }
                  return finalizeStatus;
              });

            status = enqueue_task_with_fallback(std::move(finalizeTask), i, fmt::format("next_scan_finalize[{}]", i));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Finalize task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));
            }
        }

        for (std::size_t i = 0; i < m_readerThreads.size(); ++i)
        {
            std::packaged_task<StatusCode()> collectTask(
              []() -> StatusCode
              {
                  mi_collect(true);
                  return StatusCode::STATUS_OK;
              });
            status = enqueue_task_with_fallback(std::move(collectTask), i, fmt::format("next_scan_collect[{}]", i));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Collect task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::undo_scan()
    {
        std::scoped_lock undoLock(m_undoHistoryMutex);

        if (m_undoHistory.empty())
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        auto& [iteration, writerRegions, resultsCount, config] = m_undoHistory.back();

        {
            std::scoped_lock regionsLock(m_writerRegionsMutex);
            cleanup_writer_regions(m_writerRegions);
            m_writerRegions = std::move(writerRegions);
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
        return StatusCode::STATUS_OK;
    }

    void MemoryScanner::set_scan_abort_state(bool state) { m_scanAbort.store(state, std::memory_order_release); }

    bool MemoryScanner::is_scan_complete()
    {
        const int activeReaders = m_activeReaders.load(std::memory_order_acquire);
        const int pendingWriters = m_pendingWriterTasks.load(std::memory_order_acquire);
        const int activeWriters = m_activeWriters.load(std::memory_order_acquire);

        return (activeReaders == 0) && (pendingWriters == 0) && (activeWriters == 0);
    }

    bool MemoryScanner::can_undo() const
    {
        std::scoped_lock undoLock(m_undoHistoryMutex);
        return !m_undoHistory.empty();
    }

    StatusCode MemoryScanner::is_scan_active() const
    {
        const int activeReaders = m_activeReaders.load(std::memory_order_acquire);
        const int pendingWriters = m_pendingWriterTasks.load(std::memory_order_acquire);
        const int activeWriters = m_activeWriters.load(std::memory_order_acquire);
        return (activeReaders > 0 || pendingWriters > 0 || activeWriters > 0) ? StatusCode::STATUS_ERROR_THREAD_IS_BUSY : StatusCode::STATUS_OK;
    }

    void MemoryScanner::wait_for_scan_completion()
    {
        std::unique_lock lock(m_mainThreadMutex);
        m_mainThreadWaitCondition.wait_for(lock, std::chrono::milliseconds(5000),
                                           [this]
                                           {
                                               return is_scan_complete();
                                           });
    }

    std::uint64_t MemoryScanner::get_regions_scanned() const noexcept { return m_regionsScanned.load(std::memory_order_relaxed); }

    std::uint64_t MemoryScanner::get_total_regions() const noexcept { return m_totalRegions.load(std::memory_order_relaxed); }

    std::uint64_t MemoryScanner::get_results_count() const noexcept { return m_resultsCount.load(std::memory_order_relaxed); }

    void MemoryScanner::cleanup_snapshot_regions(ScanSnapshot& snapshot) const { snapshot.writerRegions.clear(); }

    void MemoryScanner::save_snapshot_for_undo()
    {
        std::scoped_lock undoLock(m_undoHistoryMutex);
        std::unique_lock regionsLock(m_writerRegionsMutex);

        ScanSnapshot snapshot{.iteration = m_scanIteration, .writerRegions = std::move(m_writerRegions), .resultsCount = m_resultsCount.load(std::memory_order_acquire), .config = m_scanConfig};

        m_writerRegions.clear();
        regionsLock.unlock();

        m_undoHistory.push_back(std::move(snapshot));

        while (m_undoHistory.size() > MAX_UNDO_DEPTH)
        {
            cleanup_snapshot_regions(m_undoHistory.front());
            m_undoHistory.pop_front();
        }
    }

    StatusCode MemoryScanner::create_threads(int numReaders)
    {
        m_logService.log_info(fmt::format("[Scanner] Creating {} reader threads", numReaders));

        clear_thread_pools();

        m_readerThreads.reserve(static_cast<std::size_t>(numReaders));
        for (int i = 0; i < numReaders; ++i)
        {
            auto thread = std::make_unique<Thread::VertexSPSCThread>();
            if (!thread->is_running())
            {
                m_logService.log_error(fmt::format("[Scanner] Reader thread {} failed to start", i));
                return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
            }
            m_readerThreads.push_back(std::move(thread));
        }

        return StatusCode::STATUS_OK;
    }

    void MemoryScanner::clear_thread_pools()
    {
        for (const auto& reader : m_readerThreads)
        {
            if (reader && reader->is_running())
            {
                const StatusCode status = reader->stop();
                if (status != StatusCode::STATUS_OK)
                {
                    m_logService.log_warn(fmt::format("[Scanner] Failed to stop reader thread: {}", static_cast<int>(status)));
                }
            }
        }

        m_readerThreads.clear();
        mi_collect(true);
    }

    std::optional<std::size_t> MemoryScanner::find_available_thread(std::size_t excludeIndex) const
    {
        for (std::size_t i = 0; i < m_readerThreads.size(); ++i)
        {
            if (i == excludeIndex)
            {
                continue;
            }
            if (m_readerThreads[i] && m_readerThreads[i]->is_running())
            {
                return i;
            }
        }
        return std::nullopt;
    }

    StatusCode MemoryScanner::enqueue_task_with_fallback(std::packaged_task<StatusCode()>&& task, std::size_t preferredIndex, std::string_view taskLabel) const
    {
        if (preferredIndex >= m_readerThreads.size() || !m_readerThreads[preferredIndex])
        {
            m_logService.log_error(fmt::format("[Scanner] {} - thread index {} is out of range or null (pool size: {})", taskLabel, preferredIndex, m_readerThreads.size()));
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        StatusCode status = m_readerThreads[preferredIndex]->enqueue_task(std::move(task));
        if (status == StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_OK;
        }

        m_logService.log_warn(fmt::format("[Scanner] {} - enqueue failed on thread {} (status: {}), attempting recovery", taskLabel, preferredIndex, static_cast<int>(status)));

        if (status == StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING)
        {
            m_logService.log_warn(fmt::format("[Scanner] {} - thread {} not running, attempting restart", taskLabel, preferredIndex));

            StatusCode restartStatus = m_readerThreads[preferredIndex]->start();
            if (restartStatus == StatusCode::STATUS_OK)
            {
                m_logService.log_info(fmt::format("[Scanner] {} - thread {} restarted successfully, re-enqueuing", taskLabel, preferredIndex));

                status = m_readerThreads[preferredIndex]->enqueue_task(std::move(task));
                if (status == StatusCode::STATUS_OK)
                {
                    return StatusCode::STATUS_OK;
                }

                m_logService.log_warn(fmt::format("[Scanner] {} - re-enqueue after restart still failed on thread {} (status: {})", taskLabel, preferredIndex, static_cast<int>(status)));
            }
            else
            {
                m_logService.log_warn(fmt::format("[Scanner] {} - restart failed on thread {} (status: {})", taskLabel, preferredIndex, static_cast<int>(restartStatus)));
            }
        }

        auto alternateIndex = find_available_thread(preferredIndex);
        if (!alternateIndex.has_value())
        {
            m_logService.log_error(fmt::format("[Scanner] {} - no alternate threads available for redistribution, all threads are down", taskLabel));
            return status;
        }

        m_logService.log_warn(fmt::format("[Scanner] {} - redistributing from thread {} to thread {}", taskLabel, preferredIndex, *alternateIndex));

        status = m_readerThreads[*alternateIndex]->enqueue_task(std::move(task));
        if (status == StatusCode::STATUS_OK)
        {
            m_logService.log_info(fmt::format("[Scanner] {} - successfully redistributed to thread {}", taskLabel, *alternateIndex));
            return StatusCode::STATUS_OK;
        }

        m_logService.log_error(fmt::format("[Scanner] {} - redistribution to thread {} also failed (status: {})", taskLabel, *alternateIndex, static_cast<int>(status)));
        return status;
    }

    StatusCode MemoryScanner::distribute_regions_to_readers(const std::vector<ScanRegion>& memoryRegions)
    {
        const std::size_t readerCount = m_readerThreads.size();
        if (readerCount == 0)
        {
            m_logService.log_error("[Scanner] No reader threads available for distribution");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        m_logService.log_info(fmt::format("[Scanner] Distributing {} regions across {} reader threads", memoryRegions.size(), readerCount));

        m_pendingWriterTasks.store(static_cast<int>(readerCount), std::memory_order_release);

        for (std::size_t i = 0; i < memoryRegions.size(); ++i)
        {
            const std::size_t readerIndex = i % readerCount;
            const ScanRegion& region = memoryRegions[i];

            std::packaged_task<StatusCode()> task(
              [this, region, readerIndex]() -> StatusCode
              {
                  return scan_memory_region(region, readerIndex);
              });

            const StatusCode status = enqueue_task_with_fallback(std::move(task), readerIndex, fmt::format("scan_region[{}]", i));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("[Scanner] Failed to enqueue region {} after all recovery attempts (status: {})", i, static_cast<int>(status)));
                return status;
            }
        }

        for (std::size_t i = 0; i < readerCount; ++i)
        {
            std::packaged_task<StatusCode()> finalizeTask(
              [this, i]() -> StatusCode
              {
                  const StatusCode finalizeStatus = m_writerRegions[i].store.finalize();

                  if (m_pendingWriterTasks.fetch_sub(1, std::memory_order_acq_rel) == 1)
                  {
                      std::scoped_lock notifyLock(m_mainThreadMutex);
                      m_mainThreadWaitCondition.notify_one();
                  }
                  return finalizeStatus;
              });

            const StatusCode status = enqueue_task_with_fallback(std::move(finalizeTask), i, fmt::format("finalize_store[{}]", i));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Finalize task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));
            }
        }

        for (std::size_t i = 0; i < m_readerThreads.size(); ++i)
        {
            std::packaged_task<StatusCode()> collectTask(
              []() -> StatusCode
              {
                  mi_collect(true);
                  return StatusCode::STATUS_OK;
              });
            const StatusCode status = enqueue_task_with_fallback(std::move(collectTask), i, fmt::format("mi_collect[{}]", i));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Collect task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));
            }
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
