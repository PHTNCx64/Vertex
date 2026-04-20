//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <algorithm>
#include <atomic>
#include <span>
#include <vector>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/comparators.hh>
#include <vertex/memory/scannerallocator.hh>
#include <vertex/runtime/caller.hh>

namespace Vertex::Scanner
{
    namespace
    {
        thread_local std::vector<char> tl_pluginCurrent{};
        thread_local std::vector<char> tl_pluginPrevious{};

        void record_first_plugin_failure(std::atomic<StatusCode>& statusSlot,
                                          std::atomic<bool>& abortSlot,
                                          StatusCode failure)
        {
            StatusCode expected{StatusCode::STATUS_OK};
            if (statusSlot.compare_exchange_strong(expected, failure,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire))
            {
                abortSlot.store(true, std::memory_order_release);
            }
        }

        [[nodiscard]] bool invoke_plugin_comparator(VertexExtractor_t extractor,
                                                     VertexComparator_t comparator,
                                                     std::size_t valueSize,
                                                     const std::uint8_t* currentData,
                                                     const std::uint8_t* previousData,
                                                     const void* userInput,
                                                     std::atomic<StatusCode>& statusSlot,
                                                     std::atomic<bool>& abortSlot)
        {
            if (tl_pluginCurrent.size() < valueSize)
            {
                tl_pluginCurrent.assign(valueSize, 0);
            }
            if (previousData && tl_pluginPrevious.size() < valueSize)
            {
                tl_pluginPrevious.assign(valueSize, 0);
            }

            const auto extractCurrent = Runtime::safe_call(
                extractor,
                reinterpret_cast<const char*>(currentData),
                valueSize,
                tl_pluginCurrent.data(),
                valueSize);
            if (!Runtime::status_ok(extractCurrent))
            {
                record_first_plugin_failure(statusSlot, abortSlot, Runtime::get_status(extractCurrent));
                return false;
            }

            const char* previousExtracted{};
            if (previousData)
            {
                const auto extractPrevious = Runtime::safe_call(
                    extractor,
                    reinterpret_cast<const char*>(previousData),
                    valueSize,
                    tl_pluginPrevious.data(),
                    valueSize);
                if (!Runtime::status_ok(extractPrevious))
                {
                    record_first_plugin_failure(statusSlot, abortSlot, Runtime::get_status(extractPrevious));
                    return false;
                }
                previousExtracted = tl_pluginPrevious.data();
            }

            std::uint8_t matchResult{};
            const auto compareCall = Runtime::safe_call(
                comparator,
                tl_pluginCurrent.data(),
                previousExtracted,
                static_cast<const char*>(userInput),
                &matchResult);
            if (!Runtime::status_ok(compareCall))
            {
                record_first_plugin_failure(statusSlot, abortSlot, Runtime::get_status(compareCall));
                return false;
            }
            return matchResult != 0;
        }
    }

    void MemoryScanner::resolve_comparator()
    {
        m_resolvedIsString = is_string_type(m_scanConfig.valueType);
        m_resolvedSwapNeeded = needs_endian_swap(m_scanConfig.endianness);
        m_resolvedInput = m_scanConfig.input.empty() ? nullptr : m_scanConfig.input.data();
        m_resolvedInput2 = m_scanConfig.input2.empty() ? nullptr : m_scanConfig.input2.data();
        m_simdCapability = {};
        m_resolvedIsPluginDefined = false;
        m_resolvedPluginExtractor = nullptr;
        m_resolvedPluginComparator = nullptr;
        m_resolvedPluginValueSize = 0;

        if (m_activeSchema && m_activeSchema->kind == TypeKind::PluginDefined)
        {
            m_resolvedIsPluginDefined = true;
            m_resolvedPluginExtractor = m_activeSchema->sdkType->extractor;
            m_resolvedPluginComparator = m_activeSchema->sdkType->scanModes[m_scanConfig.scanMode].comparator;
            m_resolvedPluginValueSize = m_activeSchema->valueSize;
            m_resolvedComparator = nullptr;
            return;
        }

        if (m_resolvedIsString)
        {
            m_resolvedComparator = nullptr;
            return;
        }

        if (!m_resolvedSwapNeeded)
        {
            m_simdCapability = Simd::resolve_simd_scanner(m_scanConfig.valueType, m_scanConfig.get_numeric_scan_mode());
        }

        m_resolvedComparator = resolve_scan_comparator(m_scanConfig.valueType, m_scanConfig.get_numeric_scan_mode());
    }

