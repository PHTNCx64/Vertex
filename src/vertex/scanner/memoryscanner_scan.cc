//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <numeric>
#include <algorithm>
#include <span>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/comparators.hh>
#include <vertex/memory/scannerallocator.hh>

namespace Vertex::Scanner
{
    void MemoryScanner::resolve_comparator()
    {
        m_resolvedIsString = is_string_type(m_scanConfig.valueType);
        m_resolvedSwapNeeded = needs_endian_swap(m_scanConfig.endianness);
        m_resolvedInput = m_scanConfig.input.empty() ? nullptr : m_scanConfig.input.data();
        m_resolvedInput2 = m_scanConfig.input2.empty() ? nullptr : m_scanConfig.input2.data();

        if (m_resolvedIsString)
        {
            m_resolvedComparator = nullptr;
            return;
        }

        m_resolvedComparator = resolve_scan_comparator(m_scanConfig.valueType, m_scanConfig.get_numeric_scan_mode());
    }

    bool MemoryScanner::check_value_matches(const std::uint8_t* currentData) const
    {
        if (m_resolvedIsString) [[unlikely]]
        {
            return compare_string(m_scanConfig.get_string_scan_mode(),
                reinterpret_cast<const char*>(currentData), m_scanConfig.dataSize,
                reinterpret_cast<const char*>(m_scanConfig.input.data()), m_scanConfig.input.size());
        }

        if (m_resolvedSwapNeeded) [[unlikely]]
        {
            std::array<std::uint8_t, 8> swappedBuffer{};
            std::copy_n(currentData, m_scanConfig.dataSize, swappedBuffer.data());
            std::ranges::reverse(std::span{swappedBuffer.data(), m_scanConfig.dataSize});
            return m_resolvedComparator(swappedBuffer.data(), m_resolvedInput, m_resolvedInput2, nullptr);
        }

        return m_resolvedComparator(currentData, m_resolvedInput, m_resolvedInput2, nullptr);
    }

    bool MemoryScanner::check_value_matches_with_previous(const std::uint8_t* currentData, const std::uint8_t* previousData) const
    {
        if (m_resolvedIsString) [[unlikely]]
        {
            return compare_string(m_scanConfig.get_string_scan_mode(),
                reinterpret_cast<const char*>(currentData), m_scanConfig.dataSize,
                reinterpret_cast<const char*>(m_scanConfig.input.data()), m_scanConfig.input.size());
        }

        if (m_resolvedSwapNeeded) [[unlikely]]
        {
            const auto typeSize = m_scanConfig.dataSize;
            std::array<std::uint8_t, 8> swappedCurrent{};
            std::array<std::uint8_t, 8> swappedPrevious{};

            std::copy_n(currentData, typeSize, swappedCurrent.data());
            std::ranges::reverse(std::span{swappedCurrent.data(), typeSize});

            if (previousData)
            {
                std::copy_n(previousData, typeSize, swappedPrevious.data());
                std::ranges::reverse(std::span{swappedPrevious.data(), typeSize});
            }

            return m_resolvedComparator(swappedCurrent.data(), m_resolvedInput, m_resolvedInput2,
                previousData ? swappedPrevious.data() : nullptr);
        }

        return m_resolvedComparator(currentData, m_resolvedInput, m_resolvedInput2, previousData);
    }

