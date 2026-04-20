//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <vertex/scanner/simd/simd_scanner.hh>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace
{
    using Vertex::Scanner::NumericScanMode;
    using Vertex::Scanner::ScanResult;
    using Vertex::Scanner::Simd::resolve_simd_scanner;
    using Vertex::Scanner::ValueType;

    template<class T>
    [[nodiscard]] std::vector<std::uint8_t> to_bytes(const std::vector<T>& values)
    {
        std::vector<std::uint8_t> bytes(values.size() * sizeof(T));
        if (!values.empty())
        {
            std::memcpy(bytes.data(), values.data(), bytes.size());
        }
        return bytes;
    }

    [[nodiscard]] std::vector<std::uint64_t> extract_addresses(const ScanResult& result)
    {
        std::vector<std::uint64_t> addresses;
        addresses.reserve(result.matchesFound);

        for (std::size_t i{}; i < result.matchesFound; ++i)
        {
            std::uint64_t address{};
            std::memcpy(&address, result.data() + i * result.recordSize, sizeof(address));
            addresses.push_back(address);
        }

        return addresses;
    }

    template<class T, class Predicate>
    void expect_simd_matches(
        const ValueType type,
        const NumericScanMode mode,
        const std::vector<T>& values,
        const std::optional<T> input,
        const std::optional<T> input2,
        Predicate&& predicate)
    {
        const auto capability = resolve_simd_scanner(type, mode);
        ASSERT_TRUE(capability.available);
        ASSERT_NE(capability.scanFn, nullptr);

        const std::vector<std::uint8_t> bytes = to_bytes(values);
        ScanResult result;
        result.reserve(values.size() + 8, sizeof(T));

        constexpr std::uint64_t BASE_ADDRESS = 0x4000;

        const std::uint8_t* inputPtr = input ? reinterpret_cast<const std::uint8_t*>(&*input) : nullptr;
        const std::uint8_t* input2Ptr = input2 ? reinterpret_cast<const std::uint8_t*>(&*input2) : nullptr;

        const std::size_t consumed = capability.scanFn(
            bytes.data(),
            bytes.size(),
            sizeof(T),
            sizeof(T),
            inputPtr,
            input2Ptr,
            result,
            BASE_ADDRESS);

        ASSERT_EQ(consumed, bytes.size() - sizeof(T) + 1);

        std::vector<std::uint64_t> expectedAddresses;
        for (std::size_t i{}; i < values.size(); ++i)
        {
            if (predicate(values[i]))
            {
                expectedAddresses.push_back(BASE_ADDRESS + i * sizeof(T));
            }
        }

        EXPECT_EQ(extract_addresses(result), expectedAddresses);
    }
} 

TEST(SimdScannerTest, ResolveSimdScanner_ReturnsAvailableForPhaseFourNumericModes)
{
    constexpr std::array numericTypes{
        ValueType::Int8,
        ValueType::Int16,
        ValueType::Int32,
        ValueType::Int64,
        ValueType::UInt8,
        ValueType::UInt16,
        ValueType::UInt32,
        ValueType::UInt64,
        ValueType::Float,
        ValueType::Double};

    constexpr std::array phaseFourModes{
        NumericScanMode::Exact,
        NumericScanMode::GreaterThan,
        NumericScanMode::LessThan,
        NumericScanMode::Between};

    for (const auto mode : phaseFourModes)
    {
        for (const auto type : numericTypes)
        {
            const auto capability = resolve_simd_scanner(type, mode);
            EXPECT_TRUE(capability.available) << "Mode " << static_cast<int>(mode) << " type " << static_cast<int>(type);
            EXPECT_NE(capability.scanFn, nullptr);
        }
    }
}

TEST(SimdScannerTest, ResolveSimdScanner_ReturnsUnavailableForUnsupportedModes)
{
    constexpr std::array unsupportedModes{
        NumericScanMode::Unknown,
        NumericScanMode::Changed,
        NumericScanMode::Unchanged,
        NumericScanMode::Increased,
        NumericScanMode::Decreased,
        NumericScanMode::IncreasedBy,
        NumericScanMode::DecreasedBy};

    for (const auto mode : unsupportedModes)
    {
        const auto capability = resolve_simd_scanner(ValueType::Int32, mode);
        EXPECT_FALSE(capability.available);
        EXPECT_EQ(capability.scanFn, nullptr);
    }
}