    bool MemoryScanner::check_value_matches(const std::uint8_t* currentData) const
    {
        if (m_resolvedIsPluginDefined) [[unlikely]]
        {
            return invoke_plugin_comparator(m_resolvedPluginExtractor,
                                             m_resolvedPluginComparator,
                                             m_resolvedPluginValueSize,
                                             currentData,
                                             nullptr,
                                             m_resolvedInput,
                                             const_cast<std::atomic<StatusCode>&>(m_pluginCallStatus),
                                             const_cast<std::atomic<bool>&>(m_scanAbort));
        }

        if (m_resolvedIsString) [[unlikely]]
        {
            return compare_string(m_scanConfig.get_string_scan_mode(), reinterpret_cast<const char*>(currentData), m_scanConfig.dataSize, reinterpret_cast<const char*>(m_scanConfig.input.data()),
                                  m_scanConfig.input.size());
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
        if (m_resolvedIsPluginDefined) [[unlikely]]
        {
            return invoke_plugin_comparator(m_resolvedPluginExtractor,
                                             m_resolvedPluginComparator,
                                             m_resolvedPluginValueSize,
                                             currentData,
                                             previousData,
                                             m_resolvedInput,
                                             const_cast<std::atomic<StatusCode>&>(m_pluginCallStatus),
                                             const_cast<std::atomic<bool>&>(m_scanAbort));
        }

        if (m_resolvedIsString) [[unlikely]]
        {
            return compare_string(m_scanConfig.get_string_scan_mode(), reinterpret_cast<const char*>(currentData), m_scanConfig.dataSize, reinterpret_cast<const char*>(m_scanConfig.input.data()),
                                  m_scanConfig.input.size());
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

            return m_resolvedComparator(swappedCurrent.data(), m_resolvedInput, m_resolvedInput2, previousData ? swappedPrevious.data() : nullptr);
        }

        return m_resolvedComparator(currentData, m_resolvedInput, m_resolvedInput2, previousData);
    }

    StatusCode MemoryScanner::scan_memory_region(const ScanRegion& region, const std::size_t writerIndex, IMemoryReader& reader, Memory::AlignedByteVector& regionBuffer)
    {
        constexpr std::size_t BATCH_THRESHOLD = Simd::BATCH_CHECK_INTERVAL;
        const std::size_t dataSize = m_scanConfig.dataSize;
        const std::size_t alignment = m_scanConfig.alignmentRequired ? m_scanConfig.alignment : 1;

        ScanResult batchResult;
        batchResult.reserve(BATCH_THRESHOLD, dataSize);

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

                const StatusCode status = reader.read_memory(chunkBaseAddress, chunkSize, regionBuffer.data());

                if (status == StatusCode::STATUS_OK)
                {
                    const std::size_t scanEnd = (chunkSize >= dataSize) ? chunkSize - dataSize + 1 : 0;
                    const auto* chunkData = reinterpret_cast<const std::uint8_t*>(regionBuffer.data());

                    if (m_simdCapability.available && alignment == dataSize && chunkSize >= dataSize)
                    {
                        std::size_t offset{};
                        while (offset < chunkSize)
                        {
                            const std::size_t consumed = m_simdCapability.scanFn(chunkData + offset, chunkSize - offset, alignment, dataSize, static_cast<const std::uint8_t*>(m_resolvedInput),
                                                                                 static_cast<const std::uint8_t*>(m_resolvedInput2), batchResult, chunkBaseAddress + offset);

                            offset += consumed;

                            if (batchResult.matchesFound >= BATCH_THRESHOLD)
                            {
                                if (write_results_direct(batchResult, writerIndex) != StatusCode::STATUS_OK)
                                {
                                    m_scanAbort.store(true, std::memory_order_release);
                                    break;
                                }
                                batchResult.clear();
                            }
                        }
                    }
                    else
                    {
                        for (std::size_t offset = 0; offset < scanEnd; offset += alignment)
                        {
                            if (m_scanAbort.load(std::memory_order_acquire)) [[unlikely]]
                            {
                                break;
                            }

                            const std::uint8_t* currentData = chunkData + offset;

                            if (check_value_matches(currentData))
                            {
                                batchResult.add_match(chunkBaseAddress + offset, currentData, dataSize);

                                if (batchResult.matchesFound >= BATCH_THRESHOLD)
                                {
                                    if (write_results_direct(batchResult, writerIndex) != StatusCode::STATUS_OK)
                                    {
                                        m_scanAbort.store(true, std::memory_order_release);
                                        break;
                                    }
                                    batchResult.clear();
                                }
                            }
                        }
                    }
                }

                m_regionsScanned.fetch_add(1, std::memory_order_relaxed);
                notify_scan_progress_throttled();
            }
        }