    MemoryScanner::FlatRecordBuffer MemoryScanner::read_records_from_regions(const std::vector<WriterRegionMetadata>& regions, std::size_t startIndex, std::size_t count, std::size_t valueSize, std::size_t firstValueSize) const
    {
        const std::size_t recordSize = sizeof(std::uint64_t) + valueSize + firstValueSize;

        FlatRecordBuffer buffer;
        buffer.valueSize = valueSize;
        buffer.firstValueSize = firstValueSize;
        buffer.recordSize = recordSize;
        buffer.recordCount = 0;
        buffer.data.resize(count * recordSize);

        std::size_t cumulativeResults = 0;
        std::size_t written = 0;

        for (const auto& writerMeta : regions)
        {
            const std::size_t writerResultCount = writerMeta.atomics->resultCount.load(std::memory_order_acquire);

            if (cumulativeResults + writerResultCount <= startIndex)
            {
                cumulativeResults += writerResultCount;
                continue;
            }

            const std::size_t localStartIndex = (startIndex >= cumulativeResults) ? (startIndex - cumulativeResults) : 0;
            const std::size_t resultsInThisRegion = std::min(count - written, writerResultCount - localStartIndex);

            if (resultsInThisRegion == 0)
            {
                break;
            }

            if (!writerMeta.store.is_valid() || writerMeta.store.base() == nullptr)
            {
                cumulativeResults += writerResultCount;
                continue;
            }

            const std::size_t byteOffset = localStartIndex * recordSize;
            const char* srcPtr = static_cast<const char*>(writerMeta.store.base()) + byteOffset;
            const std::size_t bytesToCopy = resultsInThisRegion * recordSize;

            std::memcpy(buffer.data.data() + written * recordSize, srcPtr, bytesToCopy);
            written += resultsInThisRegion;

            cumulativeResults += writerResultCount;

            if (written >= count)
            {
                break;
            }
        }

        buffer.recordCount = written;

        if (written < count)
        {
            buffer.data.resize(written * recordSize);
        }

        return buffer;
    }

