//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//




#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sdk/memory.h>
#include <vertex/model/mainmodel.hh>
#include <vertex/scanner/scanner_typeschema.hh>
#include <vertex/thread/threadchannel.hh>

#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockIMemoryScanner.hh"
#include "../../mocks/MockIScannerRuntimeService.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <algorithm>
#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace
{
    using ::testing::_;
    using ::testing::NiceMock;
    using ::testing::Return;

    struct ConverterCapture final
    {
        std::vector<NumericSystem> bases{};
        NumericSystem succeedOn{VERTEX_NONE};
    };

    inline ConverterCapture* g_capture{nullptr};

    StatusCode capturing_converter(const char*, NumericSystem base, char* output, size_t outputSize, size_t* bytesWritten)
    {
        if (g_capture)
        {
            g_capture->bases.push_back(base);
        }
        if (g_capture && g_capture->succeedOn != VERTEX_NONE && base == g_capture->succeedOn)
        {
            if (!output || outputSize == 0 || !bytesWritten)
            {
                return STATUS_ERROR_INVALID_PARAMETER;
            }
            output[0] = 0;
            *bytesWritten = 1;
            return STATUS_OK;
        }
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    Vertex::Scanner::TypeSchema make_plugin_schema(const DataType& sdkType)
    {
        Vertex::Scanner::TypeSchema s{};
        s.id = static_cast<Vertex::Scanner::TypeId>(Vertex::Scanner::FIRST_CUSTOM_TYPE_ID);
        s.name = "Plugin";
        s.kind = Vertex::Scanner::TypeKind::PluginDefined;
        s.valueSize = 1;
        s.sdkType = &sdkType;
        return s;
    }
}

class MainModelPluginValidateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockSettings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        mockScanner = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>>();
        mockScannerService = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIScannerRuntimeService>>();
        mockLoader = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILoader>>();
        mockLogger = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        mockDispatcher = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>>();

        ON_CALL(*mockDispatcher, dispatch(_, _))
            .WillByDefault([](Vertex::Thread::ThreadChannel, std::packaged_task<StatusCode()>&& task)
                               -> std::expected<std::future<StatusCode>, StatusCode>
                           {
                               auto future = task.get_future();
                               task();
                               return future;
                           });

        model = std::make_unique<Vertex::Model::MainModel>(
            *mockSettings, *mockScanner, *mockScannerService, *mockLoader, *mockLogger, *mockDispatcher);

        sdkType = DataType{};
        sdkType.typeName = "Plugin";
        sdkType.valueSize = 1;
        sdkType.converter = capturing_converter;

        hints = DataTypeUIHints{};
        sdkType.uiHints = &hints;

        capture = ConverterCapture{};
        g_capture = &capture;
    }

    void TearDown() override
    {
        g_capture = nullptr;
    }

    void expect_find(const Vertex::Scanner::TypeSchema& schema)
    {
        ON_CALL(*mockScannerService, find_type(_))
            .WillByDefault(Return(std::optional<Vertex::Scanner::TypeSchema>{schema}));
    }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>> mockScanner;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIScannerRuntimeService>> mockScannerService;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILoader>> mockLoader;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> mockLogger;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> mockDispatcher;
    std::unique_ptr<Vertex::Model::MainModel> model;

    DataType sdkType{};
    DataTypeUIHints hints{};
    ConverterCapture capture{};
};

TEST_F(MainModelPluginValidateTest, NumericSystem_PrimarySucceeds_DoesNotTryOthers)
{
    capture.succeedOn = VERTEX_BINARY;
    hints.numericSystemsMask = VERTEX_NUMERIC_SYSTEM_MASK_BINARY | VERTEX_NUMERIC_SYSTEM_MASK_DECIMAL | VERTEX_NUMERIC_SYSTEM_MASK_HEXADECIMAL;
    const auto schema = make_plugin_schema(sdkType);
    expect_find(schema);

    std::vector<std::uint8_t> out;
    const StatusCode s = model->validate_input(schema.id, VERTEX_BINARY, "1", out);

    EXPECT_EQ(StatusCode::STATUS_OK, s);
    ASSERT_EQ(1u, capture.bases.size());
    EXPECT_EQ(VERTEX_BINARY, capture.bases.front());
}

TEST_F(MainModelPluginValidateTest, NumericSystem_MaskDrivenFallback_BinaryToDecimal)
{
    capture.succeedOn = VERTEX_DECIMAL;
    hints.numericSystemsMask = VERTEX_NUMERIC_SYSTEM_MASK_BINARY | VERTEX_NUMERIC_SYSTEM_MASK_DECIMAL;
    const auto schema = make_plugin_schema(sdkType);
    expect_find(schema);

    std::vector<std::uint8_t> out;
    const StatusCode s = model->validate_input(schema.id, VERTEX_BINARY, "5", out);

    EXPECT_EQ(StatusCode::STATUS_OK, s);
    EXPECT_EQ(VERTEX_BINARY, capture.bases.front());
    EXPECT_NE(std::ranges::find(capture.bases, VERTEX_DECIMAL), capture.bases.end());
    EXPECT_EQ(std::ranges::find(capture.bases, VERTEX_HEXADECIMAL), capture.bases.end());
    EXPECT_EQ(std::ranges::find(capture.bases, VERTEX_OCTAL), capture.bases.end());
}

