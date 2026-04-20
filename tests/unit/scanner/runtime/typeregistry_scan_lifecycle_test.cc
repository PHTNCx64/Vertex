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
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    StatusCode converter_noop(const char*, NumericSystem, char*, size_t, size_t*) { return STATUS_OK; }
    StatusCode extractor_noop(const char*, size_t, char*, size_t) { return STATUS_OK; }
    StatusCode formatter_noop(const char*, char*, size_t) { return STATUS_OK; }
    StatusCode comparator_noop(const char*, const char*, const char*, std::uint8_t* r) { *r = 0; return STATUS_OK; }
}

class TypeRegistryScanLifecycleTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_mode = ScanMode{
            .scanModeName = "eq",
            .comparator = comparator_noop,
            .needsInput = 0, .needsPrevious = 0, .reserved = {0, 0},
        };
        m_type = DataType{
            .typeName = "LifecycleType",
            .valueSize = 4,
            .converter = converter_noop,
            .extractor = extractor_noop,
            .formatter = formatter_noop,
            .scanModes = &m_mode,
            .scanModeCount = 1,
        };

        m_scannerMock = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>>();
        m_log = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        m_dispatcher = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>>();

        ON_CALL(*m_dispatcher, schedule_recurring(_, _, _, _, _, _))
            .WillByDefault([](auto, auto, auto, auto, auto, auto)
                           { return std::unexpected(StatusCode::STATUS_ERROR_NOT_IMPLEMENTED); });
        ON_CALL(*m_dispatcher, schedule_recurring_persistent(_, _, _, _, _, _))
            .WillByDefault([](auto, auto, auto, auto, auto, auto)
                           { return std::unexpected(StatusCode::STATUS_ERROR_NOT_IMPLEMENTED); });
        ON_CALL(*m_scannerMock, initialize_scan(_, _, _))
            .WillByDefault(Return(StatusCode::STATUS_OK));
        ON_CALL(*m_scannerMock, get_results_count()).WillByDefault(Return(42));
        ON_CALL(*m_scannerMock, get_last_plugin_error()).WillByDefault(Return(StatusCode::STATUS_OK));
        m_service = std::make_unique<Vertex::Scanner::ScannerRuntimeService>(
            *m_scannerMock, *m_dispatcher, *m_log);
    }

    void fire_completion()
    {
        if (m_service) m_service->on_scan_complete();
    }

    ScanMode m_mode{};
    DataType m_type{};
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>> m_scannerMock;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_log;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<Vertex::Scanner::ScannerRuntimeService> m_service;
};

