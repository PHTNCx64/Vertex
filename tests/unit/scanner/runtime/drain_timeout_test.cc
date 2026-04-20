//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/scanner/scannerruntimeservice.hh>
#include <vertex/scanner/scanner_command.hh>

#include "../../../mocks/MockIMemoryScanner.hh"
#include "../../../mocks/MockILog.hh"
#include "../../../mocks/MockIThreadDispatcher.hh"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    StatusCode conv_noop(const char*, NumericSystem, char*, size_t, size_t*) { return STATUS_OK; }
    StatusCode ext_noop(const char*, size_t, char*, size_t) { return STATUS_OK; }
    StatusCode fmt_noop(const char*, char*, size_t) { return STATUS_OK; }
    StatusCode cmp_noop(const char*, const char*, const char*, std::uint8_t* r) { *r = 0; return STATUS_OK; }
}

TEST(PluginUnloadDrainTest, ReturnsImmediatelyWhenScannerCooperates)
{
    ScanMode mode{.scanModeName = "m", .comparator = cmp_noop,
                  .needsInput = 0, .needsPrevious = 0, .reserved = {0, 0}};
    DataType type{.typeName = "Coop", .valueSize = 4,
                  .converter = conv_noop, .extractor = ext_noop, .formatter = fmt_noop,
                  .scanModes = &mode, .scanModeCount = 1};

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
    ON_CALL(scannerMock, get_last_plugin_error()).WillByDefault(Return(STATUS_OK));
    ON_CALL(scannerMock, get_results_count()).WillByDefault(Return(0));

    Vertex::Scanner::ScannerRuntimeService service{scannerMock, dispatcher, log};

    ON_CALL(scannerMock, set_scan_abort_state(true))
        .WillByDefault([&service](bool) { service.on_scan_complete(); });

    const auto regId = service.send_command(
        Vertex::Scanner::service::CmdRegisterType{.sdkType = &type, .sourcePluginIndex = 4},
        std::chrono::seconds{1});
    const auto regResult = service.await_result(regId, std::chrono::seconds{1});
    ASSERT_EQ(regResult.code, STATUS_OK);
    const auto typeId = std::get<Vertex::Scanner::service::RegisterTypeResultPayload>(regResult.payload).id;

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = typeId;
    (void) service.send_command(Vertex::Scanner::service::CmdStartScan{.config = config},
                                 std::chrono::seconds{1});

    EXPECT_CALL(log, log_warn(_)).Times(0);

    const auto started = std::chrono::steady_clock::now();
    const auto removed = service.invalidate_plugin_types(4);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_EQ(removed, 1u);
    EXPECT_LT(elapsed, Vertex::Scanner::ScannerRuntimeService::PLUGIN_UNLOAD_DRAIN_TIMEOUT)
        << "drain overran timeout despite cooperating backend";
    EXPECT_FALSE(service.is_scanning());
}

TEST(PluginUnloadDrainTest, TimesOutAndLogsWhenScannerNeverCompletes)
{
    ScanMode mode{.scanModeName = "m", .comparator = cmp_noop,
                  .needsInput = 0, .needsPrevious = 0, .reserved = {0, 0}};
    DataType type{.typeName = "Stuck", .valueSize = 4,
                  .converter = conv_noop, .extractor = ext_noop, .formatter = fmt_noop,
                  .scanModes = &mode, .scanModeCount = 1};

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
    ON_CALL(scannerMock, get_last_plugin_error()).WillByDefault(Return(STATUS_OK));
    ON_CALL(scannerMock, get_results_count()).WillByDefault(Return(0));

    
    
    

    Vertex::Scanner::ScannerRuntimeService service{scannerMock, dispatcher, log};

    const auto regId = service.send_command(
        Vertex::Scanner::service::CmdRegisterType{.sdkType = &type, .sourcePluginIndex = 8},
        std::chrono::seconds{1});
    const auto regResult = service.await_result(regId, std::chrono::seconds{1});
    ASSERT_EQ(regResult.code, STATUS_OK);
    const auto typeId = std::get<Vertex::Scanner::service::RegisterTypeResultPayload>(regResult.payload).id;

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = typeId;
    (void) service.send_command(Vertex::Scanner::service::CmdStartScan{.config = config},
                                 std::chrono::seconds{1});

    
    EXPECT_CALL(scannerMock, set_scan_abort_state(true)).Times(AnyNumber());
    bool warnSeen{false};
    EXPECT_CALL(log, log_warn(::testing::HasSubstr("drain timed out")))
        .Times(AnyNumber())
        .WillRepeatedly([&](std::string_view) { warnSeen = true; return STATUS_OK; });

    const auto started = std::chrono::steady_clock::now();
    const auto removed = service.invalidate_plugin_types(8);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_EQ(removed, 1u);
    EXPECT_GE(elapsed, Vertex::Scanner::ScannerRuntimeService::PLUGIN_UNLOAD_DRAIN_TIMEOUT);
    EXPECT_LT(elapsed, Vertex::Scanner::ScannerRuntimeService::PLUGIN_UNLOAD_DRAIN_TIMEOUT + std::chrono::seconds{1});
    EXPECT_TRUE(warnSeen) << "expected drain timeout warn log";
    EXPECT_TRUE(service.is_scanning()) << "scan slot should remain occupied when backend never completes";
}
