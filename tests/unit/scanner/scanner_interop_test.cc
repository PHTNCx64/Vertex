//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/scanner/scanner_interop.hh>
#include <vertex/scanner/scannerruntimeservice.hh>

#include "../../mocks/MockIMemoryScanner.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <chrono>
#include <memory>

using ::testing::_;
using ::testing::NiceMock;

namespace
{
    StatusCode converter_noop(const char*, NumericSystem, char*, size_t, size_t*) { return StatusCode::STATUS_OK; }
    StatusCode extractor_noop(const char*, size_t, char*, size_t) { return StatusCode::STATUS_OK; }
    StatusCode formatter_noop(const char*, char*, size_t) { return StatusCode::STATUS_OK; }
    StatusCode comparator_noop(const char*, const char*, const char*, std::uint8_t* r) { *r = 0; return StatusCode::STATUS_OK; }
}

class ScannerInteropTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_mode = ScanMode{
            .scanModeName = "eq",
            .comparator = comparator_noop,
            .needsInput = 0,
            .needsPrevious = 0,
            .reserved = {0, 0},
        };
        m_type = DataType{
            .typeName = "InteropType",
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

        m_service = std::make_unique<Vertex::Scanner::ScannerRuntimeService>(
            *m_scannerMock, *m_dispatcher, *m_log);

        Vertex::Scanner::interop::set_scanner_service(m_service.get());
    }

    void TearDown() override
    {
        Vertex::Scanner::interop::set_scanner_service(nullptr);
    }

    ScanMode m_mode{};
    DataType m_type{};
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>> m_scannerMock;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_log;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<Vertex::Scanner::ScannerRuntimeService> m_service;
};

TEST_F(ScannerInteropTest, RegisterRejectedWithoutContext)
{
    EXPECT_EQ(vertex_register_datatype(&m_type), StatusCode::STATUS_ERROR_INVALID_STATE);
}

TEST_F(ScannerInteropTest, RegisterAcceptedInInitContext)
{
    Vertex::Scanner::interop::PluginRegistrationGuard guard{3, {}};
    EXPECT_EQ(vertex_register_datatype(&m_type), StatusCode::STATUS_OK);
}

TEST_F(ScannerInteropTest, RegisterRejectedInShutdownContext)
{
    Vertex::Scanner::interop::PluginRegistrationGuard guard{
        3, {}, Vertex::Scanner::interop::RegistrationMode::Shutdown};
    EXPECT_EQ(vertex_register_datatype(&m_type), StatusCode::STATUS_ERROR_INVALID_STATE);
}

TEST_F(ScannerInteropTest, UnregisterRejectedInShutdownContext)
{
    {
        Vertex::Scanner::interop::PluginRegistrationGuard init{3, {}};
        ASSERT_EQ(vertex_register_datatype(&m_type), StatusCode::STATUS_OK);
    }

    Vertex::Scanner::interop::PluginRegistrationGuard shutdown{
        3, {}, Vertex::Scanner::interop::RegistrationMode::Shutdown};
    EXPECT_EQ(vertex_unregister_datatype(&m_type), StatusCode::STATUS_ERROR_INVALID_STATE);
}