TEST_F(TypeRegistryScanLifecycleTest, InvalidScanModeRejected)
{
    const auto regId = m_service->send_command(
        Vertex::Scanner::service::CmdRegisterType{.sdkType = &m_type, .sourcePluginIndex = 7},
        std::chrono::seconds{1});
    const auto regResult = m_service->await_result(regId, std::chrono::seconds{1});
    ASSERT_EQ(regResult.code, STATUS_OK);
    const auto* payload = std::get_if<Vertex::Scanner::service::RegisterTypeResultPayload>(&regResult.payload);
    ASSERT_NE(payload, nullptr);

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = payload->id;
    config.scanMode = 99;
    const auto scanId = m_service->send_command(
        Vertex::Scanner::service::CmdStartScan{.config = config}, std::chrono::seconds{1});
    const auto scanResult = m_service->await_result(scanId, std::chrono::seconds{1});
    EXPECT_EQ(scanResult.code, STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(TypeRegistryScanLifecycleTest, RegisterScanCompleteUnregisterRoundTrip)
{
    std::mutex eventMutex{};
    std::vector<Vertex::Scanner::ScannerEventKind> seen{};

    const auto subId = m_service->subscribe(
        static_cast<Vertex::Scanner::ScannerEventKindMask>(Vertex::Scanner::ScannerEventKind::TypeRegistered)
            | Vertex::Scanner::ScannerEventKind::ScanComplete
            | Vertex::Scanner::ScannerEventKind::TypeUnregistered,
        [&](const Vertex::Scanner::ScannerEvent& evt)
        {
            std::scoped_lock lock{eventMutex};
            seen.push_back(evt.kind);
        });

    const auto regId = m_service->send_command(
        Vertex::Scanner::service::CmdRegisterType{.sdkType = &m_type, .sourcePluginIndex = 1},
        std::chrono::seconds{1});
    const auto regResult = m_service->await_result(regId, std::chrono::seconds{1});
    ASSERT_EQ(regResult.code, STATUS_OK);
    const auto* payload = std::get_if<Vertex::Scanner::service::RegisterTypeResultPayload>(&regResult.payload);
    ASSERT_NE(payload, nullptr);
    const auto typeId = payload->id;
    ASSERT_NE(typeId, Vertex::Scanner::TypeId::Invalid);

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = typeId;
    config.scanMode = 0;
    const auto scanId = m_service->send_command(
        Vertex::Scanner::service::CmdStartScan{.config = config}, std::chrono::seconds{1});
    ASSERT_NE(scanId, Vertex::Runtime::INVALID_COMMAND_ID);

    
    
    fire_completion();

    const auto unregId = m_service->send_command(
        Vertex::Scanner::service::CmdUnregisterType{.id = typeId}, std::chrono::seconds{1});
    const auto unregResult = m_service->await_result(unregId, std::chrono::seconds{1});
    EXPECT_EQ(unregResult.code, STATUS_OK);

    Vertex::Scanner::ScanConfiguration staleConfig{};
    staleConfig.typeId = typeId;
    const auto postUnregisterScanId = m_service->send_command(
        Vertex::Scanner::service::CmdStartScan{.config = staleConfig}, std::chrono::seconds{1});
    const auto postUnregisterScan = m_service->await_result(postUnregisterScanId, std::chrono::seconds{1});
    EXPECT_EQ(postUnregisterScan.code, StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND);

    m_service->unsubscribe(subId);

    std::scoped_lock lock{eventMutex};
    EXPECT_NE(std::ranges::find(seen, Vertex::Scanner::ScannerEventKind::TypeRegistered), seen.end());
    EXPECT_NE(std::ranges::find(seen, Vertex::Scanner::ScannerEventKind::ScanComplete), seen.end());
    EXPECT_NE(std::ranges::find(seen, Vertex::Scanner::ScannerEventKind::TypeUnregistered), seen.end());
}

TEST_F(TypeRegistryScanLifecycleTest, MidScanInvalidationCancelsActiveScanForThatType)
{
    std::atomic<bool> cancelledSeen{false};
    Vertex::Runtime::CommandId cancelledCommandId{Vertex::Runtime::INVALID_COMMAND_ID};
    const auto subId = m_service->subscribe(
        static_cast<Vertex::Scanner::ScannerEventKindMask>(Vertex::Scanner::ScannerEventKind::Cancelled),
        [&](const Vertex::Scanner::ScannerEvent& evt)
        {
            if (evt.kind == Vertex::Scanner::ScannerEventKind::Cancelled)
            {
                const auto* info = std::get_if<Vertex::Scanner::CancelledInfo>(&evt.detail);
                if (info) cancelledCommandId = info->cancelledId;
                cancelledSeen = true;
            }
        });

    const auto regId = m_service->send_command(
        Vertex::Scanner::service::CmdRegisterType{.sdkType = &m_type, .sourcePluginIndex = 5},
        std::chrono::seconds{1});
    const auto regResult = m_service->await_result(regId, std::chrono::seconds{1});
    ASSERT_EQ(regResult.code, STATUS_OK);
    const auto typeId = std::get<Vertex::Scanner::service::RegisterTypeResultPayload>(regResult.payload).id;

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = typeId;
    const auto scanId = m_service->send_command(
        Vertex::Scanner::service::CmdStartScan{.config = config}, std::chrono::seconds{1});
    ASSERT_NE(scanId, Vertex::Runtime::INVALID_COMMAND_ID);

    EXPECT_CALL(*m_scannerMock, set_scan_abort_state(true)).Times(::testing::AtLeast(1));
    const auto removed = m_service->invalidate_plugin_types(5);
    EXPECT_EQ(removed, 1u);

    EXPECT_TRUE(cancelledSeen.load());
    EXPECT_EQ(cancelledCommandId, scanId);

    m_service->unsubscribe(subId);
}
