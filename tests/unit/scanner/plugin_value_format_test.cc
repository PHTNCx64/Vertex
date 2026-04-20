//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/scanner/plugin_value_format.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include <array>
#include <cstring>

namespace
{
    StatusCode formatter_upper(const char* input, char* output, size_t outputSize)
    {
        if (!input || !output || outputSize == 0) return STATUS_ERROR_INVALID_PARAMETER;
        std::size_t i{};
        while (input[i] && i + 1 < outputSize)
        {
            char c = input[i];
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            output[i] = c;
            ++i;
        }
        output[i] = '\0';
        return STATUS_OK;
    }

    StatusCode extractor_as_text(const char* memoryBytes, size_t memorySize, char* output, size_t outputSize)
    {
        if (!memoryBytes || !output || outputSize == 0) return STATUS_ERROR_INVALID_PARAMETER;
        const std::size_t copyLen = std::min(memorySize, outputSize - 1);
        std::memcpy(output, memoryBytes, copyLen);
        output[copyLen] = '\0';
        return STATUS_OK;
    }

    StatusCode extractor_fail(const char*, size_t, char*, size_t) { return STATUS_ERROR_GENERAL; }

    StatusCode converter_digits_to_byte(const char* input, NumericSystem, char* output, size_t outputSize, size_t* bytesWritten)
    {
        if (!input || !output || !bytesWritten || outputSize == 0) return STATUS_ERROR_INVALID_PARAMETER;
        std::uint8_t accumulator{};
        for (std::size_t i{}; input[i]; ++i)
        {
            if (input[i] < '0' || input[i] > '9') return STATUS_ERROR_INVALID_PARAMETER;
            accumulator = static_cast<std::uint8_t>(accumulator * 10 + (input[i] - '0'));
        }
        output[0] = static_cast<char>(accumulator);
        *bytesWritten = 1;
        return STATUS_OK;
    }

    StatusCode converter_zero_bytes(const char*, NumericSystem, char*, size_t, size_t* bytesWritten)
    {
        if (bytesWritten) *bytesWritten = 0;
        return STATUS_OK;
    }

    Vertex::Scanner::TypeSchema make_plugin_schema(const DataType* sdkType, std::uint32_t size)
    {
        Vertex::Scanner::TypeSchema s{};
        s.id = static_cast<Vertex::Scanner::TypeId>(Vertex::Scanner::FIRST_CUSTOM_TYPE_ID);
        s.name = "Plugin";
        s.kind = Vertex::Scanner::TypeKind::PluginDefined;
        s.valueSize = size;
        s.sdkType = sdkType;
        return s;
    }
}

TEST(FormatPluginBytes, RunsExtractorThenFormatter)
{
    DataType sdk{};
    sdk.typeName = "x";
    sdk.valueSize = 4;
    sdk.extractor = extractor_as_text;
    sdk.formatter = formatter_upper;

    auto schema = make_plugin_schema(&sdk, 4);
    const char data[]{"ab"};
    const auto result = Vertex::Scanner::format_plugin_bytes(schema, data, sizeof(data) - 1);
    EXPECT_EQ(result, "AB");
}

TEST(FormatPluginBytes, NullExtractorReturnsEmpty)
{
    DataType sdk{};
    sdk.extractor = nullptr;
    sdk.formatter = formatter_upper;

    auto schema = make_plugin_schema(&sdk, 4);
    const char data[]{"zz"};
    const auto result = Vertex::Scanner::format_plugin_bytes(schema, data, sizeof(data) - 1);
    EXPECT_EQ(result, "");
}

TEST(FormatPluginBytes, ExtractorFailReturnsEmpty)
{
    DataType sdk{};
    sdk.extractor = extractor_fail;
    sdk.formatter = formatter_upper;

    auto schema = make_plugin_schema(&sdk, 4);
    const char data[]{"yy"};
    const auto result = Vertex::Scanner::format_plugin_bytes(schema, data, sizeof(data) - 1);
    EXPECT_EQ(result, "");
}

TEST(FormatPluginBytes, NullFormatterFallsBackToExtracted)
{
    DataType sdk{};
    sdk.extractor = extractor_as_text;
    sdk.formatter = nullptr;

    auto schema = make_plugin_schema(&sdk, 4);
    const char data[]{"ab"};
    const auto result = Vertex::Scanner::format_plugin_bytes(schema, data, sizeof(data) - 1);
    EXPECT_EQ(result, "ab");
}

TEST(FormatPluginBytes, NonPluginKindReturnsEmpty)
{
    Vertex::Scanner::TypeSchema builtin{};
    builtin.id = Vertex::Scanner::builtin_type_id(Vertex::Scanner::ValueType::Int32);
    builtin.kind = Vertex::Scanner::TypeKind::BuiltinNumeric;
    builtin.valueSize = 4;

    const char data[]{"aa"};
    EXPECT_EQ(Vertex::Scanner::format_plugin_bytes(builtin, data, 2), "");
}

TEST(ConvertPluginInput, HappyPath)
{
    DataType sdk{};
    sdk.converter = converter_digits_to_byte;

    auto schema = make_plugin_schema(&sdk, 1);
    std::vector<std::uint8_t> out;
    const auto status = Vertex::Scanner::convert_plugin_input(schema, VERTEX_DECIMAL, "42", out);
    EXPECT_EQ(status, STATUS_OK);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], 42u);
}

TEST(ConvertPluginInput, NullConverterErrors)
{
    DataType sdk{};
    sdk.converter = nullptr;

    auto schema = make_plugin_schema(&sdk, 1);
    std::vector<std::uint8_t> out;
    const auto status = Vertex::Scanner::convert_plugin_input(schema, VERTEX_DECIMAL, "1", out);
    EXPECT_NE(status, STATUS_OK);
}

TEST(ConvertPluginInput, ZeroBytesWrittenRejected)
{
    DataType sdk{};
    sdk.converter = converter_zero_bytes;

    auto schema = make_plugin_schema(&sdk, 1);
    std::vector<std::uint8_t> out;
    const auto status = Vertex::Scanner::convert_plugin_input(schema, VERTEX_DECIMAL, "1", out);
    EXPECT_EQ(status, STATUS_ERROR_INVALID_PARAMETER);
}

TEST(ConvertPluginInput, NonPluginKindRejected)
{
    Vertex::Scanner::TypeSchema builtin{};
    builtin.kind = Vertex::Scanner::TypeKind::BuiltinNumeric;
    std::vector<std::uint8_t> out;
    const auto status = Vertex::Scanner::convert_plugin_input(builtin, VERTEX_DECIMAL, "1", out);
    EXPECT_NE(status, STATUS_OK);
}
