//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <fmt/format.h>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/valueconverter.hh>
#include <vertex/memory/scannerallocator.hh>

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
        m_resultsCount.fetch_add(results.matchesFound, std::memory_order_release);

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::get_scan_results(std::vector<ScanResultEntry>& results, const std::size_t maxResults) const
    {
        std::shared_lock regionsLock(m_writerRegionsMutex);

        if (m_writerRegions.empty())
        {
            return StatusCode::STATUS_ERROR_FILE_NOT_FOUND;
        }

        const std::uint64_t totalResults = m_resultsCount.load(std::memory_order_acquire);
        const std::size_t resultsToRead = std::min(maxResults, static_cast<std::size_t>(totalResults));

        return get_scan_results_locked(results, 0, resultsToRead);
    }

    StatusCode MemoryScanner::get_scan_results_range(std::vector<ScanResultEntry>& results, const std::size_t startIndex, const std::size_t count) const
    {
        std::shared_lock regionsLock(m_writerRegionsMutex);
        return get_scan_results_locked(results, startIndex, count);
    }

    StatusCode MemoryScanner::get_scan_results_locked(std::vector<ScanResultEntry>& results, const std::size_t startIndex, const std::size_t count) const
    {
        const std::uint64_t totalResults = m_resultsCount.load(std::memory_order_acquire);

        if (startIndex >= totalResults)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::size_t actualCount = std::min(count, totalResults - startIndex);
        results.clear();
        results.reserve(actualCount);

        const std::size_t dataSize = m_scanConfig.dataSize;
        const std::size_t firstValueSize = m_scanConfig.firstValueSize;
        const std::size_t recordSize = sizeof(std::uint64_t) + dataSize + firstValueSize;
        std::size_t remainingToRead = actualCount;
        std::size_t currentGlobalIndex = startIndex;
        std::size_t cumulativeResults = 0;

        Memory::AlignedByteVector currentValueBuffer(dataSize);

        std::shared_ptr<IMemoryReader> reader;
        {
            std::scoped_lock lock(m_memoryReaderMutex);
            reader = m_memoryReader;
        }

        for (const auto& writerMeta : m_writerRegions)
        {
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

            const auto* regionBase = static_cast<const char*>(writerMeta.store.base());
            if (regionBase == nullptr)
            {
                cumulativeResults += writerResultCount;
                continue;
            }

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

                if (reader)
                {
                    const StatusCode memReadStatus = reader->read_memory(entry.address, dataSize, currentValueBuffer.data());

                    if (memReadStatus == StatusCode::STATUS_OK)
                    {
                        entry.value.assign(currentValueBuffer.begin(), currentValueBuffer.begin() + dataSize);
                        entry.formattedValue = ValueConverter::format(m_scanConfig.valueType, currentValueBuffer.data(), dataSize, m_scanConfig.hexDisplay, m_scanConfig.endianness);
                    }
                }

                results.push_back(std::move(entry));
            }

            remainingToRead -= resultsInThisRegion;
            currentGlobalIndex += resultsInThisRegion;
            cumulativeResults += writerResultCount;

            if (remainingToRead == 0)
                break;
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
