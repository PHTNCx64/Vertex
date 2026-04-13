//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscanner.hh>

#include <algorithm>
#include <future>
#include <queue>
#include <fmt/format.h>

namespace Vertex::Scanner
{
    StatusCode PointerScanner::build_reverse_index(const std::vector<ScanRegion>& regions)
    {
        m_currentPhase.store(PointerScanPhase::IndexBuild, std::memory_order_release);
        m_totalRegions.store(regions.size(), std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);

        const auto bufferSizeMB = static_cast<std::size_t>(m_settingsService.get_int("pointerScan.threadBufferSizeMB", 64));
        const std::size_t bufferCapacity = (bufferSizeMB * 1024ULL * 1024) / sizeof(ReverseIndexEntry);

        m_workerIndexRuns.resize(m_workerCount);
        m_workerIndexBuffers.resize(m_workerCount);

        for (std::size_t i{}; i < m_workerCount; ++i)
        {
            m_workerIndexBuffers[i].capacity = bufferCapacity;
            m_workerIndexBuffers[i].entries.reserve(bufferCapacity);
        }

        int remoteRegionCount{};
        for (std::size_t i{}; i < regions.size(); ++i)
        {
            if (i % m_workerCount != 0)
            {
                ++remoteRegionCount;
            }
        }

        m_activeWorkers.store(remoteRegionCount, std::memory_order_release);

        for (std::size_t i{}; i < regions.size(); ++i)
        {
            const std::size_t workerIndex = i % m_workerCount;
            if (workerIndex == 0)
            {
                continue;
            }

            const auto& region = regions[i];

            std::packaged_task<StatusCode()> task{
                [this, region, workerIndex]() -> StatusCode
                {
                    const StatusCode result = scan_region_for_pointers(region, workerIndex);
                    if (result != StatusCode::STATUS_OK)
                    {
                        report_worker_error(result);
                    }
                    m_regionsScanned.fetch_add(1, std::memory_order_relaxed);

                    if (m_activeWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
                        std::scoped_lock lock{m_completionMutex};
                        m_completionCondition.notify_all();
                    }

                    return result;
                }};

            const StatusCode status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::PointerScanner, workerIndex, std::move(task));
            if (status != StatusCode::STATUS_OK)
            {
                m_activeWorkers.fetch_sub(1, std::memory_order_acq_rel);
                m_logService.log_error(fmt::format("[PointerScanner] Failed to enqueue region {} (status: {})", i, static_cast<int>(status)));
                report_worker_error(status);
            }
        }

