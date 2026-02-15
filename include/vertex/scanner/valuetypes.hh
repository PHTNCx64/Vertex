//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <bit>
#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <cstring>

namespace Vertex::Scanner
{
    enum class ValueType : std::uint8_t
    {
        Int8 = 0,
        Int16,
        Int32,
        Int64,
        UInt8,
        UInt16,
        UInt32,
        UInt64,
        Float,
        Double,
        StringASCII,
        StringUTF8,
        StringUTF16,
        StringUTF32,
        COUNT
    };

    enum class NumericScanMode : std::uint8_t
    {
        Exact = 0,
        GreaterThan,
        LessThan,
        Between,
        Unknown,
        Changed,
        Unchanged,
        Increased,
        Decreased,
        IncreasedBy,
        DecreasedBy,
        COUNT
    };

    enum class StringScanMode : std::uint8_t
    {
        Exact = 0,
        Contains,
        BeginsWith,
        EndsWith,
        COUNT
    };

    enum class Endianness : std::uint8_t
    {
        Little = 0,
        Big = 1,
        HostCPU = 2
    };

    [[nodiscard]] inline constexpr Endianness get_host_endianness()
    {
        return std::endian::native == std::endian::big ? Endianness::Big : Endianness::Little;
    }

    [[nodiscard]] inline constexpr bool needs_endian_swap(Endianness endianness)
    {
        const auto resolved = (endianness == Endianness::HostCPU) ? get_host_endianness() : endianness;
        return resolved != get_host_endianness();
    }

    struct ValueTypeInfo
    {
        const char* name;
        std::size_t size;
        bool isSigned;
        bool isFloatingPoint;
        bool isString;
    };

    constexpr std::array<ValueTypeInfo, static_cast<std::size_t>(ValueType::COUNT)> VALUE_TYPE_INFO = {{
        {"Int8",   sizeof(std::int8_t),   true,  false, false},
        {"Int16",  sizeof(std::int16_t),  true,  false, false},
        {"Int32",  sizeof(std::int32_t),  true,  false, false},
        {"Int64",  sizeof(std::int64_t),  true,  false, false},
        {"UInt8",  sizeof(std::uint8_t),  false, false, false},
        {"UInt16", sizeof(std::uint16_t), false, false, false},
        {"UInt32", sizeof(std::uint32_t), false, false, false},
        {"UInt64", sizeof(std::uint64_t), false, false, false},
        {"Float",  sizeof(float),         true,  true,  false},
        {"Double", sizeof(double),        true,  true,  false},
        {"ASCII String",  0, false, false, true},
        {"UTF-8 String",  0, false, false, true},
        {"UTF-16 String", 0, false, false, true},
        {"UTF-32 String", 0, false, false, true},
    }};

    constexpr std::array<const char*, static_cast<std::size_t>(NumericScanMode::COUNT)> NUMERIC_SCAN_MODE_NAMES = {{
        "Exact Value",
        "Greater Than",
        "Less Than",
        "Between",
        "Unknown Initial Value",
        "Changed",
        "Unchanged",
        "Increased",
        "Decreased",
        "Increased by",
        "Decreased by",
    }};

    constexpr std::array<const char*, static_cast<std::size_t>(StringScanMode::COUNT)> STRING_SCAN_MODE_NAMES = {{
        "Exact",
        "Contains",
        "Begins With",
        "Ends With",
    }};

    [[nodiscard]] inline constexpr const ValueTypeInfo& get_value_type_info(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)];
    }

    [[nodiscard]] inline constexpr std::size_t get_value_size(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)].size;
    }

    [[nodiscard]] inline constexpr std::size_t get_value_type_size(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)].size;
    }

    [[nodiscard]] inline std::string get_value_type_name(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)].name;
    }

    [[nodiscard]] inline std::string get_numeric_scan_mode_name(NumericScanMode mode)
    {
        return NUMERIC_SCAN_MODE_NAMES[static_cast<std::size_t>(mode)];
    }

    [[nodiscard]] inline std::string get_string_scan_mode_name(StringScanMode mode)
    {
        return STRING_SCAN_MODE_NAMES[static_cast<std::size_t>(mode)];
    }

    [[nodiscard]] inline constexpr bool is_string_type(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)].isString;
    }

    [[nodiscard]] inline constexpr bool is_numeric_type(ValueType type)
    {
        return !is_string_type(type);
    }

    [[nodiscard]] inline constexpr std::size_t get_string_char_size(ValueType type)
    {
        switch (type)
        {
            case ValueType::StringASCII:
            case ValueType::StringUTF8:
                return 1;
            case ValueType::StringUTF16:
                return 2;
            case ValueType::StringUTF32:
                return 4;
            default:
                return 0;
        }
    }

    [[nodiscard]] inline constexpr bool string_type_has_endianness(ValueType type)
    {
        return type == ValueType::StringUTF16 || type == ValueType::StringUTF32;
    }

    [[nodiscard]] inline constexpr bool is_floating_point(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)].isFloatingPoint;
    }

    [[nodiscard]] inline constexpr bool is_signed(ValueType type)
    {
        return VALUE_TYPE_INFO[static_cast<std::size_t>(type)].isSigned;
    }

    [[nodiscard]] inline constexpr bool scan_mode_needs_input(NumericScanMode mode)
    {
        switch (mode)
        {
            case NumericScanMode::Unknown:
            case NumericScanMode::Changed:
            case NumericScanMode::Unchanged:
            case NumericScanMode::Increased:
            case NumericScanMode::Decreased:
                return false;
            default:
                return true;
        }
    }

    [[nodiscard]] inline constexpr bool scan_mode_needs_previous(NumericScanMode mode)
    {
        switch (mode)
        {
            case NumericScanMode::Changed:
            case NumericScanMode::Unchanged:
            case NumericScanMode::Increased:
            case NumericScanMode::Decreased:
            case NumericScanMode::IncreasedBy:
            case NumericScanMode::DecreasedBy:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] inline constexpr bool scan_mode_needs_second_input(NumericScanMode mode)
    {
        return mode == NumericScanMode::Between;
    }

} // namespace Vertex::Scanner