TEST_F(MainModelPluginValidateTest, NumericSystem_MaskExcludesHex_DoesNotFallBackToHex)
{
    hints.numericSystemsMask = VERTEX_NUMERIC_SYSTEM_MASK_BINARY;
    const auto schema = make_plugin_schema(sdkType);
    expect_find(schema);

    std::vector<std::uint8_t> out;
    const StatusCode s = model->validate_input(schema.id, VERTEX_BINARY, "x", out);

    EXPECT_NE(StatusCode::STATUS_OK, s);
    for (const auto base : capture.bases)
    {
        EXPECT_EQ(VERTEX_BINARY, base);
    }
}

TEST_F(MainModelPluginValidateTest, NumericSystem_NoHintsAndPrimaryNotHex_LegacyFallsBackToHex)
{
    capture.succeedOn = VERTEX_HEXADECIMAL;
    sdkType.uiHints = nullptr;
    const auto schema = make_plugin_schema(sdkType);
    expect_find(schema);

    std::vector<std::uint8_t> out;
    const StatusCode s = model->validate_input(schema.id, VERTEX_DECIMAL, "ff", out);

    EXPECT_EQ(StatusCode::STATUS_OK, s);
    ASSERT_GE(capture.bases.size(), 2u);
    EXPECT_EQ(VERTEX_DECIMAL, capture.bases[0]);
    EXPECT_EQ(VERTEX_HEXADECIMAL, capture.bases[1]);
}

TEST_F(MainModelPluginValidateTest, Bool_DefaultNumericSystemUsedAsPrimaryWhenHexFalse)
{
    capture.succeedOn = VERTEX_OCTAL;
    hints.defaultNumericSystem = VERTEX_OCTAL;
    hints.numericSystemsMask = VERTEX_NUMERIC_SYSTEM_MASK_OCTAL;
    const auto schema = make_plugin_schema(sdkType);
    expect_find(schema);

    std::vector<std::uint8_t> out;
    const StatusCode s = model->validate_input(schema.id, false, "7", out);

    EXPECT_EQ(StatusCode::STATUS_OK, s);
    ASSERT_FALSE(capture.bases.empty());
    EXPECT_EQ(VERTEX_OCTAL, capture.bases.front());
}

TEST_F(MainModelPluginValidateTest, Bool_HexFlagOverridesDefaultNumericSystem)
{
    capture.succeedOn = VERTEX_HEXADECIMAL;
    hints.defaultNumericSystem = VERTEX_OCTAL;
    hints.numericSystemsMask = VERTEX_NUMERIC_SYSTEM_MASK_OCTAL | VERTEX_NUMERIC_SYSTEM_MASK_HEXADECIMAL;
    const auto schema = make_plugin_schema(sdkType);
    expect_find(schema);

    std::vector<std::uint8_t> out;
    const StatusCode s = model->validate_input(schema.id, true, "ff", out);

    EXPECT_EQ(StatusCode::STATUS_OK, s);
    ASSERT_FALSE(capture.bases.empty());
    EXPECT_EQ(VERTEX_HEXADECIMAL, capture.bases.front());
}

TEST_F(MainModelPluginValidateTest, UIHints_MaskDefaultSynchronized_DefaultAppearsInMask)
{
    hints.defaultNumericSystem = VERTEX_OCTAL;
    hints.numericSystemsMask = VERTEX_NUMERIC_SYSTEM_MASK_OCTAL;
    const auto schema = make_plugin_schema(sdkType);

    ASSERT_NE(schema.sdkType, nullptr);
    ASSERT_NE(schema.sdkType->uiHints, nullptr);
    const auto& h = *schema.sdkType->uiHints;

    EXPECT_EQ(VERTEX_OCTAL, h.defaultNumericSystem);
    EXPECT_NE(0u, h.numericSystemsMask & VERTEX_NUMERIC_SYSTEM_MASK_OCTAL);
}

TEST_F(MainModelPluginValidateTest, UIHints_MultilinePlaceholderMaxLength_ExposedViaSchema)
{
    hints.inputIsMultiline = 1;
    hints.maxInputLength = 256;
    hints.inputPlaceholder = "enter value";
    hints.supportsEndianness = 0;
    const auto schema = make_plugin_schema(sdkType);

    ASSERT_NE(schema.sdkType, nullptr);
    ASSERT_NE(schema.sdkType->uiHints, nullptr);
    const auto& h = *schema.sdkType->uiHints;

    EXPECT_EQ(1u, h.inputIsMultiline);
    EXPECT_EQ(256u, h.maxInputLength);
    ASSERT_NE(nullptr, h.inputPlaceholder);
    EXPECT_STREQ("enter value", h.inputPlaceholder);
    EXPECT_EQ(0u, h.supportsEndianness);
}

TEST_F(MainModelPluginValidateTest, UIHints_EndiannessToggle_ReflectedInSchema)
{
    hints.supportsEndianness = 1;
    const auto schemaEnabled = make_plugin_schema(sdkType);
    ASSERT_NE(schemaEnabled.sdkType->uiHints, nullptr);
    EXPECT_EQ(1u, schemaEnabled.sdkType->uiHints->supportsEndianness);

    hints.supportsEndianness = 0;
    EXPECT_EQ(0u, schemaEnabled.sdkType->uiHints->supportsEndianness);
}
