//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/memory.h>
#include <sdk/statuscode.h>
#include <vertex/runtime/caller.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

namespace Vertex::Scanner
{
    inline constexpr std::size_t PLUGIN_VALUE_BUFFER_SIZE{256};

    [[nodiscard]] inline std::string format_plugin_bytes(const TypeSchema& schema, const void* data, std::size_t size)
    {
        if (schema.kind != TypeKind::PluginDefined || !schema.sdkType || !data || size == 0)
        {
            return {};
        }

        std::array<char, PLUGIN_VALUE_BUFFER_SIZE> extracted{};
        const auto extractResult = Runtime::safe_call(schema.sdkType->extractor,
                                                       static_cast<const char*>(data),
                                                       size,
                                                       extracted.data(),
                                                       extracted.size());
        if (!Runtime::status_ok(extractResult))
        {
            return {};
        }

        std::array<char, PLUGIN_VALUE_BUFFER_SIZE> formatted{};
        const auto formatResult = Runtime::safe_call(schema.sdkType->formatter,
                                                      extracted.data(),
                                                      formatted.data(),
                                                      formatted.size());
        if (!Runtime::status_ok(formatResult))
        {
            return std::string{extracted.data()};
        }

        return std::string{formatted.data()};
    }

    [[nodiscard]] inline StatusCode convert_plugin_input(const TypeSchema& schema,
                                                          ::NumericSystem numericBase,
                                                          const std::string& input,
                                                          std::vector<std::uint8_t>& output)
    {
        if (schema.kind != TypeKind::PluginDefined || !schema.sdkType)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::array<char, PLUGIN_VALUE_BUFFER_SIZE> buffer{};
        std::size_t bytesWritten{};

        const auto result = Runtime::safe_call(schema.sdkType->converter,
                                                input.c_str(),
                                                numericBase,
                                                buffer.data(),
                                                buffer.size(),
                                                &bytesWritten);
        if (!Runtime::status_ok(result))
        {
            return Runtime::get_status(result);
        }

        if (bytesWritten == 0 || bytesWritten > buffer.size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        output.assign(buffer.data(), buffer.data() + bytesWritten);
        return StatusCode::STATUS_OK;
    }
}
