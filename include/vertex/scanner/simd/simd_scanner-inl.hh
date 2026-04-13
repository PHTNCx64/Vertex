//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

// no pragma once here because of Google Highway using it for dispatch
#include <vertex/scanner/simd/simd_scanner.hh>
#include <hwy/cache_control.h>
#include <hwy/highway.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>

HWY_BEFORE_NAMESPACE();
namespace Vertex::Scanner::Simd::HWY_NAMESPACE
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr std::size_t SIMD_PREFETCH_DISTANCE = 256;

    template<class T>
    [[nodiscard]] std::size_t simd_scan_single_input_impl(
        const std::uint8_t* buffer,
        const std::size_t bufferSize,
        const std::size_t alignment,
        [[maybe_unused]] const std::size_t dataSize,
        const std::uint8_t* input,
        [[maybe_unused]] const std::uint8_t* input2,
        ScanResult& results,
        const std::uint64_t baseAddress,
        const auto& maskComparator,
        const auto& scalarComparator)
    {
        if (bufferSize < sizeof(T))
        {
            return bufferSize;
        }

        const std::size_t scanEnd = bufferSize - sizeof(T) + 1;
        if (input == nullptr)
        {
            return scanEnd;
        }

        T targetVal{};
        std::copy_n(input, sizeof(T), reinterpret_cast<std::uint8_t*>(&targetVal));
        const hn::ScalableTag<T> tag{};
        const auto target = hn::Set(tag, targetVal);

        const std::size_t lanes = hn::Lanes(tag);
        const std::size_t simdStride = lanes * sizeof(T);
        const std::size_t simdEnd = (bufferSize / simdStride) * simdStride;

        std::size_t offset{};
        for (; offset < simdEnd; offset += simdStride)
        {
            const std::size_t prefetchOffset = offset + SIMD_PREFETCH_DISTANCE;
            if (prefetchOffset < simdEnd)
            {
                hwy::Prefetch(buffer + prefetchOffset);
            }

            const auto current = hn::LoadU(tag, reinterpret_cast<const T*>(buffer + offset));
            const auto mask = maskComparator(current, target);

            if (!hn::AllFalse(tag, mask))
            {
                const auto laneIndices = hn::Iota(tag, T{0});
                HWY_ALIGN T matchedLanes[hn::MaxLanes(tag)];
                const std::size_t matchedCount = hn::CompressStore(laneIndices, mask, tag, matchedLanes);

                for (std::size_t i{}; i < matchedCount; ++i)
                {
                    const std::size_t lane = static_cast<std::size_t>(matchedLanes[i]);
                    const std::size_t laneOffset = offset + lane * sizeof(T);
                    results.add_match(baseAddress + laneOffset, buffer + laneOffset, sizeof(T));
                }
            }

            if (results.matchesFound >= BATCH_CHECK_INTERVAL) [[unlikely]]
            {
                return offset + simdStride;
            }
        }

        for (; offset < scanEnd; offset += alignment)
        {
            T currentVal{};
            std::copy_n(buffer + offset, sizeof(T), reinterpret_cast<std::uint8_t*>(&currentVal));

            if (scalarComparator(currentVal, targetVal))
            {
                results.add_match(baseAddress + offset, buffer + offset, sizeof(T));
            }

            if (results.matchesFound >= BATCH_CHECK_INTERVAL) [[unlikely]]
            {
                return offset + alignment;
            }
        }

        return scanEnd;
    }

    template<class T>
    [[nodiscard]] std::size_t simd_scan_dual_input_impl(
        const std::uint8_t* buffer,
        const std::size_t bufferSize,
        const std::size_t alignment,
        [[maybe_unused]] const std::size_t dataSize,
        const std::uint8_t* input,
        const std::uint8_t* input2,
        ScanResult& results,
        const std::uint64_t baseAddress,
        const auto& maskComparator,
        const auto& scalarComparator)
    {
        if (bufferSize < sizeof(T))
        {
            return bufferSize;
        }

        const std::size_t scanEnd = bufferSize - sizeof(T) + 1;
        if (input == nullptr || input2 == nullptr)
        {
            return scanEnd;
        }

        T minVal{};
        T maxVal{};
        std::copy_n(input, sizeof(T), reinterpret_cast<std::uint8_t*>(&minVal));
        std::copy_n(input2, sizeof(T), reinterpret_cast<std::uint8_t*>(&maxVal));

        const hn::ScalableTag<T> tag{};
        const auto minTarget = hn::Set(tag, minVal);
        const auto maxTarget = hn::Set(tag, maxVal);

        const std::size_t lanes = hn::Lanes(tag);
        const std::size_t simdStride = lanes * sizeof(T);
        const std::size_t simdEnd = (bufferSize / simdStride) * simdStride;

        std::size_t offset{};
        for (; offset < simdEnd; offset += simdStride)
        {
            const std::size_t prefetchOffset = offset + SIMD_PREFETCH_DISTANCE;
            if (prefetchOffset < simdEnd)
            {
                hwy::Prefetch(buffer + prefetchOffset);
            }

            const auto current = hn::LoadU(tag, reinterpret_cast<const T*>(buffer + offset));
            const auto mask = maskComparator(current, minTarget, maxTarget);

            if (!hn::AllFalse(tag, mask))
            {
                const auto laneIndices = hn::Iota(tag, T{0});
                HWY_ALIGN T matchedLanes[hn::MaxLanes(tag)];
                const std::size_t matchedCount = hn::CompressStore(laneIndices, mask, tag, matchedLanes);

                for (std::size_t i{}; i < matchedCount; ++i)
                {
                    const std::size_t lane = static_cast<std::size_t>(matchedLanes[i]);
                    const std::size_t laneOffset = offset + lane * sizeof(T);
                    results.add_match(baseAddress + laneOffset, buffer + laneOffset, sizeof(T));
                }
            }

            if (results.matchesFound >= BATCH_CHECK_INTERVAL) [[unlikely]]
            {
                return offset + simdStride;
            }
        }

        for (; offset < scanEnd; offset += alignment)
        {
            T currentVal{};
            std::copy_n(buffer + offset, sizeof(T), reinterpret_cast<std::uint8_t*>(&currentVal));

            if (scalarComparator(currentVal, minVal, maxVal))
            {
                results.add_match(baseAddress + offset, buffer + offset, sizeof(T));
            }

            if (results.matchesFound >= BATCH_CHECK_INTERVAL) [[unlikely]]
            {
                return offset + alignment;
            }
        }

        return scanEnd;
    }

    template<class T>
    [[nodiscard]] std::size_t simd_scan_exact_impl(
        const std::uint8_t* buffer,
        const std::size_t bufferSize,
        const std::size_t alignment,
        const std::size_t dataSize,
        const std::uint8_t* input,
        const std::uint8_t* input2,
        ScanResult& results,
        const std::uint64_t baseAddress)
    {
        if constexpr (std::is_same_v<T, float>)
        {
            constexpr float EPSILON = 0.0001f;
            const hn::ScalableTag<T> tag{};
            const auto epsilon = hn::Set(tag, EPSILON);

            return simd_scan_single_input_impl<T>(
                buffer,
                bufferSize,
                alignment,
                dataSize,
                input,
                input2,
                results,
                baseAddress,
                [epsilon](const auto current, const auto target)
                {
                    return hn::Lt(hn::Abs(hn::Sub(current, target)), epsilon);
                },
                [](const T currentVal, const T targetVal)
                {
                    return std::fabs(currentVal - targetVal) < EPSILON;
                });
        }

        if constexpr (std::is_same_v<T, double>)
        {
            constexpr double EPSILON = 0.0000001;
            const hn::ScalableTag<T> tag{};
            const auto epsilon = hn::Set(tag, EPSILON);

            return simd_scan_single_input_impl<T>(
                buffer,
                bufferSize,
                alignment,
                dataSize,
                input,
                input2,
                results,
                baseAddress,
                [epsilon](const auto current, const auto target)
                {
                    return hn::Lt(hn::Abs(hn::Sub(current, target)), epsilon);
                },
                [](const T currentVal, const T targetVal)
                {
                    return std::fabs(currentVal - targetVal) < EPSILON;
                });
        }

        return simd_scan_single_input_impl<T>(
            buffer,
            bufferSize,
            alignment,
            dataSize,
            input,
            input2,
            results,
            baseAddress,
            [](const auto current, const auto target)
            {
                return hn::Eq(current, target);
            },
            [](const T currentVal, const T targetVal)
            {
                return currentVal == targetVal;
            });
    }

    template<class T>
    [[nodiscard]] std::size_t simd_scan_greater_than_impl(
        const std::uint8_t* buffer,
        const std::size_t bufferSize,
        const std::size_t alignment,
        const std::size_t dataSize,
        const std::uint8_t* input,
        const std::uint8_t* input2,
        ScanResult& results,
        const std::uint64_t baseAddress)
    {
        return simd_scan_single_input_impl<T>(
            buffer,
            bufferSize,
            alignment,
            dataSize,
            input,
            input2,
            results,
            baseAddress,
            [](const auto current, const auto target)
            {
                return hn::Gt(current, target);
            },
            [](const T currentVal, const T targetVal)
            {
                return currentVal > targetVal;
            });
    }

    template<class T>
    [[nodiscard]] std::size_t simd_scan_less_than_impl(
        const std::uint8_t* buffer,
        const std::size_t bufferSize,
        const std::size_t alignment,
        const std::size_t dataSize,
        const std::uint8_t* input,
        const std::uint8_t* input2,
        ScanResult& results,
        const std::uint64_t baseAddress)
    {
        return simd_scan_single_input_impl<T>(
            buffer,
            bufferSize,
            alignment,
            dataSize,
            input,
            input2,
            results,
            baseAddress,
            [](const auto current, const auto target)
            {
                return hn::Lt(current, target);
            },
            [](const T currentVal, const T targetVal)
            {
                return currentVal < targetVal;
            });
    }

    template<class T>
    [[nodiscard]] std::size_t simd_scan_between_impl(
        const std::uint8_t* buffer,
        const std::size_t bufferSize,
        const std::size_t alignment,
        const std::size_t dataSize,
        const std::uint8_t* input,
        const std::uint8_t* input2,
        ScanResult& results,
        const std::uint64_t baseAddress)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return simd_scan_dual_input_impl<T>(
                buffer,
                bufferSize,
                alignment,
                dataSize,
                input,
                input2,
                results,
                baseAddress,
                [](const auto current, const auto minTarget, const auto maxTarget)
                {
                    const auto validCurrent = hn::Not(hn::IsNaN(current));
                    const auto validMin = hn::Not(hn::IsNaN(minTarget));
                    const auto validMax = hn::Not(hn::IsNaN(maxTarget));
                    const auto inRange = hn::And(
                        hn::Ge(current, minTarget),
                        hn::Le(current, maxTarget));

                    return hn::And(
                        hn::And(validCurrent, validMin),
                        hn::And(validMax, inRange));
                },
                [](const T currentVal, const T minVal, const T maxVal)
                {
                    return !std::isnan(currentVal) &&
                           !std::isnan(minVal) &&
                           !std::isnan(maxVal) &&
                           currentVal >= minVal &&
                           currentVal <= maxVal;
                });
        }

        return simd_scan_dual_input_impl<T>(
            buffer,
            bufferSize,
            alignment,
            dataSize,
            input,
            input2,
            results,
            baseAddress,
            [](const auto current, const auto minTarget, const auto maxTarget)
            {
                return hn::And(
                    hn::Ge(current, minTarget),
                    hn::Le(current, maxTarget));
            },
            [](const T currentVal, const T minVal, const T maxVal)
            {
                return currentVal >= minVal && currentVal <= maxVal;
            });
    }

