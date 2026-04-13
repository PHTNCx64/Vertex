//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <fmt/format.h>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/valueconverter.hh>
#include <vertex/memory/scannerallocator.hh>
#include <span>

namespace Vertex::Scanner
{
    StatusCode MemoryScanner::create_writer_regions(std::size_t writerCount)
    {
        std::scoped_lock regionsLock(m_writerRegionsMutex);

        cleanup_writer_regions(m_writerRegions);
        m_writerRegions.clear();
        m_writerRegions.reserve(writerCount);

        for (std::size_t i = 0; i < writerCount; ++i)
        {
            IO::ScanResultStore store{};
            const StatusCode status = store.open();
            if (status != StatusCode::STATUS_OK)
            {
                cleanup_writer_regions(m_writerRegions);
                return status;
            }

            m_writerRegions.push_back(WriterRegionMetadata{.writerIndex = i, .store = std::move(store)});
        }

        return StatusCode::STATUS_OK;
    }

    void MemoryScanner::cleanup_writer_regions(std::vector<WriterRegionMetadata>& regions) const { regions.clear(); }

    StatusCode MemoryScanner::write_results_direct(const ScanResult& results, const std::size_t writerIndex)
    {
        if (results.matchesFound == 0)
        {
            return StatusCode::STATUS_OK;
        }

        if (m_scanAbort.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_OK;
        }

        WriterRegionMetadata& writerMeta = m_writerRegions[writerIndex];

        const std::size_t totalDataSize = results.total_data_size();
        const StatusCode appendStatus = writerMeta.store.append(results.data(), totalDataSize);
        if (appendStatus != StatusCode::STATUS_OK)
        {
            return appendStatus;
        }

        writerMeta.atomics->resultCount.fetch_add(results.matchesFound, std::memory_order_release);

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::get_scan_results(std::vector<ScanResultEntry>& results, const std::size_t maxResults) const
    {
        std::shared_lock regionsLock(m_writerRegionsMutex);

        if (m_writerRegions.empty())
        {
            return StatusCode::STATUS_ERROR_FILE_NOT_FOUND;
        }

        // Keep this path consistent with get_scan_results_range() by relying on
        // per-region readable counts inside get_scan_results_locked().
        return get_scan_results_locked(results, 0, maxResults);
    }

    StatusCode MemoryScanner::get_scan_results_range(std::vector<ScanResultEntry>& results, const std::size_t startIndex, const std::size_t count) const
    {
        std::shared_lock regionsLock(m_writerRegionsMutex);
        return get_scan_results_locked(results, startIndex, count);
    }

    StatusCode MemoryScanner::get_scan_results_locked(std::vector<ScanResultEntry>& results, const std::size_t startIndex, const std::size_t count) const
    {
        std::uint64_t readableResults = 0;
        for (const auto& writerMeta : m_writerRegions)
        {
            if (writerMeta.store.is_valid() && writerMeta.store.base() != nullptr)
            {
                readableResults += writerMeta.atomics->resultCount.load(std::memory_order_acquire);
            }
        }

        if (readableResults == 0)
        {
            results.clear();
            return StatusCode::STATUS_OK;
        }

        if (startIndex >= readableResults)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::size_t actualCount = std::min(count, static_cast<std::size_t>(readableResults - startIndex));
        results.clear();
        results.reserve(actualCount);

        const std::size_t dataSize = m_scanConfig.dataSize;
        const std::size_t firstValueSize = m_scanConfig.firstValueSize;
        const std::size_t recordSize = sizeof(std::uint64_t) + dataSize + firstValueSize;
        std::size_t remainingToRead = actualCount;
        std::size_t currentGlobalIndex = startIndex;
        std::size_t cumulativeResults = 0;

        std::shared_ptr<IMemoryReader> reader;
        {
            std::scoped_lock lock(m_memoryReaderMutex);
            reader = m_memoryReader;
        }

        for (const auto& writerMeta : m_writerRegions)
        {
            if (!writerMeta.store.is_valid())
            {
                continue;
            }

            const auto* regionBase = static_cast<const char*>(writerMeta.store.base());
            if (regionBase == nullptr)
            {
                continue;
            }

            const std::size_t writerResultCount = writerMeta.atomics->resultCount.load(std::memory_order_acquire);

            if (cumulativeResults + writerResultCount <= startIndex)
            {
                cumulativeResults += writerResultCount;
                continue;
            }

            const std::size_t localStartIndex = (currentGlobalIndex >= cumulativeResults) ? (currentGlobalIndex - cumulativeResults) : 0;
            const std::size_t resultsInThisRegion = std::min(remainingToRead, writerResultCount - localStartIndex);

            if (resultsInThisRegion == 0)
                break;

            const std::size_t byteOffset = localStartIndex * recordSize;
            auto readPtr = regionBase + byteOffset;

            for (std::size_t i = 0; i < resultsInThisRegion; ++i)
            {
                ScanResultEntry entry;

                std::copy_n(readPtr, sizeof(std::uint64_t), reinterpret_cast<char*>(&entry.address));
                readPtr += sizeof(std::uint64_t);

                entry.previousValue.assign(reinterpret_cast<const std::uint8_t*>(readPtr), reinterpret_cast<const std::uint8_t*>(readPtr) + dataSize);
                readPtr += dataSize;

                if (firstValueSize > 0)
                {
                    entry.firstValue.assign(reinterpret_cast<const std::uint8_t*>(readPtr), reinterpret_cast<const std::uint8_t*>(readPtr) + firstValueSize);
                    readPtr += firstValueSize;
                }

                results.push_back(std::move(entry));
            }

            remainingToRead -= resultsInThisRegion;
            currentGlobalIndex += resultsInThisRegion;
            cumulativeResults += writerResultCount;

            if (remainingToRead == 0)
                break;
        }

        if (reader && !results.empty() && dataSize > 0)
        {
            if (reader->supports_bulk_read())
            {
                std::size_t maxBulkRequests = static_cast<std::size_t>(std::max(1, m_settingsService.get_int("bulk.maxRequestSize", 4096)));
                const std::uint32_t readerLimit = reader->bulk_request_limit();
                if (readerLimit > 0)
                {
                    maxBulkRequests = std::min(maxBulkRequests, static_cast<std::size_t>(readerLimit));
                }
                maxBulkRequests = std::max<std::size_t>(1, maxBulkRequests);

                std::vector<std::vector<std::uint8_t>> bulkValueBuffers(results.size(), std::vector<std::uint8_t>(dataSize));
                std::vector<BulkReadRequest> bulkRequests(results.size());
                std::vector<BulkReadResult> bulkResults(results.size());
                std::vector<std::uint8_t> readSuccess(results.size(), 0);

                for (std::size_t i{}; i < results.size(); ++i)
                {
                    bulkRequests[i] = {
                        results[i].address,
                        dataSize,
                        bulkValueBuffers[i].data()
                    };
                    bulkResults[i].status = StatusCode::STATUS_OK;
                }

                std::size_t offset{};
                while (offset < results.size())
                {
                    const std::size_t chunkCount = std::min(maxBulkRequests, results.size() - offset);
                    const auto requestSpan = std::span<const BulkReadRequest>(bulkRequests.data() + offset, chunkCount);
                    auto resultSpan = std::span<BulkReadResult>(bulkResults.data() + offset, chunkCount);
                    const StatusCode bulkStatus = reader->read_memory_bulk(requestSpan, resultSpan);

                    if (bulkStatus != StatusCode::STATUS_OK)
                    {
                        Memory::AlignedByteVector singleReadBuffer(dataSize);
                        for (std::size_t i = offset; i < offset + chunkCount; ++i)
                        {
                            const StatusCode readStatus = reader->read_memory(results[i].address, dataSize, singleReadBuffer.data());
                            if (readStatus != StatusCode::STATUS_OK)
                            {
                                continue;
                            }

                            std::copy_n(singleReadBuffer.begin(), dataSize, bulkValueBuffers[i].begin());
                            readSuccess[i] = 1;
                        }
                    }
                    else
                    {
                        for (std::size_t i = offset; i < offset + chunkCount; ++i)
                        {
                            if (bulkResults[i].status == StatusCode::STATUS_OK)
                            {
                                readSuccess[i] = 1;
                            }
                        }
                    }

                    offset += chunkCount;
                }

                for (std::size_t i{}; i < results.size(); ++i)
                {
                    if (readSuccess[i] == 0)
                    {
                        continue;
                    }

                    results[i].value = bulkValueBuffers[i];
                    results[i].formattedValue = ValueConverter::format(
                        m_scanConfig.valueType,
                        bulkValueBuffers[i].data(),
                        dataSize,
                        m_scanConfig.hexDisplay,
                        m_scanConfig.endianness);
                }
            }
            else
            {
                Memory::AlignedByteVector currentValueBuffer(dataSize);
                for (auto& entry : results)
                {
                    const StatusCode memReadStatus = reader->read_memory(entry.address, dataSize, currentValueBuffer.data());
                    if (memReadStatus != StatusCode::STATUS_OK)
                    {
                        continue;
                    }

                    entry.value.assign(currentValueBuffer.begin(), currentValueBuffer.begin() + dataSize);
                    entry.formattedValue = ValueConverter::format(
                        m_scanConfig.valueType,
                        currentValueBuffer.data(),
                        dataSize,
                        m_scanConfig.hexDisplay,
                        m_scanConfig.endianness);
                }
            }
        }

        if (results.size() != actualCount)
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