        for (std::size_t i{}; i < regions.size(); i += m_workerCount)
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                break;
            }

            const StatusCode inlineResult = scan_region_for_pointers(regions[i], 0);
            if (inlineResult != StatusCode::STATUS_OK)
            {
                report_worker_error(inlineResult);
            }
            m_regionsScanned.fetch_add(1, std::memory_order_relaxed);
        }

        if (remoteRegionCount > 0)
        {
            wait_for_remote_workers();
        }

        if (m_scanAbort.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
        }

        const StatusCode scanPhaseError = collect_worker_error();
        if (scanPhaseError != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Index scan phase failed: {}", static_cast<int>(scanPhaseError)));
            return scanPhaseError;
        }

        const int remoteFlushCount = static_cast<int>(m_workerCount > 1 ? m_workerCount - 1 : 0);
        m_activeWorkers.store(remoteFlushCount, std::memory_order_release);

        for (std::size_t i = 1; i < m_workerCount; ++i)
        {
            std::packaged_task<StatusCode()> flushTask{
                [this, i]() -> StatusCode
                {
                    StatusCode result = StatusCode::STATUS_OK;

                    if (!m_workerIndexBuffers[i].entries.empty())
                    {
                        result = flush_index_buffer(i);
                        if (result != StatusCode::STATUS_OK)
                        {
                            report_worker_error(result);
                        }
                    }

                    if (m_activeWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
                        std::scoped_lock lock{m_completionMutex};
                        m_completionCondition.notify_all();
                    }

                    return result;
                }};

            const StatusCode status = m_dispatcher.enqueue_on_worker(Thread::ThreadChannel::PointerScanner, i, std::move(flushTask));
            if (status != StatusCode::STATUS_OK)
            {
                m_activeWorkers.fetch_sub(1, std::memory_order_acq_rel);
                report_worker_error(status);
            }
        }

        if (!m_workerIndexBuffers[0].entries.empty())
        {
            const StatusCode inlineFlushResult = flush_index_buffer(0);
            if (inlineFlushResult != StatusCode::STATUS_OK)
            {
                report_worker_error(inlineFlushResult);
            }
        }

        if (remoteFlushCount > 0)
        {
            wait_for_remote_workers();
        }

        const StatusCode flushPhaseError = collect_worker_error();
        if (flushPhaseError != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Index flush phase failed: {}", static_cast<int>(flushPhaseError)));
            return flushPhaseError;
        }

        std::size_t totalEntries{};
        std::size_t totalRuns{};
        for (const auto& workerRuns : m_workerIndexRuns)
        {
            totalRuns += workerRuns.size();
            for (const auto& run : workerRuns)
            {
                totalEntries += run.entryCount;
            }
        }

        m_logService.log_info(fmt::format("[PointerScanner] Index build complete: {} entries across {} runs", totalEntries, totalRuns));

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::scan_region_for_pointers(const ScanRegion& region, const std::size_t workerIndex)
    {
        constexpr std::size_t CHUNK_SIZE = 4ULL * 1024 * 1024;

        const std::uint32_t moduleId = resolve_module_id(region.moduleName);
        const std::uint64_t alignment = m_config.alignment;
        const std::uint64_t ptrSize = m_pointerSize;

        std::shared_ptr<IMemoryReader> reader;
        {
            std::scoped_lock lock{m_memoryReaderMutex};
            reader = m_memoryReader;
        }

        if (!reader)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        std::vector<char> readBuffer(CHUNK_SIZE);

        std::uint64_t offset{};
        while (offset < region.size)
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
            }

            const std::uint64_t chunkSize = std::min(static_cast<std::uint64_t>(CHUNK_SIZE), region.size - offset);
            const std::uint64_t chunkAddress = region.baseAddress + offset;

            const StatusCode readStatus = reader->read_memory(chunkAddress, chunkSize, readBuffer.data());
            if (readStatus != StatusCode::STATUS_OK)
            {
                offset += chunkSize;
                continue;
            }

            const std::uint64_t scanEnd = chunkSize >= ptrSize ? chunkSize - ptrSize + 1 : 0;

            for (std::uint64_t pos{}; pos < scanEnd; pos += alignment)
            {
                std::uint64_t value{};
                std::copy_n(readBuffer.data() + pos, ptrSize, reinterpret_cast<char*>(&value));

                if (value < MIN_VALID_POINTER)
                {
                    continue;
                }

                auto& buffer = m_workerIndexBuffers[workerIndex];
                buffer.entries.emplace_back(ReverseIndexEntry{
                    .pointerValue = value,
                    .sourceAddress = chunkAddress + pos,
                    .moduleId = moduleId,
                    .flags = 0});

                if (buffer.entries.size() >= buffer.capacity)
                {
                    const StatusCode flushStatus = flush_index_buffer(workerIndex);
                    if (flushStatus != StatusCode::STATUS_OK)
                    {
                        return flushStatus;
                    }
                }
            }

            offset += chunkSize;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::flush_index_buffer(const std::size_t workerIndex)
    {
        auto& buffer = m_workerIndexBuffers[workerIndex];

        if (buffer.entries.empty())
        {
            return StatusCode::STATUS_OK;
        }

        std::ranges::sort(buffer.entries, [](const ReverseIndexEntry& a, const ReverseIndexEntry& b)
                          {
                              return a.pointerValue < b.pointerValue;
                          });

        IO::ScanResultStore store{};
        StatusCode status = store.open();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to open index run store for worker {}", workerIndex));
            return StatusCode::STATUS_ERROR_POINTER_SCAN_INDEX_BUILD_FAILED;
        }

        status = store.append(buffer.entries.data(), buffer.entries.size() * sizeof(ReverseIndexEntry));
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to append index run data for worker {}", workerIndex));
            return StatusCode::STATUS_ERROR_POINTER_SCAN_INDEX_BUILD_FAILED;
        }

        status = store.finalize();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to finalize index run store for worker {}", workerIndex));
            return StatusCode::STATUS_ERROR_POINTER_SCAN_INDEX_BUILD_FAILED;
        }

        m_workerIndexRuns[workerIndex].emplace_back(IndexRunMetadata{
            .store = std::move(store),
            .entryCount = buffer.entries.size()});

        buffer.entries.clear();
        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::merge_index_runs()
    {
        m_currentPhase.store(PointerScanPhase::IndexMergeSort, std::memory_order_release);

        std::vector<IndexRunMetadata*> allRuns{};
        for (auto& workerRuns : m_workerIndexRuns)
        {
            for (auto& run : workerRuns)
            {
                allRuns.push_back(&run);
            }
        }

        if (allRuns.empty())
        {
            m_mergedIndexEntryCount = 0;
            m_logService.log_info("[PointerScanner] No index runs to merge");
            return StatusCode::STATUS_OK;
        }

        if (allRuns.size() == 1)
        {
            m_mergedIndex = std::move(allRuns[0]->store);
            m_mergedIndexEntryCount = allRuns[0]->entryCount;
            m_workerIndexRuns.clear();
            m_logService.log_info(fmt::format("[PointerScanner] Single run, no merge needed: {} entries", m_mergedIndexEntryCount));
            return StatusCode::STATUS_OK;
        }

        m_totalRegions.store(allRuns.size(), std::memory_order_relaxed);
        m_regionsScanned.store(0, std::memory_order_relaxed);

        struct MergeEntry final
        {
            ReverseIndexEntry entry{};
            std::size_t runIndex{};
            std::size_t position{};

            [[nodiscard]] bool operator>(const MergeEntry& other) const
            {
                return entry.pointerValue > other.entry.pointerValue;
            }
        };

        std::priority_queue<MergeEntry, std::vector<MergeEntry>, std::greater<>> heap{};

        std::vector<const ReverseIndexEntry*> runBases(allRuns.size());
        std::vector<std::size_t> runSizes(allRuns.size());

        for (std::size_t i{}; i < allRuns.size(); ++i)
        {
            runBases[i] = static_cast<const ReverseIndexEntry*>(allRuns[i]->store.base());
            runSizes[i] = allRuns[i]->entryCount;

            if (runSizes[i] > 0 && runBases[i] != nullptr)
            {
                heap.push(MergeEntry{.entry = runBases[i][0], .runIndex = i, .position = 0});
            }
        }

        IO::ScanResultStore outputStore{};
        StatusCode status = outputStore.open();
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error("[PointerScanner] Failed to open merged index store");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
        }

        std::size_t totalMerged{};
        constexpr std::size_t MERGE_BATCH_SIZE = 16384;
        std::vector<ReverseIndexEntry> mergeBatch{};
        mergeBatch.reserve(MERGE_BATCH_SIZE);

        while (!heap.empty())
        {
            if (m_scanAbort.load(std::memory_order_acquire))
            {
                return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
            }

            auto [entry, runIndex, position] = heap.top();
            heap.pop();

            mergeBatch.push_back(entry);
            ++totalMerged;

            if (mergeBatch.size() >= MERGE_BATCH_SIZE)
            {
                status = outputStore.append(mergeBatch.data(), mergeBatch.size() * sizeof(ReverseIndexEntry));
                if (status != StatusCode::STATUS_OK)
                {
                    return StatusCode::STATUS_ERROR_POINTER_SCAN_MERGE_FAILED;
                }
                mergeBatch.clear();
            }

            const std::size_t nextPos = position + 1;
            if (nextPos < runSizes[runIndex])
            {
                heap.push(MergeEntry{.entry = runBases[runIndex][nextPos], .runIndex = runIndex, .position = nextPos});
            }
            else
            {
                m_regionsScanned.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if (!mergeBatch.empty())
        {
            status = outputStore.append(mergeBatch.data(), mergeBatch.size() * sizeof(ReverseIndexEntry));
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

        m_mergedIndex = std::move(outputStore);
        m_mergedIndexEntryCount = totalMerged;

        m_workerIndexRuns.clear();
        m_workerIndexBuffers.clear();

        m_logService.log_info(fmt::format("[PointerScanner] Merge complete: {} entries", m_mergedIndexEntryCount));

        return StatusCode::STATUS_OK;
    }
} // namespace Vertex::Scanner
