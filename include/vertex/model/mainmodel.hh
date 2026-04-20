//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/theme.hh>
#include <vertex/log/ilog.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/scanner/iscannerruntimeservice.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/valuetypes.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <sdk/statuscode.h>
#include <sdk/memory.h>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Vertex::Model
{
    struct BulkReadEntry final
    {
        std::uint64_t address{};
        std::uint64_t size{};
        void* buffer{};
    };

    struct BulkWriteEntry final
    {
        std::uint64_t address{};
        std::span<const std::uint8_t> bytes{};
    };

    class MainModel final
    {
      public:
        explicit MainModel(Configuration::ISettings& settingsService, Scanner::IMemoryScanner& memoryService, Scanner::IScannerRuntimeService& scannerService, Runtime::ILoader& loaderService, Log::ILog& loggerService, Thread::IThreadDispatcher& dispatcher);

        [[nodiscard]] StatusCode validate_input(Scanner::ValueType type, bool hexadecimal, std::string_view input, std::vector<std::uint8_t>& output) const;

        [[nodiscard]] StatusCode validate_input(Scanner::TypeId typeId, bool hexadecimal, std::string_view input, std::vector<std::uint8_t>& output) const;

        [[nodiscard]] StatusCode validate_input(Scanner::TypeId typeId, ::NumericSystem numericBase, std::string_view input, std::vector<std::uint8_t>& output) const;

        [[nodiscard]] std::vector<Scanner::TypeSchema> list_scanner_types() const;

        [[nodiscard]] std::optional<Scanner::TypeSchema> find_scanner_type(Scanner::TypeId id) const;

        [[nodiscard]] StatusCode read_process_memory(std::uint64_t address, std::size_t size, std::vector<char>& output) const;
        [[nodiscard]] StatusCode write_process_memory(std::uint64_t address, const std::vector<std::uint8_t>& data) const;
        [[nodiscard]] bool supports_bulk_read() const;
        [[nodiscard]] bool supports_bulk_write() const;
        [[nodiscard]] StatusCode get_bulk_request_limit(std::uint32_t& maxRequestCount) const;
        [[nodiscard]] StatusCode read_process_memory_bulk(std::span<const BulkReadEntry> entries, std::span<BulkReadResult> results) const;
        [[nodiscard]] StatusCode write_process_memory_bulk(std::span<const BulkWriteEntry> entries) const;
        [[nodiscard]] StatusCode query_memory_regions(std::vector<MemoryRegion>& regions) const;
        [[nodiscard]] StatusCode get_file_executable_extensions(std::vector<std::string>& extensions) const;
        [[nodiscard]] StatusCode open_new_process(std::string_view processPath, int argc, const char** argv) const;
        [[nodiscard]] StatusCode get_min_process_address(std::uint64_t& address) const;
        [[nodiscard]] StatusCode get_max_process_address(std::uint64_t& address) const;

        [[nodiscard]] StatusCode initialize_scan(Scanner::ValueType valueType,
                                                 std::uint8_t scanMode,
                                                 bool hexDisplay,
                                                 bool alignmentEnabled,
                                                 std::size_t alignmentValue,
                                                 Scanner::Endianness endianness,
                                                 const std::vector<std::uint8_t>& input,
                                                 const std::vector<std::uint8_t>& input2) const;

        [[nodiscard]] StatusCode initialize_scan(Scanner::TypeId typeId,
                                                 std::uint32_t scanMode,
                                                 bool hexDisplay,
                                                 bool alignmentEnabled,
                                                 std::size_t alignmentValue,
                                                 Scanner::Endianness endianness,
                                                 const std::vector<std::uint8_t>& input,
                                                 const std::vector<std::uint8_t>& input2) const;

        [[nodiscard]] StatusCode initialize_next_scan(Scanner::ValueType valueType,
                                                      std::uint8_t scanMode,
                                                      bool hexDisplay,
                                                      bool alignmentEnabled,
                                                      std::size_t alignmentValue,
                                                      Scanner::Endianness endianness,
                                                      const std::vector<std::uint8_t>& input,
                                                      const std::vector<std::uint8_t>& input2) const;

        [[nodiscard]] StatusCode initialize_next_scan(Scanner::TypeId typeId,
                                                      std::uint32_t scanMode,
                                                      bool hexDisplay,
                                                      bool alignmentEnabled,
                                                      std::size_t alignmentValue,
                                                      Scanner::Endianness endianness,
                                                      const std::vector<std::uint8_t>& input,
                                                      const std::vector<std::uint8_t>& input2) const;

        [[nodiscard]] StatusCode undo_scan() const;
        [[nodiscard]] StatusCode stop_scan() const;
        void finalize_scan() const;
        [[nodiscard]] bool can_undo_scan() const;
        [[nodiscard]] std::uint64_t get_scan_progress_current() const;
        [[nodiscard]] std::uint64_t get_scan_progress_total() const;
        [[nodiscard]] std::uint64_t get_scan_results_count() const;
        [[nodiscard]] StatusCode get_scan_results(std::vector<Scanner::IMemoryScanner::ScanResultEntry>& results, std::size_t maxResults = 10000) const;
        [[nodiscard]] StatusCode get_scan_results_range(std::vector<Scanner::IMemoryScanner::ScanResultEntry>& results, std::size_t startIndex, std::size_t count) const;

        [[nodiscard]] Theme get_theme() const;
        [[nodiscard]] StatusCode is_process_opened() const;
        [[nodiscard]] StatusCode close_process() const;
        [[nodiscard]] StatusCode kill_process() const;

        [[nodiscard]] bool is_scan_complete() const;
        [[nodiscard]] Log::ILog* get_log_service() const;

        [[nodiscard]] int get_ui_state_int(std::string_view key, int defaultValue) const;
        [[nodiscard]] bool get_ui_state_bool(std::string_view key, bool defaultValue) const;
        void set_ui_state_int(std::string_view key, int value) const;
        void set_ui_state_bool(std::string_view key, bool value) const;

      private:
        static constexpr std::string_view MODEL_NAME{"MainModel"};

        void ensure_memory_reader_setup() const;

        Configuration::ISettings& m_settingsService;
        Scanner::IMemoryScanner& m_memoryService;
        Scanner::IScannerRuntimeService& m_scannerService;
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Thread::IThreadDispatcher& m_dispatcher;
    };
} // namespace Vertex::Model
