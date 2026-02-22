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

        m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::Scanner);

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
        return m_memoryReader != nullptr;
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

        m_scanAbort.store(false, std::memory_order_seq_cst);

        {
            std::scoped_lock undoLock(m_undoHistoryMutex);
            for (auto& snapshot : m_undoHistory)
            {
                cleanup_snapshot_regions(snapshot);
            }
            m_undoHistory.clear();
        }

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
        m_pendingWriterTasks.store(0, std::memory_order_relaxed);

        const int configuredThreads = m_settingsService.get_int("memoryScan.readerThreads");
        const int readerThreads = m_dispatcher.is_single_threaded() ? 1 : configuredThreads;

        m_logService.log_info(fmt::format("[Scanner] Creating {} reader threads (configured: {})", readerThreads, configuredThreads));

        StatusCode status = create_writer_regions(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create writer regions: {}", static_cast<int>(status)));
            return status;
        }

        status = create_worker_pool(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create worker pool: {}", static_cast<int>(status)));
            return status;
        }

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
            m_pendingWriterTasks.store(0, std::memory_order_relaxed);
            return StatusCode::STATUS_OK;
        }

        m_totalRegions.store(previousResultCount, std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);
        m_resultsCount.store(0, std::memory_order_relaxed);
        m_activeReaders.store(0, std::memory_order_relaxed);
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

        const int configuredThreads = m_settingsService.get_int("memoryScan.readerThreads");
        const int readerThreads = m_dispatcher.is_single_threaded() ? 1 : configuredThreads;

        StatusCode status = create_writer_regions(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = create_worker_pool(static_cast<std::size_t>(readerThreads));
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        const auto readerCount = static_cast<std::size_t>(readerThreads);
        const std::size_t recordsPerReader = (previousResultCount + readerCount - 1) / readerCount;

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

            status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(task));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("[Scanner] Failed to enqueue next scan chunk {} (status: {})", i, static_cast<int>(status)));
                return status;
            }

            m_activeReaders.fetch_add(1, std::memory_order_release);
        }

        for (std::size_t i = 0; i < readerCount; ++i)
        {
            m_pendingWriterTasks.fetch_add(1, std::memory_order_release);

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

            status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(finalizeTask));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Finalize task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));

                if (m_pendingWriterTasks.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    std::scoped_lock notifyLock(m_mainThreadMutex);
                    m_mainThreadWaitCondition.notify_one();
                }
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

    void MemoryScanner::finalize_scan()
    {
        m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::Scanner);
        m_workerCount = 0;
    }

    void MemoryScanner::set_scan_abort_state(bool state) { m_scanAbort.store(state, std::memory_order_release); }

    bool MemoryScanner::is_scan_complete()
    {
        const int activeReaders = m_activeReaders.load(std::memory_order_acquire);
        const int pendingWriters = m_pendingWriterTasks.load(std::memory_order_acquire);

        return (activeReaders == 0) && (pendingWriters == 0);
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
        return (activeReaders > 0 || pendingWriters > 0) ? StatusCode::STATUS_ERROR_THREAD_IS_BUSY : StatusCode::STATUS_OK;
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

    StatusCode MemoryScanner::create_worker_pool(const std::size_t workerCount)
    {
        m_logService.log_info(fmt::format("[Scanner] Creating worker pool with {} workers", workerCount));

        m_dispatcher.destroy_worker_pool(Thread::ThreadChannel::Scanner);
        m_workerCount = workerCount;

        const StatusCode status = m_dispatcher.create_worker_pool(Thread::ThreadChannel::Scanner, workerCount);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[Scanner] Failed to create worker pool: {}", static_cast<int>(status)));
            m_workerCount = 0;
        }

        return status;
    }

    StatusCode MemoryScanner::distribute_regions_to_readers(const std::vector<ScanRegion>& memoryRegions)
    {
        if (m_workerCount == 0)
        {
            m_logService.log_error("[Scanner] No workers available for distribution");
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        m_logService.log_info(fmt::format("[Scanner] Distributing {} regions across {} workers", memoryRegions.size(), m_workerCount));

        const std::size_t threadBufferSize = static_cast<std::size_t>(m_settingsService.get_int("memoryScan.threadBufferSizeMB", 32)) * 1024ULL * 1024;

        for (std::size_t i = 0; i < memoryRegions.size(); ++i)
        {
            const std::size_t readerIndex = i % m_workerCount;
            const ScanRegion& region = memoryRegions[i];

            std::packaged_task<StatusCode()> task(
              [this, region, readerIndex, threadBufferSize]() -> StatusCode
              {
                  thread_local Memory::AlignedByteVector regionBuffer {};
                  if (regionBuffer.size() < threadBufferSize)
                  {
                      regionBuffer.resize(threadBufferSize);
                  }

                  return scan_memory_region(region, readerIndex, regionBuffer);
              });

            const StatusCode status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, readerIndex, std::move(task));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("[Scanner] Failed to enqueue region {} (status: {})", i, static_cast<int>(status)));
                return status;
            }

            m_activeReaders.fetch_add(1, std::memory_order_release);
        }

        for (std::size_t i = 0; i < m_workerCount; ++i)
        {
            m_pendingWriterTasks.fetch_add(1, std::memory_order_release);

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

            const StatusCode status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(finalizeTask));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Finalize task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));

                if (m_pendingWriterTasks.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    std::scoped_lock notifyLock(m_mainThreadMutex);
                    m_mainThreadWaitCondition.notify_one();
                }
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
            const StatusCode status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::Scanner, i, std::move(collectTask));

            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("[Scanner] Collect task for thread {} could not be enqueued (status: {})", i, static_cast<int>(status)));
            }
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
