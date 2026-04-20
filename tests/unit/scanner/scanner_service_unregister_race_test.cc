//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/scanner/scannerruntimeservice.hh>
#include <vertex/scanner/scanner_command.hh>

#include "../../mocks/MockIMemoryScanner.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <chrono>
#include <cstring>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    StatusCode converter_noop(const char*, NumericSystem, char*, size_t, size_t*)
    {
        return StatusCode::STATUS_OK;
    }
    StatusCode extractor_noop(const char*, size_t, char*, size_t) { return StatusCode::STATUS_OK; }
    StatusCode formatter_noop(const char*, char*, size_t) { return StatusCode::STATUS_OK; }
    StatusCode comparator_noop(const char*, const char*, const char*, std::uint8_t* r)
    {
        *r = 0;
        return StatusCode::STATUS_OK;
    }
}

class ScannerServiceUnregisterRaceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_scanMode = ScanMode{
            .scanModeName = "eq",
            .comparator = comparator_noop,
            .needsInput = 0,
            .needsPrevious = 0,
            .reserved = {0, 0},
        };
        m_dataType = DataType{
            .typeName = "RaceType",
            .valueSize = 4,
            .converter = converter_noop,
            .extractor = extractor_noop,
            .formatter = formatter_noop,
            .scanModes = &m_scanMode,
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

        m_service = std::make_unique<Vertex::Scanner::ScannerRuntimeService>(
            *m_scannerMock, *m_dispatcher, *m_log);
    }

    ScanMode m_scanMode{};
    DataType m_dataType{};

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>> m_scannerMock;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_log;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<Vertex::Scanner::ScannerRuntimeService> m_service;
};

TEST_F(ScannerServiceUnregisterRaceTest, UnregisterAfterInvalidationReturnsNotFound)
{
    constexpr std::size_t kPluginIndex = 7;
    const auto registerId = m_service->send_command(
        Vertex::Scanner::service::CmdRegisterType{
            .sdkType = &m_dataType,
            .sourcePluginIndex = kPluginIndex,
            .libraryKeepalive = {},
        },
        std::chrono::seconds{1});
    ASSERT_NE(registerId, Vertex::Runtime::INVALID_COMMAND_ID);
    const auto registerResult = m_service->await_result(registerId, std::chrono::milliseconds{500});
    ASSERT_EQ(registerResult.code, StatusCode::STATUS_OK);
    const auto* registerPayload = std::get_if<Vertex::Scanner::service::RegisterTypeResultPayload>(&registerResult.payload);
    ASSERT_NE(registerPayload, nullptr);
    const auto capturedId = registerPayload->id;

    
    const auto removed = m_service->invalidate_plugin_types(kPluginIndex);
    EXPECT_EQ(removed, 1u);

    const auto unregisterId = m_service->send_command(
        Vertex::Scanner::service::CmdUnregisterType{.id = capturedId},
        std::chrono::seconds{1});
    ASSERT_NE(unregisterId, Vertex::Runtime::INVALID_COMMAND_ID);
    const auto unregisterResult = m_service->await_result(unregisterId, std::chrono::milliseconds{500});
    EXPECT_EQ(unregisterResult.code, StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND);
}

TEST_F(ScannerServiceUnregisterRaceTest, UnregisterUnknownIdReturnsNotFound)
{
    const auto unregisterId = m_service->send_command(
        Vertex::Scanner::service::CmdUnregisterType{
            .id = static_cast<Vertex::Scanner::TypeId>(9999),
        },
        std::chrono::seconds{1});
    ASSERT_NE(unregisterId, Vertex::Runtime::INVALID_COMMAND_ID);
    const auto result = m_service->await_result(unregisterId, std::chrono::milliseconds{500});
    EXPECT_EQ(result.code, StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND);
}
