//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <gmock/gmock.h>
#include <../../include/vertex/scanner/iscannerruntimeservice.hh>

namespace Vertex::Testing::Mocks
{
    class MockIScannerRuntimeService : public Vertex::Scanner::IScannerRuntimeService
    {
      public:
        ~MockIScannerRuntimeService() override = default;

        MOCK_METHOD(Vertex::Runtime::CommandId, send_command,
                    (Vertex::Scanner::service::Command command, std::chrono::milliseconds timeout), (override));
        MOCK_METHOD(void, subscribe_result,
                    (Vertex::Runtime::CommandId id, ResultCallback callback), (override));
        MOCK_METHOD(Vertex::Scanner::service::CommandResult, await_result,
                    (Vertex::Runtime::CommandId id, std::chrono::milliseconds timeout), (override));
        MOCK_METHOD(Vertex::Runtime::SubscriptionId, subscribe,
                    (Vertex::Scanner::ScannerEventKindMask mask, EventCallback callback), (override));
        MOCK_METHOD(void, unsubscribe,
                    (Vertex::Runtime::SubscriptionId subscriptionId), (noexcept, override));

        MOCK_METHOD(std::uint64_t, results_count, (), (const, override));
        MOCK_METHOD(StatusCode, snapshot_results,
                    (std::vector<Vertex::Scanner::IMemoryScanner::ScanResultEntry>& out,
                     std::size_t startIndex, std::size_t count), (const, override));
        MOCK_METHOD(bool, can_undo, (), (const, override));
        MOCK_METHOD(bool, is_scanning, (), (const, override));

        MOCK_METHOD(std::optional<Vertex::Scanner::TypeSchema>, find_type,
                    (Vertex::Scanner::TypeId id), (const, override));
        MOCK_METHOD(std::vector<Vertex::Scanner::TypeSchema>, list_types, (), (const, override));
        MOCK_METHOD(std::uint32_t, invalidate_plugin_types,
                    (std::size_t pluginIndex), (override));

        MOCK_METHOD(void, on_scanner_event, (Vertex::Scanner::ScannerEvent event), (override));
        MOCK_METHOD(void, on_scanner_command_result,
                    (Vertex::Scanner::service::CommandResult result), (override));

        MOCK_METHOD(void, shutdown, (), (override));
    };
}