        if (batchResult.matchesFound > 0 && !m_scanAbort.load(std::memory_order_acquire))
        {
            if (write_results_direct(batchResult, writerIndex) != StatusCode::STATUS_OK)
            {
                m_scanAbort.store(true, std::memory_order_release);
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode
    MemoryScanner::scan_previous_results_from_regions(const std::size_t sortedStartIndex, const std::size_t totalCount, const std::size_t previousValueSize, const std::size_t previousFirstValueSize, const std::size_t writerIndex)
    {
        constexpr std::size_t WRITE_THRESHOLD = 50000;
        const std::size_t dataSize = m_scanConfig.dataSize;
        const std::size_t firstValueSize = m_scanConfig.firstValueSize;
        const bool needsPreviousValue = m_scanConfig.needs_previous_value();

        ScanResult batchResult;
        batchResult.reserve(std::min(WRITE_THRESHOLD, totalCount), dataSize, firstValueSize);

        std::shared_ptr<IMemoryReader> reader;
        {
            std::scoped_lock lock(m_memoryReaderMutex);
            reader = m_memoryReader;
        }

        if (!reader)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        if (totalCount == 0 || sortedStartIndex >= m_sortedNextScanRecords.size())
        {
            return StatusCode::STATUS_OK;
        }

        const std::size_t sortedEndIndex = std::min(sortedStartIndex + totalCount, m_sortedNextScanRecords.size());

        Memory::AlignedByteVector readBuffer{};
        const bool supportsBulkRead = reader->supports_bulk_read();
        std::size_t maxBulkRequests = static_cast<std::size_t>(std::max(1, m_settingsService.get_int("bulk.maxRequestSize", 4096)));
        if (supportsBulkRead)
        {
            const std::uint32_t readerLimit = reader->bulk_request_limit();
            if (readerLimit > 0)
            {
                maxBulkRequests = std::min(maxBulkRequests, static_cast<std::size_t>(readerLimit));
            }
            maxBulkRequests = std::max<std::size_t>(1, maxBulkRequests);
        }

        std::vector<std::uint64_t> addresses;
        std::vector<const std::uint8_t*> previousValuePtrs;
        std::vector<const std::uint8_t*> firstValuePtrs;
        addresses.reserve(256);
        previousValuePtrs.reserve(256);
        firstValuePtrs.reserve(256);

        std::size_t cursor = sortedStartIndex;
        while (cursor < sortedEndIndex && !m_scanAbort.load(std::memory_order_acquire))
        {
            addresses.clear();
            previousValuePtrs.clear();
            firstValuePtrs.clear();

            while (cursor < sortedEndIndex && addresses.size() < 256)
            {
                const auto& recordRef = m_sortedNextScanRecords[cursor];
                if (!addresses.empty())
                {
                    const std::uint64_t gap = recordRef.address - addresses.back();
                    if (gap > 512)
                    {
                        break;
                    }
                }

                const auto* previousValue = reinterpret_cast<const std::uint8_t*>(recordRef.recordPtr + sizeof(std::uint64_t));
                const auto* firstValue = (previousFirstValueSize == 0) ? previousValue : (previousValue + previousValueSize);
                addresses.push_back(recordRef.address);
                previousValuePtrs.push_back(previousValue);
                firstValuePtrs.push_back(firstValue);
                ++cursor;
            }

            if (addresses.empty())
            {
                continue;
            }

            auto process_match = [&](const std::uint64_t address, const std::uint8_t* currentData, const std::uint8_t* previousValue, const std::uint8_t* firstValue) -> bool
            {
                if (m_scanAbort.load(std::memory_order_acquire)) [[unlikely]]
                {
                    return false;
                }

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
                        if (write_results_direct(batchResult, writerIndex) != StatusCode::STATUS_OK)
                        {
                            m_scanAbort.store(true, std::memory_order_release);
                            return false;
                        }
                        batchResult.clear();
                    }
                }

                return true;
            };

            const std::uint64_t startAddress = addresses.front();
            const std::uint64_t endAddress = addresses.back();
            const std::size_t bundleReadSize = (endAddress - startAddress) + dataSize;

            if (readBuffer.size() < bundleReadSize)
            {
                readBuffer.resize(bundleReadSize);
            }

            const StatusCode readStatus = reader->read_memory(startAddress, bundleReadSize, readBuffer.data());

            if (readStatus != StatusCode::STATUS_OK)
            {
                if (supportsBulkRead)
                {
                    std::vector<std::uint8_t> bulkReadBuffer(addresses.size() * dataSize);
                    std::vector<BulkReadRequest> requests(addresses.size());
                    std::vector<BulkReadResult> results(addresses.size());

                    for (std::size_t idx = 0; idx < addresses.size(); ++idx)
                    {
                        requests[idx] = {addresses[idx], dataSize, bulkReadBuffer.data() + (idx * dataSize)};
                        results[idx].status = StatusCode::STATUS_OK;
                    }

                    std::size_t offset{};
                    while (offset < addresses.size() && !m_scanAbort.load(std::memory_order_acquire))
                    {
                        const std::size_t chunkCount = std::min(maxBulkRequests, addresses.size() - offset);
                        const auto requestSpan = std::span<const BulkReadRequest>(requests.data() + offset, chunkCount);
                        auto resultSpan = std::span<BulkReadResult>(results.data() + offset, chunkCount);
                        const StatusCode bulkStatus = reader->read_memory_bulk(requestSpan, resultSpan);

                        if (bulkStatus != StatusCode::STATUS_OK)
                        {
                            for (std::size_t idx = offset; idx < offset + chunkCount; ++idx)
                            {
                                if (m_scanAbort.load(std::memory_order_acquire))
                                {
                                    break;
                                }

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
                                if (!process_match(address, currentData, previousValue, firstValue))
                                {
                                    break;
                                }
                            }
                        }
                        else
                        {
                            for (std::size_t idx = offset; idx < offset + chunkCount; ++idx)
                            {
                                if (results[idx].status != StatusCode::STATUS_OK)
                                {
                                    continue;
                                }

                                const std::uint64_t address = addresses[idx];
                                const auto* previousValue = previousValuePtrs[idx];
                                const auto* firstValue = firstValuePtrs[idx];
                                const auto* currentData = bulkReadBuffer.data() + (idx * dataSize);
                                if (!process_match(address, currentData, previousValue, firstValue))
                                {
                                    break;
                                }
                            }
                        }

                        offset += chunkCount;
                    }
                }
                else
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
                        if (!process_match(address, currentData, previousValue, firstValue))
                        {
                            break;
                        }
                    }
                }

                m_regionsScanned.fetch_add(addresses.size(), std::memory_order_relaxed);
                notify_scan_progress_throttled();
                continue;
            }

            for (std::size_t idx = 0; idx < addresses.size(); ++idx)
            {
                const std::uint64_t address = addresses[idx];
                const auto* previousValue = previousValuePtrs[idx];
                const auto* firstValue = firstValuePtrs[idx];
                const std::size_t offsetInBuffer = address - startAddress;

                const auto* currentData = reinterpret_cast<const std::uint8_t*>(readBuffer.data()) + offsetInBuffer;
                if (!process_match(address, currentData, previousValue, firstValue))
                {
                    break;
                }
            }

            m_regionsScanned.fetch_add(addresses.size(), std::memory_order_relaxed);
            notify_scan_progress_throttled();
        }

        if (batchResult.matchesFound > 0 && !m_scanAbort.load(std::memory_order_acquire))
        {
            if (write_results_direct(batchResult, writerIndex) != StatusCode::STATUS_OK)
            {
                m_scanAbort.store(true, std::memory_order_release);
            }
        }

        return StatusCode::STATUS_OK;
    }

} // namespace Vertex::Scanner
