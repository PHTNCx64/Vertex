//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/valuetypes.hh>
#include <cstdint>
#include <optional>
#include <vector>

namespace Vertex::Scanner
{
    struct ScanConfiguration final
    {
        ValueType valueType{ValueType::Int32};

        std::uint8_t scanMode{};

        std::vector<std::uint8_t> input{};
        std::vector<std::uint8_t> input2{};

        std::size_t dataSize{};
        std::size_t firstValueSize{};

        bool alignmentRequired{true};
        std::size_t alignment{4};

        std::optional<std::uint64_t> maxResults{};

        bool hexDisplay{};

        Endianness endianness{Endianness::Little};

        [[nodiscard]] NumericScanMode get_numeric_scan_mode() const
        {
            return static_cast<NumericScanMode>(scanMode);
        }

        [[nodiscard]] StringScanMode get_string_scan_mode() const
        {
            return static_cast<StringScanMode>(scanMode);
        }

        [[nodiscard]] bool needs_input() const
        {
            if (is_string_type(valueType))
            {
                return true;
            }
            return scan_mode_needs_input(get_numeric_scan_mode());
        }

        [[nodiscard]] bool needs_second_input() const
        {
            if (is_string_type(valueType))
            {
                return false;
            }
            return scan_mode_needs_second_input(get_numeric_scan_mode());
        }

        [[nodiscard]] bool needs_previous_value() const
        {
            if (is_string_type(valueType))
            {
                return false;
            }
            return scan_mode_needs_previous(get_numeric_scan_mode());
        }
    };
}
