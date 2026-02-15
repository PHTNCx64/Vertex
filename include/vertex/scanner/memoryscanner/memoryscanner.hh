//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/macrohelp.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/thread/vertexspscthread.hh>
#include <vertex/scanner/scanconfig.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/scanresult.hh>
#include <vertex/io/scanresultstore.hh>
#include <vertex/log/ilog.hh>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <optional>

namespace Vertex::Scanner
{
    struct WriterAtomics final
    {
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> resultCount{};
    };

    struct WriterRegionMetadata final
    {
        std::size_t writerIndex{};
        IO::ScanResultStore store{};
        std::shared_ptr<WriterAtomics> atomics{std::make_shared<WriterAtomics>()};
    };

    struct ScanSnapshot final
    {
        int iteration{};
        std::vector<WriterRegionMetadata> writerRegions{};
        std::uint64_t resultsCount{};
        ScanConfiguration config{};
    };

    class MemoryScanner final : public IMemoryScanner
    {
      public:
        MemoryScanner(Configuration::ISettings& settingsService, Log::ILog& logService);
        ~MemoryScanner() override;

        void set_memory_reader(std::shared_ptr<IMemoryReader> reader) override;
        [[nodiscard]] bool has_memory_reader() const override;

        StatusCode initialize_scan(const ScanConfiguration& configuration, const std::vector<ScanRegion>& memoryRegions) override;
        StatusCode initialize_next_scan(const ScanConfiguration& configuration) override;
        StatusCode undo_scan() override;
        StatusCode stop_scan() override;

        StatusCode get_scan_results_range(std::vector<ScanResultEntry>& results, std::size_t startIndex, std::size_t count) const override;
        StatusCode get_scan_results(std::vector<ScanResultEntry>& results, std::size_t maxResults) const override;

        void set_scan_abort_state(bool state) override;
        bool is_scan_complete() override;
        [[nodiscard]] bool can_undo() const override;
        [[nodiscard]] StatusCode is_scan_active() const;
        void wait_for_scan_completion();

        [[nodiscard]] std::uint64_t get_regions_scanned() const noexcept override;
        [[nodiscard]] std::uint64_t get_total_regions() const noexcept override;
        [[nodiscard]] std::uint64_t get_results_count() const noexcept override;

      private:
        struct PreviousResultRecord final
        {
            std::uint64_t address{};
            std::vector<std::uint8_t> previousValue{};
            std::vector<std::uint8_t> firstValue{};
        };

        StatusCode scan_memory_region(const ScanRegion& region, std::size_t writerIndex);
        StatusCode scan_previous_results(const std::vector<PreviousResultRecord>& previousResults, std::size_t writerIndex);
        StatusCode scan_previous_results_from_regions(
            const std::vector<WriterRegionMetadata>& previousRegions,
            std::size_t globalStartIndex,
            std::size_t totalCount,
            std::size_t previousValueSize,
            std::size_t previousFirstValueSize,
            std::size_t writerIndex);

        [[nodiscard]] bool check_value_matches(const std::uint8_t* currentData) const;
        [[nodiscard]] bool check_value_matches_with_previous(const std::uint8_t* currentData, const std::uint8_t* previousData) const;
        void resolve_comparator();

        StatusCode create_threads(int numReaders);
        void clear_thread_pools();
        StatusCode distribute_regions_to_readers(const std::vector<ScanRegion>& memoryRegions);
        StatusCode enqueue_task_with_fallback(
            std::packaged_task<StatusCode()>&& task,
            std::size_t preferredIndex,
            std::string_view taskLabel) const;
        [[nodiscard]] std::optional<std::size_t> find_available_thread(std::size_t excludeIndex) const;

        StatusCode write_results_direct(const ScanResult& results, std::size_t writerIndex);
        StatusCode get_scan_results_locked(std::vector<ScanResultEntry>& results, std::size_t startIndex, std::size_t count) const;

        struct AddressBundle final
        {
            std::uint64_t startAddress{};
            std::uint64_t endAddress{};
            std::vector<std::uint64_t> addresses{};
            std::vector<std::vector<std::uint8_t>> previousValues{};
            std::vector<std::vector<std::uint8_t>> firstValues{};
        };
        [[nodiscard]] std::vector<AddressBundle> bundle_adjacent_addresses(const std::vector<PreviousResultRecord>& records, std::size_t maxGapBytes = 512) const;

        StatusCode create_writer_regions(std::size_t writerCount);
        void cleanup_writer_regions(std::vector<WriterRegionMetadata>& regions) const;
        void cleanup_snapshot_regions(ScanSnapshot& snapshot) const;
        void save_snapshot_for_undo();

        [[nodiscard]] std::vector<PreviousResultRecord> read_records_from_regions(const std::vector<WriterRegionMetadata>& regions, std::size_t startIndex, std::size_t count, std::size_t valueSize, std::size_t firstValueSize) const;


        // Give each atomic enough space to hold their own CPU cache line to prevent false sharing between threads
        // since it can heavily tank performance through cache invalidation and these atomics are partly in hot paths.

        // NOTE: It should be compiled per CPU architecture since the size of hardware_destructive_interference_size depends on the CPU's cache line size.
        // e.g. x86 usually has 64 byte cache line sizes, Apple Silicon ARM uses 128 bytes.

        MSVC_SUPPRESS_PADDING_WARNING

        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_scanAbort{};
        alignas(std::hardware_destructive_interference_size) std::atomic<int> m_activeReaders{};
        alignas(std::hardware_destructive_interference_size) std::atomic<int> m_activeWriters{};
        alignas(std::hardware_destructive_interference_size) std::atomic<int> m_pendingWriterTasks{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_regionsScanned{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_totalRegions{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_resultsCount{};

        MSVC_END_WARNING_SUPPRESSION

        int m_scanIteration{};
        ScanConfiguration m_scanConfig{};

        using ScanComparatorFn = bool(*)(const void*, const void*, const void*, const void*);
        ScanComparatorFn m_resolvedComparator{};
        const void* m_resolvedInput{};
        const void* m_resolvedInput2{};
        bool m_resolvedSwapNeeded{};
        bool m_resolvedIsString{};

        std::vector<std::unique_ptr<Thread::VertexSPSCThread>> m_readerThreads{};

        mutable std::shared_mutex m_writerRegionsMutex{};
        std::vector<WriterRegionMetadata> m_writerRegions{};

        static constexpr std::size_t MAX_UNDO_DEPTH = 10;
        std::deque<ScanSnapshot> m_undoHistory{};
        mutable std::mutex m_undoHistoryMutex{};

        std::shared_ptr<IMemoryReader> m_memoryReader{};
        mutable std::mutex m_memoryReaderMutex{};

        std::condition_variable m_mainThreadWaitCondition{};
        std::mutex m_mainThreadMutex{};

        Configuration::ISettings& m_settingsService;
        Log::ILog& m_logService;
    };
} // namespace Vertex::Scanner
