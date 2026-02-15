//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/valuetypes.hh>
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>
#include <optional>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ranges>

namespace Vertex::Scanner
{
    [[nodiscard]] inline std::string trim_whitespace(std::string_view str)
    {
        const auto start = str.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string_view::npos)
        {
            return "";
        }
        const auto end = str.find_last_not_of(" \t\n\r\f\v");
        return std::string{str.substr(start, end - start + 1)};
    }

    [[nodiscard]] inline std::string strip_hex_prefix(std::string_view str)
    {
        if (str.size() >= 2 && (str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X"))
        {
            return std::string{str.substr(2)};
        }
        return std::string{str};
    }

    [[nodiscard]] inline constexpr std::uint16_t byte_swap_16(std::uint16_t value)
    {
        return static_cast<std::uint16_t>((value >> 8) | (value << 8));
    }

    [[nodiscard]] inline constexpr std::uint32_t byte_swap_32(std::uint32_t value)
    {
        return ((value >> 24) & 0x000000FF) |
               ((value >> 8)  & 0x0000FF00) |
               ((value << 8)  & 0x00FF0000) |
               ((value << 24) & 0xFF000000);
    }

    [[nodiscard]] inline constexpr std::uint64_t byte_swap_64(std::uint64_t value)
    {
        return ((value >> 56) & 0x00000000000000FFULL) |
               ((value >> 40) & 0x000000000000FF00ULL) |
               ((value >> 24) & 0x0000000000FF0000ULL) |
               ((value >> 8)  & 0x00000000FF000000ULL) |
               ((value << 8)  & 0x000000FF00000000ULL) |
               ((value << 24) & 0x0000FF0000000000ULL) |
               ((value << 40) & 0x00FF000000000000ULL) |
               ((value << 56) & 0xFF00000000000000ULL);
    }

    [[nodiscard]] inline float byte_swap_float(float value)
    {
        std::uint32_t temp;
        std::memcpy(&temp, &value, sizeof(temp));
        temp = byte_swap_32(temp);
        float result;
        std::memcpy(&result, &temp, sizeof(result));
        return result;
    }

    [[nodiscard]] inline double byte_swap_double(double value)
    {
        std::uint64_t temp;
        std::memcpy(&temp, &value, sizeof(temp));
        temp = byte_swap_64(temp);
        double result;
        std::memcpy(&result, &temp, sizeof(result));
        return result;
    }

    class ValueConverter
    {
    public:
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse(
            ValueType type,
            const std::string& input,
            bool hexadecimal = false)
        {
            return parse(type, input, hexadecimal, Endianness::Little);
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse(
            ValueType type,
            const std::string& input,
            bool hexadecimal,
            Endianness endianness)
        {
            if (input.empty())
            {
                return std::nullopt;
            }

            switch (type)
            {
                case ValueType::Int8:
                    return parse_integer<std::int8_t>(input, hexadecimal);
                case ValueType::Int16:
                    return parse_integer<std::int16_t>(input, hexadecimal);
                case ValueType::Int32:
                    return parse_integer<std::int32_t>(input, hexadecimal);
                case ValueType::Int64:
                    return parse_integer<std::int64_t>(input, hexadecimal);
                case ValueType::UInt8:
                    return parse_integer<std::uint8_t>(input, hexadecimal);
                case ValueType::UInt16:
                    return parse_integer<std::uint16_t>(input, hexadecimal);
                case ValueType::UInt32:
                    return parse_integer<std::uint32_t>(input, hexadecimal);
                case ValueType::UInt64:
                    return parse_integer<std::uint64_t>(input, hexadecimal);
                case ValueType::Float:
                    return parse_float<float>(input);
                case ValueType::Double:
                    return parse_float<double>(input);
                case ValueType::StringASCII:
                    return parse_string_ascii(input);
                case ValueType::StringUTF8:
                    return parse_string_utf8(input);
                case ValueType::StringUTF16:
                    return endianness == Endianness::Big
                        ? parse_string_utf16be(input)
                        : parse_string_utf16le(input);
                case ValueType::StringUTF32:
                    return endianness == Endianness::Big
                        ? parse_string_utf32be(input)
                        : parse_string_utf32le(input);
                default:
                    return std::nullopt;
            }
        }

        [[nodiscard]] static std::string format(
            ValueType type,
            const void* data,
            std::size_t size,
            bool hexadecimal = false)
        {
            return format(type, data, size, hexadecimal, Endianness::Little);
        }

        [[nodiscard]] static std::string format(
            ValueType type,
            const void* data,
            std::size_t size,
            bool hexadecimal,
            Endianness endianness)
        {
            if (!data || size == 0)
            {
                return "";
            }

            if (is_numeric_type(type) && needs_endian_swap(endianness))
            {
                const auto typeSize = get_value_type_size(type);
                if (typeSize > 0 && typeSize <= size)
                {
                    std::array<std::uint8_t, 8> swapped{};
                    std::memcpy(swapped.data(), data, typeSize);
                    std::ranges::reverse(std::span{swapped.data(), typeSize});
                    return format(type, swapped.data(), typeSize, hexadecimal, get_host_endianness());
                }
            }

            switch (type)
            {
                case ValueType::Int8:
                    return format_integer<std::int8_t>(data, hexadecimal);
                case ValueType::Int16:
                    return format_integer<std::int16_t>(data, hexadecimal);
                case ValueType::Int32:
                    return format_integer<std::int32_t>(data, hexadecimal);
                case ValueType::Int64:
                    return format_integer<std::int64_t>(data, hexadecimal);
                case ValueType::UInt8:
                    return format_integer<std::uint8_t>(data, hexadecimal);
                case ValueType::UInt16:
                    return format_integer<std::uint16_t>(data, hexadecimal);
                case ValueType::UInt32:
                    return format_integer<std::uint32_t>(data, hexadecimal);
                case ValueType::UInt64:
                    return format_integer<std::uint64_t>(data, hexadecimal);
                case ValueType::Float:
                    return format_float<float>(data);
                case ValueType::Double:
                    return format_float<double>(data);
                case ValueType::StringASCII:
                case ValueType::StringUTF8:
                    return format_string(data, size);
                case ValueType::StringUTF16:
                    return endianness == Endianness::Big
                        ? format_string_utf16be(data, size)
                        : format_string_utf16le(data, size);
                case ValueType::StringUTF32:
                    return endianness == Endianness::Big
                        ? format_string_utf32be(data, size)
                        : format_string_utf32le(data, size);
                default:
                    return "";
            }
        }

    private:
        template<typename T>
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_integer(
            const std::string& input, bool hexadecimal)
        {
            std::string trimmed = trim_whitespace(input);
            if (trimmed.empty())
            {
                return std::nullopt;
            }

            if (hexadecimal)
            {
                trimmed = strip_hex_prefix(trimmed);
            }

            T value{};
            const int base = hexadecimal ? 16 : 10;

            const auto [ptr, ec] = std::from_chars(
                trimmed.data(), trimmed.data() + trimmed.size(), value, base);

            if (ec != std::errc{} || ptr != trimmed.data() + trimmed.size())
            {
                return std::nullopt;
            }

            std::vector<std::uint8_t> result(sizeof(T));
            std::memcpy(result.data(), &value, sizeof(T));
            return result;
        }

        template<typename T>
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_float(const std::string& input)
        {
            std::string trimmed = trim_whitespace(input);
            if (trimmed.empty())
            {
                return std::nullopt;
            }

            try
            {
                T value{};
                if constexpr (std::is_same_v<T, float>)
                {
                    value = std::stof(trimmed);
                }
                else
                {
                    value = std::stod(trimmed);
                }

                std::vector<std::uint8_t> result(sizeof(T));
                std::memcpy(result.data(), &value, sizeof(T));
                return result;
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_string_ascii(const std::string& input)
        {
            std::vector<std::uint8_t> result(input.begin(), input.end());
            result.push_back(0);
            return result;
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_string_utf8(const std::string& input)
        {
            std::vector<std::uint8_t> result(input.begin(), input.end());
            result.push_back(0);
            return result;
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_string_utf16le(const std::string& input)
        {
            std::vector<std::uint8_t> result;
            result.reserve(input.size() * 2 + 2);
            for (const unsigned char c : input)
            {
                result.push_back(c);
                result.push_back(0);
            }
            result.push_back(0);
            result.push_back(0);
            return result;
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_string_utf16be(const std::string& input)
        {
            std::vector<std::uint8_t> result;
            result.reserve(input.size() * 2 + 2);
            for (const unsigned char c : input)
            {
                result.push_back(0);
                result.push_back(c);
            }
            result.push_back(0);
            result.push_back(0);
            return result;
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_string_utf32le(const std::string& input)
        {
            std::vector<std::uint8_t> result;
            result.reserve(input.size() * 4 + 4);
            for (const unsigned char c : input)
            {
                result.push_back(c);
                result.push_back(0);
                result.push_back(0);
                result.push_back(0);
            }
            result.push_back(0);
            result.push_back(0);
            result.push_back(0);
            result.push_back(0);
            return result;
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_string_utf32be(const std::string& input)
        {
            std::vector<std::uint8_t> result;
            result.reserve(input.size() * 4 + 4);
            for (const unsigned char c : input)
            {
                result.push_back(0);
                result.push_back(0);
                result.push_back(0);
                result.push_back(c);
            }
            result.push_back(0);
            result.push_back(0);
            result.push_back(0);
            result.push_back(0);
            return result;
        }

        template<typename T>
        [[nodiscard]] static std::string format_integer(const void* data, bool hexadecimal)
        {
            T value{};
            std::memcpy(&value, data, sizeof(T));

            if (hexadecimal)
            {
                std::ostringstream oss;
                if constexpr (sizeof(T) == 1)
                {
                    oss << std::hex << std::uppercase << static_cast<int>(value);
                }
                else
                {
                    oss << std::hex << std::uppercase << value;
                }
                return oss.str();
            }
            else
            {
                if constexpr (sizeof(T) == 1)
                {
                    return std::to_string(static_cast<int>(value));
                }
                else
                {
                    return std::to_string(value);
                }
            }
        }

        template<typename T>
        [[nodiscard]] static std::string format_float(const void* data)
        {
            T value{};
            std::memcpy(&value, data, sizeof(T));

            std::ostringstream oss;
            oss << std::setprecision(std::is_same_v<T, float> ? 7 : 15) << value;
            return oss.str();
        }

        [[nodiscard]] static std::string format_string(const void* data, std::size_t size)
        {
            const auto* str = static_cast<const char*>(data);
            std::size_t len = 0;
            while (len < size && str[len] != '\0')
            {
                ++len;
            }
            return std::string(str, len);
        }

        [[nodiscard]] static std::string format_string_utf16le(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            std::string result;
            result.reserve(size / 2);

            for (std::size_t i = 0; i + 1 < size; i += 2)
            {
                const std::uint16_t ch = static_cast<std::uint16_t>(bytes[i]) |
                                         (static_cast<std::uint16_t>(bytes[i + 1]) << 8);
                if (ch == 0)
                {
                    break;
                }
                if (ch < 128)
                {
                    result.push_back(static_cast<char>(ch));
                }
                else
                {
                    result.push_back('?');
                }
            }
            return result;
        }

        [[nodiscard]] static std::string format_string_utf16be(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            std::string result;
            result.reserve(size / 2);

            for (std::size_t i = 0; i + 1 < size; i += 2)
            {
                const std::uint16_t ch = (static_cast<std::uint16_t>(bytes[i]) << 8) |
                                         static_cast<std::uint16_t>(bytes[i + 1]);
                if (ch == 0)
                {
                    break;
                }
                if (ch < 128)
                {
                    result.push_back(static_cast<char>(ch));
                }
                else
                {
                    result.push_back('?');
                }
            }
            return result;
        }

        [[nodiscard]] static std::string format_string_utf32le(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            std::string result;
            result.reserve(size / 4);

            for (std::size_t i = 0; i + 3 < size; i += 4)
            {
                const std::uint32_t ch = static_cast<std::uint32_t>(bytes[i]) |
                                         (static_cast<std::uint32_t>(bytes[i + 1]) << 8) |
                                         (static_cast<std::uint32_t>(bytes[i + 2]) << 16) |
                                         (static_cast<std::uint32_t>(bytes[i + 3]) << 24);
                if (ch == 0)
                {
                    break;
                }
                if (ch < 128)
                {
                    result.push_back(static_cast<char>(ch));
                }
                else
                {
                    result.push_back('?');
                }
            }
            return result;
        }

        [[nodiscard]] static std::string format_string_utf32be(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            std::string result;
            result.reserve(size / 4);

            for (std::size_t i = 0; i + 3 < size; i += 4)
            {
                const std::uint32_t ch = (static_cast<std::uint32_t>(bytes[i]) << 24) |
                                         (static_cast<std::uint32_t>(bytes[i + 1]) << 16) |
                                         (static_cast<std::uint32_t>(bytes[i + 2]) << 8) |
                                         static_cast<std::uint32_t>(bytes[i + 3]);
                if (ch == 0)
                {
                    break;
                }
                if (ch < 128)
                {
                    result.push_back(static_cast<char>(ch));
                }
                else
                {
                    result.push_back('?');
                }
            }
            return result;
        }
    };

} // namespace Vertex::Scanner
