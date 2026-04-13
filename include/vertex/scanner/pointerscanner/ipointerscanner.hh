//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/scanner/pointerscanner/pointerscannerconfig.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/imemoryreader.hh>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace Vertex::Scanner
{
    class IPointerScanner
    {
    public:
        virtual ~IPointerScanner() = default;

        virtual void set_memory_reader(std::shared_ptr<IMemoryReader> reader) = 0;
        [[nodiscard]] virtual bool has_memory_reader() const = 0;

        virtual StatusCode initialize_scan(const PointerScanConfig& config, const std::vector<ScanRegion>& memoryRegions) = 0;
        virtual StatusCode initialize_rescan() = 0;
        virtual StatusCode stop_scan() = 0;
        virtual void finalize_scan() = 0;

        [[nodiscard]] virtual bool is_scan_complete() = 0;
        [[nodiscard]] virtual StatusCode get_scan_status() const noexcept = 0;
        [[nodiscard]] virtual PointerScanProgress get_progress() const noexcept = 0;

        [[nodiscard]] virtual std::uint64_t get_node_count() const noexcept = 0;
        [[nodiscard]] virtual std::uint64_t get_edge_count() const noexcept = 0;

        [[nodiscard]] virtual std::uint64_t get_root_count() const noexcept = 0;
        [[nodiscard]] virtual StatusCode get_roots_range(std::vector<PointerNodeRecord>& roots, std::size_t startIndex, std::size_t count) const = 0;
        [[nodiscard]] virtual StatusCode get_root_node_ids(std::vector<std::uint32_t>& nodeIds, std::size_t startIndex, std::size_t count) const = 0;
        [[nodiscard]] virtual StatusCode get_children(std::uint32_t nodeId, std::vector<PointerEdgeRecord>& edges, std::size_t startIndex, std::size_t count) const = 0;
        [[nodiscard]] virtual StatusCode get_node(std::uint32_t nodeId, PointerNodeRecord& node) const = 0;
        [[nodiscard]] virtual const std::vector<ModuleRecord>& get_modules() const noexcept = 0;

        [[nodiscard]] virtual StatusCode get_nodes_range(std::vector<PointerNodeRecord>& nodes, std::size_t startIndex, std::size_t count) const = 0;
        [[nodiscard]] virtual StatusCode get_edges_range(std::vector<PointerEdgeRecord>& edges, std::size_t startIndex, std::size_t count) const = 0;
        [[nodiscard]] virtual StatusCode get_parent_ranges(std::vector<ParentEdgeRange>& ranges, std::size_t startIndex, std::size_t count) const = 0;

        [[nodiscard]] virtual StatusCode save_graph(const std::filesystem::path& filePath) const = 0;
        [[nodiscard]] virtual StatusCode load_graph(const std::filesystem::path& filePath) = 0;
        [[nodiscard]] virtual const PointerScanConfig& get_config() const noexcept = 0;
    };
} // namespace Vertex::Scanner
