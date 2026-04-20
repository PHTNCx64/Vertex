//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/scanner/imemoryreader.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace
{
    class StubMemoryReader final : public Vertex::Scanner::IMemoryReader
    {
      public:
        StatusCode read_memory(std::uint64_t address, std::uint64_t size, void* buffer) override
        {
            if (address + size > m_bytes.size())
            {
                return StatusCode::STATUS_ERROR_MEMORY_OUT_OF_BOUNDS;
            }
            std::memcpy(buffer, m_bytes.data() + address, size);
            return StatusCode::STATUS_OK;
        }

        std::vector<std::uint8_t> m_bytes{};
    };

    StatusCode plugin_extractor_identity(const char* memoryBytes, size_t memorySize, char* output, size_t outputSize)
    {
        if (outputSize < memorySize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }
        std::memcpy(output, memoryBytes, memorySize);
        return StatusCode::STATUS_OK;
    }

    StatusCode plugin_formatter_noop(const char*, char* output, size_t outputSize)
    {
        if (outputSize > 0)
        {
            output[0] = '\0';
        }
        return StatusCode::STATUS_OK;
    }

    StatusCode plugin_converter_noop(const char*, NumericSystem, char*, size_t, size_t*)
    {
        return StatusCode::STATUS_OK;
    }

    StatusCode plugin_comparator_magic(const char* currentValue, const char*, const char*, std::uint8_t* result)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(currentValue);
        *result = (bytes[0] == 0xAB && bytes[1] == 0xCD) ? 1 : 0;
        return StatusCode::STATUS_OK;
    }

    StatusCode plugin_comparator_failing(const char*, const char*, const char*, std::uint8_t*)
    {
        return StatusCode::STATUS_ERROR_GENERAL;
    }
}

class PluginDefinedScanTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_scanMode = {
            .scanModeName = "magic-match",
            .comparator = plugin_comparator_magic,
            .needsInput = 0,
            .needsPrevious = 0,
            .reserved = {0, 0},
        };
        m_dataType = {
            .typeName = "PluginU16Magic",
            .valueSize = 2,
            .converter = plugin_converter_noop,
            .extractor = plugin_extractor_identity,
            .formatter = plugin_formatter_noop,
            .scanModes = &m_scanMode,
            .scanModeCount = 1,
        };

        m_settings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        m_log = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        m_dispatcher = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>>();

        ON_CALL(*m_settings, get_int(::testing::HasSubstr("readerThreads"), _)).WillByDefault(Return(1));
        ON_CALL(*m_settings, get_int(::testing::HasSubstr("threadBufferSizeMB"), _)).WillByDefault(Return(32));
        ON_CALL(*m_settings, get_int(::testing::HasSubstr("workerChunkSizeMB"), _)).WillByDefault(Return(8));
        ON_CALL(*m_dispatcher, is_single_threaded()).WillByDefault(Return(true));
        ON_CALL(*m_dispatcher, create_worker_pool(_, _)).WillByDefault(Return(StatusCode::STATUS_OK));
        ON_CALL(*m_dispatcher, destroy_worker_pool(_)).WillByDefault(Return(StatusCode::STATUS_OK));
        ON_CALL(*m_dispatcher, enqueue_on_worker(_, _, _))
            .WillByDefault([](Vertex::Thread::ThreadChannel, std::size_t,
                               std::packaged_task<StatusCode()>&& task) -> StatusCode
                           {
                               task();
                               return StatusCode::STATUS_OK;
                           });

        m_scanner = std::make_unique<Vertex::Scanner::MemoryScanner>(*m_settings, *m_log, *m_dispatcher);
        m_reader = std::make_shared<StubMemoryReader>();
        m_scanner->set_memory_reader(m_reader);
    }

    [[nodiscard]] std::shared_ptr<const Vertex::Scanner::TypeSchema> make_plugin_schema(std::uint32_t customId) const
    {
        Vertex::Scanner::TypeSchema schema{};
        schema.id = static_cast<Vertex::Scanner::TypeId>(customId);
        schema.name = m_dataType.typeName;
        schema.kind = Vertex::Scanner::TypeKind::PluginDefined;
        schema.valueSize = static_cast<std::uint32_t>(m_dataType.valueSize);
        schema.sdkType = &m_dataType;
        return std::make_shared<const Vertex::Scanner::TypeSchema>(schema);
    }

    ScanMode m_scanMode{};
    DataType m_dataType{};

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> m_settings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_log;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<Vertex::Scanner::MemoryScanner> m_scanner;
    std::shared_ptr<StubMemoryReader> m_reader;
};

