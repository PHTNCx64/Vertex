//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/macrohelp.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/scanner/scanconfig.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/scanresult.hh>
#include <vertex/scanner/simd/simd_scanner.hh>
#include <vertex/io/scanresultstore.hh>
#include <vertex/log/ilog.hh>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <deque>

namespace Vertex::Scanner
{
    struct WriterAtomics final
    {
        START_PADDING_WARNING_SUPPRESSION
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> resultCount{};
        END_PADDING_WARNING_SUPPRESSION
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
        std::shared_ptr<std::vector<WriterRegionMetadata>> writerRegions{};
        std::uint64_t resultsCount{};
        ScanConfiguration config{};
    };

    class MemoryScanner final : public IMemoryScanner
    {
      public:
        static constexpr std::chrono::milliseconds SCAN_PROGRESS_MIN_INTERVAL{50};

        MemoryScanner(Configuration::ISettings& settingsService, Log::ILog& logService, Thread::IThreadDispatcher& dispatcher);
        ~MemoryScanner() override;

        void set_memory_reader(std::shared_ptr<IMemoryReader> reader) override;
        void set_scan_completion_callback(std::move_only_function<void()> callback) override;
        void set_scan_progress_callback(std::move_only_function<void()> callback) override;
        [[nodiscard]] bool has_memory_reader() const override;

        StatusCode initialize_scan(const ScanConfiguration& configuration, std::shared_ptr<const TypeSchema> schema, const std::vector<ScanRegion>& memoryRegions) override;
        StatusCode initialize_next_scan(const ScanConfiguration& configuration, std::shared_ptr<const TypeSchema> schema) override;
        StatusCode undo_scan() override;
        StatusCode stop_scan() override;
        void finalize_scan() override;

        StatusCode get_scan_results_range(std::vector<ScanResultEntry>& results, std::size_t startIndex, std::size_t count) const override;
        StatusCode get_scan_results(std::vector<ScanResultEntry>& results, std::size_t maxResults) const override;

        void set_scan_abort_state(bool state) override;
        bool is_scan_complete() override;
        [[nodiscard]] bool can_undo() const override;
        [[nodiscard]] StatusCode is_scan_active() const;
        void wait_for_scan_completion();

        [[nodiscard]] std::uint64_t get_regions_scanned() const noexcept override;
        [[nodiscard]] std::uint64_t get_total_regions() const noexcept override;
        [[nodiscard]] std::uint64_t get_results_count() const override;
        [[nodiscard]] StatusCode get_last_plugin_error() const noexcept override;

      private:
        StatusCode scan_memory_region(const ScanRegion& region, std::size_t writerIndex, IMemoryReader& reader, Memory::AlignedByteVector& regionBuffer);
        StatusCode scan_previous_results_from_regions(std::size_t sortedStartIndex, std::size_t totalCount, std::size_t previousValueSize, std::size_t previousFirstValueSize, std::size_t writerIndex);

        [[nodiscard]] bool check_value_matches(const std::uint8_t* currentData) const;
        [[nodiscard]] bool check_value_matches_with_previous(const std::uint8_t* currentData, const std::uint8_t* previousData) const;
        void resolve_comparator();

        StatusCode create_worker_pool(std::size_t workerCount);
        StatusCode distribute_regions_to_readers(const std::vector<ScanRegion>& memoryRegions);

        StatusCode write_results_direct(const ScanResult& results, std::size_t writerIndex);
        StatusCode get_scan_results_locked(std::vector<ScanResultEntry>& results, std::size_t startIndex, std::size_t count) const;

        struct ChunkDescriptor final
        {
            ScanRegion region{};
            std::size_t chunkOffset{};
            std::size_t chunkSize{};
        };

        struct SortedRecordRef final
        {
            std::uint64_t address{};
            const std::byte* recordPtr{};
        };