    StatusCode MemoryScanner::scan_memory_region(const ScanRegion& region, const std::size_t writerIndex, Memory::AlignedByteVector& regionBuffer)
    {
        constexpr std::size_t BATCH_THRESHOLD = 50000;
        const std::size_t dataSize = m_scanConfig.dataSize;
        const std::size_t alignment = m_scanConfig.alignmentRequired ? m_scanConfig.alignment : 1;

        ScanResult batchResult;
        batchResult.reserve(BATCH_THRESHOLD, dataSize);

        std::shared_ptr<IMemoryReader> reader;
        {
            std::scoped_lock lock(m_memoryReaderMutex);
            reader = m_memoryReader;
        }

        if (!reader)
        {
            if (m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                std::scoped_lock notifyLock(m_mainThreadMutex);
                m_mainThreadWaitCondition.notify_one();
            }
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        if (!m_scanAbort.load(std::memory_order_acquire))
        {
            const std::size_t threadBufferSize = regionBuffer.size();
            const std::size_t numChunks = (region.size + threadBufferSize - 1) / threadBufferSize;

            for (std::size_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
            {
                if (m_scanAbort.load(std::memory_order_acquire))
                {
                    break;
                }

                const std::size_t chunkOffset = chunkIndex * threadBufferSize;
                const std::size_t chunkSize = std::min<std::size_t>(threadBufferSize, region.size - chunkOffset);
                const std::uint64_t chunkBaseAddress = region.baseAddress + chunkOffset;

                const StatusCode status = reader->read_memory(chunkBaseAddress, chunkSize, regionBuffer.data());

                if (status != StatusCode::STATUS_OK)
                {
                    continue;
                }

                const std::size_t scanEnd = (chunkSize >= dataSize) ? chunkSize - dataSize + 1 : 0;

                for (std::size_t offset = 0; offset < scanEnd; offset += alignment)
                {
                    const std::uint8_t* currentData = reinterpret_cast<const std::uint8_t*>(regionBuffer.data()) + offset;

                    if (check_value_matches(currentData))
                    {
                        batchResult.add_match(chunkBaseAddress + offset, currentData, dataSize);

                        if (batchResult.matchesFound >= BATCH_THRESHOLD)
                        {
                            write_results_direct(batchResult, writerIndex);
                            batchResult.clear();
                        }
                    }
                }
            }

            m_regionsScanned.fetch_add(1, std::memory_order_relaxed);
        }

        if (batchResult.matchesFound > 0)
        {
            write_results_direct(batchResult, writerIndex);
        }

        const int remainingReaders = m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remainingReaders == 0)
        {
            std::scoped_lock notifyLock(m_mainThreadMutex);
            m_mainThreadWaitCondition.notify_one();
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode MemoryScanner::scan_previous_results_from_regions(const std::vector<WriterRegionMetadata>& previousRegions,
                                                                  const std::size_t globalStartIndex,
                                                                  const std::size_t totalCount,
                                                                  const std::size_t previousValueSize,
                                                                  const std::size_t previousFirstValueSize,
                                                                  const std::size_t writerIndex)
    {
        constexpr std::size_t RECORDS_PER_BATCH = 100000;
        constexpr std::size_t WRITE_THRESHOLD = 50000;
        const std::size_t dataSize = m_scanConfig.dataSize;
        const std::size_t firstValueSize = m_scanConfig.firstValueSize;
        const bool needsPreviousValue = m_scanConfig.needs_previous_value();

        ScanResult batchResult;
        batchResult.reserve(WRITE_THRESHOLD, dataSize, firstValueSize);

        std::shared_ptr<IMemoryReader> reader;
        {
            std::scoped_lock lock(m_memoryReaderMutex);
            reader = m_memoryReader;
        }

        if (!reader)
        {
            if (m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                std::scoped_lock notifyLock(m_mainThreadMutex);
                m_mainThreadWaitCondition.notify_one();
            }
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        Memory::AlignedByteVector readBuffer{};
        std::size_t processed = 0;

        while (processed < totalCount && !m_scanAbort.load(std::memory_order_acquire))
        {
            const std::size_t batchCount = std::min(RECORDS_PER_BATCH, totalCount - processed);
            FlatRecordBuffer records = read_records_from_regions(previousRegions, globalStartIndex + processed, batchCount, previousValueSize, previousFirstValueSize);
            processed += batchCount;

            if (records.recordCount == 0)
            {
                m_regionsScanned.fetch_add(batchCount, std::memory_order_relaxed);
                continue;
            }

            const std::vector<AddressBundle> bundles = bundle_adjacent_addresses(records, 512);

            for (const auto& [startAddress, endAddress, addresses, previousValuePtrs, firstValuePtrs] : bundles)
            {
                if (m_scanAbort.load(std::memory_order_acquire))
                {
                    break;
                }

                const std::size_t bundleReadSize = (endAddress - startAddress) + dataSize;

                if (readBuffer.size() < bundleReadSize)
                {
                    readBuffer.resize(bundleReadSize);
                }

                const StatusCode readStatus = reader->read_memory(startAddress, bundleReadSize, readBuffer.data());

                if (readStatus != StatusCode::STATUS_OK)
                {
                    for (std::size_t idx = 0; idx < addresses.size(); ++idx)
                    {
                        const std::uint64_t address = addresses[idx];
                        const auto* previousValue = previousValuePtrs[idx];
                        const auto* firstValue = firstValuePtrs[idx];

                        if (readBuffer.size() < dataSize)
                        {
                            readBuffer.resize(dataSize);
                        }

                        const StatusCode individualRead = reader->read_memory(address, dataSize, readBuffer.data());

                        if (individualRead != StatusCode::STATUS_OK)
                        {
                            continue;
                        }

                        const auto* currentData = reinterpret_cast<const std::uint8_t*>(readBuffer.data());
                        bool matches = false;

                        if (needsPreviousValue && previousValue != nullptr)
                        {
                            matches = check_value_matches_with_previous(currentData, previousValue);
                        }
                        else
                        {
                            matches = check_value_matches(currentData);
                        }

                        if (matches)
                        {
                            batchResult.add_match(address, currentData, dataSize, firstValue, firstValueSize);

                            if (batchResult.matchesFound >= WRITE_THRESHOLD)
                            {
                                write_results_direct(batchResult, writerIndex);
                                batchResult.clear();
                            }
                        }
                    }

                    m_regionsScanned.fetch_add(addresses.size(), std::memory_order_relaxed);
                    continue;
                }

                for (std::size_t idx = 0; idx < addresses.size(); ++idx)
                {
                    const std::uint64_t address = addresses[idx];
                    const auto* previousValue = previousValuePtrs[idx];
                    const auto* firstValue = firstValuePtrs[idx];
                    const std::size_t offsetInBuffer = address - startAddress;

                    const auto* currentData = reinterpret_cast<const std::uint8_t*>(readBuffer.data()) + offsetInBuffer;
                    bool matches = false;

                    if (needsPreviousValue && previousValue != nullptr)
                    {
                        matches = check_value_matches_with_previous(currentData, previousValue);
                    }
                    else
                    {
                        matches = check_value_matches(currentData);
                    }

                    if (matches)
                    {
                        batchResult.add_match(address, currentData, dataSize, firstValue, firstValueSize);

                        if (batchResult.matchesFound >= WRITE_THRESHOLD)
                        {
                            write_results_direct(batchResult, writerIndex);
                            batchResult.clear();
                        }
                    }
                }

                m_regionsScanned.fetch_add(addresses.size(), std::memory_order_relaxed);
            }
        }

        if (batchResult.matchesFound > 0)
        {
            write_results_direct(batchResult, writerIndex);
        }

        if (m_activeReaders.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            std::scoped_lock notifyLock(m_mainThreadMutex);
            m_mainThreadWaitCondition.notify_one();
        }

        return StatusCode::STATUS_OK;
    }

    std::vector<MemoryScanner::AddressBundle> MemoryScanner::bundle_adjacent_addresses(const FlatRecordBuffer& records, std::size_t maxGapBytes) const
    {
        std::vector<AddressBundle> bundles;

        if (records.recordCount == 0)
        {
            return bundles;
        }

        std::vector<std::size_t> sortedIndices(records.recordCount);
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
        std::ranges::sort(sortedIndices,
                          [&records](const std::size_t a, const std::size_t b)
                          {
                              return records.get_address(a) < records.get_address(b);
                          });

        const std::size_t firstIdx = sortedIndices[0];
        AddressBundle currentBundle;
        currentBundle.startAddress = records.get_address(firstIdx);
        currentBundle.endAddress = currentBundle.startAddress;
        currentBundle.addresses.push_back(currentBundle.startAddress);
        currentBundle.previousValuePtrs.push_back(records.get_previous_value(firstIdx));
        currentBundle.firstValuePtrs.push_back(records.get_first_value(firstIdx));

        for (std::size_t i = 1; i < sortedIndices.size(); ++i)
        {
            const std::size_t curIdx = sortedIndices[i];
            const std::uint64_t curAddress = records.get_address(curIdx);
            const std::uint64_t prevAddress = records.get_address(sortedIndices[i - 1]);
            const std::uint64_t gap = curAddress - prevAddress;

            if (gap <= maxGapBytes && currentBundle.addresses.size() < 256)
            {
                currentBundle.addresses.push_back(curAddress);
                currentBundle.previousValuePtrs.push_back(records.get_previous_value(curIdx));
                currentBundle.firstValuePtrs.push_back(records.get_first_value(curIdx));
                currentBundle.endAddress = curAddress;
            }
            else
            {
                bundles.push_back(std::move(currentBundle));

                currentBundle = AddressBundle{};
                currentBundle.startAddress = curAddress;
                currentBundle.endAddress = curAddress;
                currentBundle.addresses.push_back(curAddress);
                currentBundle.previousValuePtrs.push_back(records.get_previous_value(curIdx));
                currentBundle.firstValuePtrs.push_back(records.get_first_value(curIdx));
            }
        }

        if (!currentBundle.addresses.empty())
        {
            bundles.push_back(std::move(currentBundle));
        }

        return bundles;
    }

} // namespace Vertex::Scanner