TEST_F(PluginDefinedScanTest, InitializeScanRejectsNullSdkType)
{
    Vertex::Scanner::TypeSchema schema{};
    schema.id = static_cast<Vertex::Scanner::TypeId>(1000);
    schema.kind = Vertex::Scanner::TypeKind::PluginDefined;
    schema.valueSize = 2;
    schema.sdkType = nullptr;

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = schema.id;
    config.scanMode = 0;
    config.dataSize = 2;

    const auto status = m_scanner->initialize_scan(
        config,
        std::make_shared<const Vertex::Scanner::TypeSchema>(schema),
        {{.baseAddress = 0, .size = 8}});
    EXPECT_EQ(status, StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(PluginDefinedScanTest, InitializeScanRejectsOutOfRangeScanMode)
{
    auto schema = make_plugin_schema(1000);

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = schema->id;
    config.scanMode = 5;
    config.dataSize = 2;

    const auto status = m_scanner->initialize_scan(
        config,
        schema,
        {{.baseAddress = 0, .size = 8}});
    EXPECT_EQ(status, StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(PluginDefinedScanTest, InitializeScanRejectsNullComparator)
{
    ScanMode brokenMode = m_scanMode;
    brokenMode.comparator = nullptr;
    DataType brokenType = m_dataType;
    brokenType.scanModes = &brokenMode;

    Vertex::Scanner::TypeSchema schema{};
    schema.id = static_cast<Vertex::Scanner::TypeId>(1001);
    schema.name = brokenType.typeName;
    schema.kind = Vertex::Scanner::TypeKind::PluginDefined;
    schema.valueSize = static_cast<std::uint32_t>(brokenType.valueSize);
    schema.sdkType = &brokenType;

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = schema.id;
    config.scanMode = 0;
    config.dataSize = 2;

    const auto status = m_scanner->initialize_scan(
        config,
        std::make_shared<const Vertex::Scanner::TypeSchema>(schema),
        {{.baseAddress = 0, .size = 8}});
    EXPECT_EQ(status, StatusCode::STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(PluginDefinedScanTest, InitializeScanAcceptsValidPluginSchema)
{
    auto schema = make_plugin_schema(1000);

    m_reader->m_bytes = {0xAB, 0xCD, 0x00, 0x00};

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = schema->id;
    config.valueType = Vertex::Scanner::ValueType::Int16;
    config.scanMode = 0;
    config.dataSize = 2;
    config.alignmentRequired = false;
    config.alignment = 1;

    const auto status = m_scanner->initialize_scan(
        config,
        schema,
        {{.baseAddress = 0, .size = 4}});
    EXPECT_EQ(status, StatusCode::STATUS_OK);
}






TEST_F(PluginDefinedScanTest, SchemaReleasedAfterStopScan)
{
    auto schema = make_plugin_schema(1000);
    std::weak_ptr<const Vertex::Scanner::TypeSchema> weakSchema{schema};

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = schema->id;
    config.valueType = Vertex::Scanner::ValueType::Int16;
    config.scanMode = 0;
    config.dataSize = 2;
    config.alignmentRequired = false;

    m_reader->m_bytes = {0xAB, 0xCD};

    (void) m_scanner->initialize_scan(
        config,
        schema,
        {{.baseAddress = 0, .size = 2}});

    schema.reset();
    m_scanner->stop_scan();
    m_scanner->finalize_scan();

    EXPECT_TRUE(weakSchema.expired())
        << "schema keepalive leaked past scan lifecycle";
}

TEST_F(PluginDefinedScanTest, PluginDataSizeOverridesBuiltinValueTypeSize)
{
    
    
    ScanMode wideMode = m_scanMode;
    DataType wideType = m_dataType;
    wideType.valueSize = 2;
    wideType.scanModes = &wideMode;

    Vertex::Scanner::TypeSchema schema{};
    schema.id = static_cast<Vertex::Scanner::TypeId>(1500);
    schema.name = wideType.typeName;
    schema.kind = Vertex::Scanner::TypeKind::PluginDefined;
    schema.valueSize = 2;
    schema.sdkType = &wideType;

    Vertex::Scanner::ScanConfiguration config{};
    config.typeId = schema.id;
    config.valueType = Vertex::Scanner::ValueType::Int32;  
    config.scanMode = 0;
    config.dataSize = 4;                                   
    config.alignmentRequired = false;
    config.alignment = 1;

    m_reader->m_bytes = {0xAB, 0xCD, 0x00, 0x00, 0xAB, 0xCD, 0x00, 0x00};

    const auto status = m_scanner->initialize_scan(
        config,
        std::make_shared<const Vertex::Scanner::TypeSchema>(schema),
        {{.baseAddress = 0, .size = 8}});
    EXPECT_EQ(status, StatusCode::STATUS_OK);
}
