//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/theme.hh>
#include <vertex/log/ilog.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/valuetypes.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <sdk/statuscode.h>
#include <sdk/memory.h>

#include <string_view>

namespace Vertex::Model
{
    class MainModel final
    {
    public:
        explicit MainModel(
            Configuration::ISettings& settingsService,
            Scanner::IMemoryScanner& memoryService,
            Runtime::ILoader& loaderService,
            Log::ILog& loggerService,
            Thread::IThreadDispatcher& dispatcher
            );

        [[nodiscard]] StatusCode validate_input(Scanner::ValueType type, bool hexadecimal,
                                  std::string_view input,
                                  std::vector<std::uint8_t>& output) const;

        [[nodiscard]] StatusCode read_process_memory(std::uint64_t address, std::size_t size, std::vector<char>& output) const;
        [[nodiscard]] StatusCode write_process_memory(std::uint64_t address, const std::vector<std::uint8_t>& data) const;
        [[nodiscard]] StatusCode query_memory_regions(std::vector<MemoryRegion>& regions) const;
        [[nodiscard]] StatusCode get_file_executable_extensions(std::vector<std::string>& extensions) const;
        [[nodiscard]] StatusCode get_min_process_address(std::uint64_t& address) const;
        [[nodiscard]] StatusCode get_max_process_address(std::uint64_t& address) const;

        [[nodiscard]] StatusCode initialize_scan(Scanner::ValueType valueType, std::uint8_t scanMode,
                                   bool hexDisplay, bool alignmentEnabled, std::size_t alignmentValue,
                                   Scanner::Endianness endianness,
                                   const std::vector<std::uint8_t>& input,
                                   const std::vector<std::uint8_t>& input2) const;

        [[nodiscard]] StatusCode initialize_next_scan(Scanner::ValueType valueType, std::uint8_t scanMode,
                                        bool hexDisplay, bool alignmentEnabled, std::size_t alignmentValue,
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
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Thread::IThreadDispatcher& m_dispatcher;
    };
}