#define VERTEX_DEFINE_SIMD_SCAN_WRAPPER(FN_NAME, VALUE_TYPE, IMPL_FN)                  \
    [[nodiscard]] inline std::size_t FN_NAME(                                           \
        const std::uint8_t* buffer,                                                     \
        const std::size_t bufferSize,                                                   \
        const std::size_t alignment,                                                    \
        const std::size_t dataSize,                                                     \
        const std::uint8_t* input,                                                      \
        const std::uint8_t* input2,                                                     \
        ScanResult& results,                                                            \
        const std::uint64_t baseAddress)                                                \
    {                                                                                   \
        return IMPL_FN<VALUE_TYPE>(                                                     \
            buffer,                                                                     \
            bufferSize,                                                                 \
            alignment,                                                                  \
            dataSize,                                                                   \
            input,                                                                      \
            input2,                                                                     \
            results,                                                                    \
            baseAddress);                                                               \
    }

    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_i8, std::int8_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_i16, std::int16_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_i32, std::int32_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_i64, std::int64_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_u8, std::uint8_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_u16, std::uint16_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_u32, std::uint32_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_u64, std::uint64_t, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_f32, float, simd_scan_exact_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_exact_f64, double, simd_scan_exact_impl);

    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_i8, std::int8_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_i16, std::int16_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_i32, std::int32_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_i64, std::int64_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_u8, std::uint8_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_u16, std::uint16_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_u32, std::uint32_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_u64, std::uint64_t, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_f32, float, simd_scan_greater_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_greater_than_f64, double, simd_scan_greater_than_impl);

    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_i8, std::int8_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_i16, std::int16_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_i32, std::int32_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_i64, std::int64_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_u8, std::uint8_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_u16, std::uint16_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_u32, std::uint32_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_u64, std::uint64_t, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_f32, float, simd_scan_less_than_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_less_than_f64, double, simd_scan_less_than_impl);

    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_i8, std::int8_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_i16, std::int16_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_i32, std::int32_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_i64, std::int64_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_u8, std::uint8_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_u16, std::uint16_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_u32, std::uint32_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_u64, std::uint64_t, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_f32, float, simd_scan_between_impl);
    VERTEX_DEFINE_SIMD_SCAN_WRAPPER(simd_scan_between_f64, double, simd_scan_between_impl);

#undef VERTEX_DEFINE_SIMD_SCAN_WRAPPER
} // namespace Vertex::Scanner::Simd::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();
