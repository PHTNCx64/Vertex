//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/scanner/scannerruntimeservice.hh>
#include <vertex/scanner/scanner_command.hh>
#include <vertex/scanner/scanner_event.hh>

#include "../../../mocks/MockIMemoryScanner.hh"
#include "../../../mocks/MockILog.hh"
#include "../../../mocks/MockIThreadDispatcher.hh"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace
{
    StatusCode conv_noop(const char*, NumericSystem, char*, size_t, size_t*) { return STATUS_OK; }
    StatusCode ext_noop(const char*, size_t, char*, size_t) { return STATUS_OK; }
    StatusCode fmt_noop(const char*, char*, size_t) { return STATUS_OK; }
    StatusCode cmp_noop(const char*, const char*, const char*, std::uint8_t* r) { *r = 0; return STATUS_OK; }
}

TEST(MidScanUnloadTest, InvalidateActivePluginTypeFiresCancelledAndReopensSlot)
{
    ScanMode mode{
        .scanModeName = "eq",
        .comparator = cmp_noop,
        .needsInput = 0, .needsPrevious = 0, .reserved = {0, 0},
    };
    DataType type{
        .typeName = "HotSwap",
        .valueSize = 4,
        .converter = conv_noop,
        .extractor = ext_noop,
        .formatter = fmt_noop,
        .scanModes = &mode,
        .scanModeCount = 1,
    };

    NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner> scannerMock{};
    NiceMock<Vertex::Testing::Mocks::MockILog> log{};
    NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher> dispatcher{};

    ON_CALL(dispatcher, schedule_recurring(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto, auto, auto)
                       { return std::unexpected(StatusCode::STATUS_ERROR_NOT_IMPLEMENTED); });
    ON_CALL(dispatcher, schedule_recurring_persistent(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto, auto, auto)
                       { return std::unexpected(StatusCode::STATUS_ERROR_NOT_IMPLEMENTED); });
    ON_CALL(scannerMock, initialize_scan(_, _, _)).WillByDefault(Return(STATUS_OK));
    ON_CALL(scannerMock, get_results_count()).WillByDefault(Return(0));
    ON_CALL(scannerMock, get_last_plugin_error()).WillByDefault(Return(STATUS_OK));

    Vertex::Scanner::ScannerRuntimeService service{scannerMock, dispatcher, log};

    ON_CALL(scannerMock, set_scan_abort_state(true))
        .WillByDefault([&service](bool) { service.on_scan_complete(); });

    
    
    
    std::weak_ptr<const Vertex::Scanner::TypeSchema> weakSchema{};
    EXPECT_CALL(scannerMock, initialize_scan(_, _, _))
        .WillOnce([&](const Vertex::Scanner::ScanConfiguration&,
                      std::shared_ptr<const Vertex::Scanner::TypeSchema> schema,
                      const std::vector<Vertex::Scanner::ScanRegion>&)
                  {
                      weakSchema = schema;
                      return STATUS_OK;
                  });

    Vertex::Runtime::CommandId cancelledId{Vertex::Runtime::INVALID_COMMAND_ID};
    std::atomic<bool> cancelledSeen{false};
    const auto sub = service.subscribe(
        static_cast<Vertex::Scanner::ScannerEventKindMask>(Vertex::Scanner::ScannerEventKind::Cancelled),
        [&](const Vertex::Scanner::ScannerEvent& evt)
        {
            const auto* info = std::get_if<Vertex::Scanner::CancelledInfo>(&evt.detail);
            if (info) cancelledId = info->cancelledId;
            cancelledSeen = true;
        });

    
    const auto regId = service.send_command(
        Vertex::Scanner::service::CmdRegisterType{.sdkType = &type, .sourcePluginIndex = 11},
        std::chrono::seconds{1});
    const auto regResult = service.await_result(regId, std::chrono::seconds{1});
    ASSERT_EQ(regResult.code, STATUS_OK);
    const auto typeId = std::get<Vertex::Scanner::service::RegisterTypeResultPayload>(regResult.payload).id;

    
    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = typeId;
    const auto scanId = service.send_command(
        Vertex::Scanner::service::CmdStartScan{.config = config}, std::chrono::seconds{1});
    ASSERT_NE(scanId, Vertex::Runtime::INVALID_COMMAND_ID);
    EXPECT_TRUE(service.is_scanning());

    
    const auto removed = service.invalidate_plugin_types(11);
    EXPECT_EQ(removed, 1u);

    
    EXPECT_TRUE(cancelledSeen.load());
    EXPECT_EQ(cancelledId, scanId);

    
    service.on_scanner_event(Vertex::Scanner::ScannerEvent{
        .kind = Vertex::Scanner::ScannerEventKind::ScanComplete,
        .detail = Vertex::Scanner::ScanCompleteInfo{
            .matchCount = 0, .elapsed = std::chrono::milliseconds{0}, .typeId = typeId}});
    EXPECT_FALSE(service.is_scanning());

    
    const auto postScanId = service.send_command(
        Vertex::Scanner::service::CmdStartScan{.config = config}, std::chrono::seconds{1});
    const auto postScan = service.await_result(postScanId, std::chrono::seconds{1});
    EXPECT_EQ(postScan.code, StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND);

    service.unsubscribe(sub);
}