        StatusCode create_writer_regions(std::size_t writerCount);
        void cleanup_writer_regions(std::vector<WriterRegionMetadata>& regions) const;
        void cleanup_snapshot_regions(const ScanSnapshot& snapshot) const;
        void save_snapshot_for_undo();
        StatusCode finalize_writer_store(std::size_t writerIndex);
        void reconcile_result_count();
        void notify_scan_completion();
        void notify_scan_progress();
        void notify_scan_progress_throttled();
        [[nodiscard]] bool drain_active_scan();
        StatusCode build_sorted_next_scan_records(const std::vector<WriterRegionMetadata>& previousRegions, std::size_t previousValueSize, std::size_t previousFirstValueSize);

        // Give each atomic enough space to hold their own CPU cache line to prevent false sharing between threads
        // since it can heavily tank performance through cache invalidation and these atomics are partly in hot paths.

        // NOTE: It should be compiled per CPU architecture since the size of hardware_destructive_interference_size depends on the CPU's cache line size.
        // e.g. x86 usually has 64 byte cache line sizes, Apple Silicon ARM uses 128 bytes.

        START_PADDING_WARNING_SUPPRESSION

        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_scanAbort{};
        alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_resultsReconciled{true};
        alignas(std::hardware_destructive_interference_size) std::atomic<StatusCode> m_pluginCallStatus{StatusCode::STATUS_OK};
        alignas(std::hardware_destructive_interference_size) std::atomic<int> m_activeReaders{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_regionsScanned{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_totalRegions{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_resultsCount{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> m_lastProgressNotifyTick{};

        END_PADDING_WARNING_SUPPRESSION

        int m_scanIteration{};
        ScanConfiguration m_scanConfig{};
        std::shared_ptr<const TypeSchema> m_activeSchema{};
        TypeId m_lastScanTypeId{TypeId::Invalid};
        void release_active_schema() noexcept;

        using ScanComparatorFn = bool (*)(const void*, const void*, const void*, const void*);
        ScanComparatorFn m_resolvedComparator{};
        const void* m_resolvedInput{};
        const void* m_resolvedInput2{};
        bool m_resolvedSwapNeeded{};
        bool m_resolvedIsString{};
        bool m_resolvedIsPluginDefined{};
        VertexExtractor_t m_resolvedPluginExtractor{};
        VertexComparator_t m_resolvedPluginComparator{};
        std::size_t m_resolvedPluginValueSize{};
        Simd::SimdScanCapability m_simdCapability{};

        std::size_t m_workerCount{};
        std::vector<ChunkDescriptor> m_allChunks{};

        START_PADDING_WARNING_SUPPRESSION
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_nextChunkIndex{};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_totalChunks{};
        END_PADDING_WARNING_SUPPRESSION

        std::vector<SortedRecordRef> m_sortedNextScanRecords{};

        mutable std::shared_mutex m_writerRegionsMutex{};
        std::vector<WriterRegionMetadata> m_writerRegions{};

        static constexpr std::size_t MAX_UNDO_DEPTH = 10;
        static constexpr std::size_t NEXT_SCAN_CHUNK_SIZE = 4096;
        std::deque<ScanSnapshot> m_undoHistory{};
        mutable std::mutex m_undoHistoryMutex{};

        std::shared_ptr<IMemoryReader> m_memoryReader{};
        mutable std::mutex m_memoryReaderMutex{};
        std::move_only_function<void()> m_scanCompletionCallback{};
        mutable std::mutex m_scanCompletionCallbackMutex{};
        std::move_only_function<void()> m_scanProgressCallback{};
        mutable std::mutex m_scanProgressCallbackMutex{};

        std::condition_variable m_mainThreadWaitCondition{};
        std::mutex m_mainThreadMutex{};
        std::mutex m_scanLifecycleMutex{};

        Configuration::ISettings& m_settingsService;
        Log::ILog& m_logService;
        Thread::IThreadDispatcher& m_dispatcher;
    };
} // namespace Vertex::Scanner
