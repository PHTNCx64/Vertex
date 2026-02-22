//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/scanner/scanconfig.hh>
#include <vertex/scanner/imemoryreader.hh>

#include <vector>
#include <string>
#include <memory>

namespace Vertex::Scanner
{
    struct ScanRegion final
    {
        std::string moduleName{};
        std::uint64_t baseAddress{};
        std::uint64_t size{};
    };

    class IMemoryScanner
    {
    public:
        struct ScanResultEntry final
        {
            std::uint64_t address{};
            std::vector<std::uint8_t> value{};
            std::vector<std::uint8_t> firstValue{};
            std::vector<std::uint8_t> previousValue{};
            std::string formattedValue{};
        };

        virtual ~IMemoryScanner() = default;

        virtual void set_memory_reader(std::shared_ptr<IMemoryReader> reader) = 0;
        [[nodiscard]] virtual bool has_memory_reader() const = 0;

        virtual StatusCode initialize_scan(const ScanConfiguration& configuration, const std::vector<ScanRegion>& memoryRegions) = 0;
        virtual StatusCode initialize_next_scan(const ScanConfiguration& configuration) = 0;
        virtual StatusCode undo_scan() = 0;
        virtual StatusCode stop_scan() = 0;
        virtual void finalize_scan() = 0;

        [[nodiscard]] virtual std::uint64_t get_regions_scanned() const noexcept = 0;
        [[nodiscard]] virtual std::uint64_t get_total_regions() const noexcept = 0;
        [[nodiscard]] virtual std::uint64_t get_results_count() const noexcept = 0;
        virtual void set_scan_abort_state(bool state) = 0;
        virtual bool is_scan_complete() = 0;
        [[nodiscard]] virtual bool can_undo() const = 0;

        virtual StatusCode get_scan_results_range(std::vector<ScanResultEntry>& results, std::size_t startIndex, std::size_t count) const = 0;
        virtual StatusCode get_scan_results(std::vector<ScanResultEntry>& results, std::size_t maxResults) const = 0;
    };
} // namespace Vertex::Scanner
