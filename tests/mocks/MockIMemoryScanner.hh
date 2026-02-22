#pragma once

#include <gmock/gmock.h>
#include <../../include/vertex/scanner/memoryscanner/imemoryscanner.hh>

namespace Vertex::Testing::Mocks
{
    class MockIMemoryScanner : public Vertex::Scanner::IMemoryScanner
    {
    public:
        ~MockIMemoryScanner() override = default;

        // Memory reader management
        MOCK_METHOD(void, set_memory_reader, (std::shared_ptr<Scanner::IMemoryReader> reader), (override));
        MOCK_METHOD(bool, has_memory_reader, (), (const, override));

        // Scan operations
        MOCK_METHOD(StatusCode, initialize_scan, (const Scanner::ScanConfiguration& configuration, const std::vector<Scanner::ScanRegion>& memoryRegions), (override));
        MOCK_METHOD(StatusCode, initialize_next_scan, (const Scanner::ScanConfiguration& configuration), (override));
        MOCK_METHOD(StatusCode, undo_scan, (), (override));
        MOCK_METHOD(StatusCode, stop_scan, (), (override));
        MOCK_METHOD(void, finalize_scan, (), (override));

        // Progress and state
        MOCK_METHOD(std::uint64_t, get_regions_scanned, (), (const, noexcept, override));
        MOCK_METHOD(std::uint64_t, get_total_regions, (), (const, noexcept, override));
        MOCK_METHOD(std::uint64_t, get_results_count, (), (const, noexcept, override));
        MOCK_METHOD(void, set_scan_abort_state, (bool state), (override));
        MOCK_METHOD(bool, is_scan_complete, (), (override));
        MOCK_METHOD(bool, can_undo, (), (const, override));

        // Results
        MOCK_METHOD(StatusCode, get_scan_results_range, (std::vector<ScanResultEntry>& results, std::size_t startIndex, std::size_t count), (const, override));
        MOCK_METHOD(StatusCode, get_scan_results, (std::vector<ScanResultEntry>& results, std::size_t maxResults), (const, override));
    };
} // namespace Vertex::Testing::Mocks
