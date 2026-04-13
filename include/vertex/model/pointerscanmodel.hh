//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/configuration/ipluginconfig.hh>
#include <vertex/log/ilog.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/scanner/pointerscanner/ipointerscanner.hh>
#include <vertex/scanner/pointerscanner/pointerscancomparator.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <filesystem>
#include <vector>

namespace Vertex::Model
{
    class PointerScanModel final
    {
    public:
        explicit PointerScanModel(
            Scanner::IPointerScanner& pointerScanner,
            Runtime::ILoader& loaderService,
            Configuration::IPluginConfig& pluginConfig,
            Log::ILog& logService,
            Thread::IThreadDispatcher& dispatcher
        );

        void set_process_identity(std::uint32_t processId);
        [[nodiscard]] StatusCode start_scan(const Scanner::PointerScanConfig& config) const;
        [[nodiscard]] StatusCode start_rescan() const;
        [[nodiscard]] StatusCode stop_scan() const;
        void finalize_scan() const;

        [[nodiscard]] bool is_scan_complete() const;
        [[nodiscard]] Scanner::PointerScanProgress get_progress() const noexcept;
        [[nodiscard]] std::uint64_t get_node_count() const noexcept;
        [[nodiscard]] std::uint64_t get_edge_count() const noexcept;
        [[nodiscard]] std::uint64_t get_root_count() const noexcept;

        [[nodiscard]] StatusCode get_root_node_ids(
            std::vector<std::uint32_t>& nodeIds,
            std::size_t startIndex,
            std::size_t count) const;

        [[nodiscard]] StatusCode get_roots_range(
            std::vector<Scanner::PointerNodeRecord>& roots,
            std::size_t startIndex,
            std::size_t count) const;

        [[nodiscard]] StatusCode get_children(
            std::uint32_t nodeId,
            std::vector<Scanner::PointerEdgeRecord>& edges,
            std::size_t startIndex,
            std::size_t count) const;

        [[nodiscard]] StatusCode get_node(
            std::uint32_t nodeId,
            Scanner::PointerNodeRecord& node) const;

        [[nodiscard]] const std::vector<Scanner::ModuleRecord>& get_modules() const noexcept;

        [[nodiscard]] StatusCode save_graph(const std::filesystem::path& filePath) const;
        [[nodiscard]] StatusCode load_graph(const std::filesystem::path& filePath) const;
        [[nodiscard]] bool has_graph_data() const noexcept;
        [[nodiscard]] const Scanner::PointerScanConfig& get_config() const noexcept;

        [[nodiscard]] StatusCode compare_with_files(
            const std::vector<std::filesystem::path>& comparisonFiles,
            std::vector<Scanner::PointerPathSignature>& stablePaths) const;

    private:
        void ensure_memory_reader_initialized() const;
        [[nodiscard]] bool has_pointer_scan_memory_attribute_overrides() const;
        [[nodiscard]] StatusCode apply_pointer_scan_memory_attributes() const;
        [[nodiscard]] StatusCode query_memory_regions(std::vector<Scanner::ScanRegion>& regions) const;

        [[nodiscard]] std::uint64_t compute_scan_context_hash() const;

        Scanner::IPointerScanner& m_pointerScanner;
        Runtime::ILoader& m_loaderService;
        Configuration::IPluginConfig& m_pluginConfig;
        Log::ILog& m_logService;
        Thread::IThreadDispatcher& m_dispatcher;

        std::uint32_t m_processId{};
    };
}