TEST(SimdScannerTest, SimdGreaterThan_MatchesScalar_Int32)
{
    const std::vector<std::int32_t> values{7, -5, 12, 0, 4, 9, 12, 3, -1};
    constexpr std::int32_t threshold = 4;

    expect_simd_matches<std::int32_t>(
        ValueType::Int32,
        NumericScanMode::GreaterThan,
        values,
        threshold,
        std::nullopt,
        [](const std::int32_t value)
        {
            return value > threshold;
        });
}

TEST(SimdScannerTest, SimdLessThan_MatchesScalar_UInt16)
{
    const std::vector<std::uint16_t> values{2, 14, 1, 65535, 9, 8, 3, 10};
    constexpr std::uint16_t threshold = 9;

    expect_simd_matches<std::uint16_t>(
        ValueType::UInt16,
        NumericScanMode::LessThan,
        values,
        threshold,
        std::nullopt,
        [](const std::uint16_t value)
        {
            return value < threshold;
        });
}

TEST(SimdScannerTest, SimdBetween_MatchesScalar_Int8)
{
    const std::vector<std::int8_t> values{-10, -4, -3, 0, 2, 5, 9, 12};
    constexpr std::int8_t minVal = -3;
    constexpr std::int8_t maxVal = 5;

    expect_simd_matches<std::int8_t>(
        ValueType::Int8,
        NumericScanMode::Between,
        values,
        minVal,
        maxVal,
        [](const std::int8_t value)
        {
            return value >= minVal && value <= maxVal;
        });
}

TEST(SimdScannerTest, SimdBetween_MissingSecondInputYieldsNoMatches)
{
    const std::vector<std::int32_t> values{1, 2, 3, 4, 5, 6, 7};
    constexpr std::int32_t minVal = 2;

    expect_simd_matches<std::int32_t>(
        ValueType::Int32,
        NumericScanMode::Between,
        values,
        minVal,
        std::nullopt,
        [](const std::int32_t)
        {
            return false;
        });
}

TEST(SimdScannerTest, SimdExact_MatchesScalar_FloatToleranceAndNaN)
{
    const std::vector<float> values{
        4.99995f,
        5.00005f,
        5.0f,
        4.99970f,
        5.00020f,
        std::numeric_limits<float>::quiet_NaN(),
        10.0f};
    constexpr float target = 5.0f;
    constexpr float epsilon = 0.0001f;

    expect_simd_matches<float>(
        ValueType::Float,
        NumericScanMode::Exact,
        values,
        target,
        std::nullopt,
        [](const float value)
        {
            return std::fabs(value - target) < epsilon;
        });
}

TEST(SimdScannerTest, SimdGreaterThan_MatchesScalar_Double)
{
    const std::vector<double> values{-4.0, -1.2, 0.0, 2.5, 5.0, std::numeric_limits<double>::quiet_NaN()};
    constexpr double threshold = 0.5;

    expect_simd_matches<double>(
        ValueType::Double,
        NumericScanMode::GreaterThan,
        values,
        threshold,
        std::nullopt,
        [](const double value)
        {
            return value > threshold;
        });
}

TEST(SimdScannerTest, SimdBetween_MatchesScalar_Double)
{
    const std::vector<double> values{
        -1.2,
        -0.5,
        -0.49,
        0.0,
        0.49,
        0.5,
        0.51,
        std::numeric_limits<double>::quiet_NaN()};
    constexpr double minVal = -0.5;
    constexpr double maxVal = 0.5;

    expect_simd_matches<double>(
        ValueType::Double,
        NumericScanMode::Between,
        values,
        minVal,
        maxVal,
        [](const double value)
        {
            return value >= minVal && value <= maxVal;
        });
}

TEST(SimdScannerTest, SimdBetween_NaNBoundsYieldNoMatches_Float)
{
    const std::vector<float> values{-1.0f, -0.1f, 0.0f, 0.1f, 1.0f, std::numeric_limits<float>::quiet_NaN()};
    const float minVal = std::numeric_limits<float>::quiet_NaN();
    constexpr float maxVal = 1.0f;

    expect_simd_matches<float>(
        ValueType::Float,
        NumericScanMode::Between,
        values,
        minVal,
        maxVal,
        [](const float)
        {
            return false;
        });
}
