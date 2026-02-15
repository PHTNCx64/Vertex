//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/valuetypes.hh>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace Vertex::Scanner
{
    template<class T>
    [[nodiscard]] inline bool compare_exact(const void* current, const void* input)
    {
        T currentVal, inputVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(input), sizeof(T), reinterpret_cast<std::byte*>(&inputVal));
        return currentVal == inputVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_greater_than(const void* current, const void* input)
    {
        T currentVal, inputVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(input), sizeof(T), reinterpret_cast<std::byte*>(&inputVal));
        return currentVal > inputVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_less_than(const void* current, const void* input)
    {
        T currentVal, inputVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(input), sizeof(T), reinterpret_cast<std::byte*>(&inputVal));
        return currentVal < inputVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_between(const void* current, const void* inputMin, const void* inputMax)
    {
        T currentVal, minVal, maxVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(inputMin), sizeof(T), reinterpret_cast<std::byte*>(&minVal));
        std::copy_n(static_cast<const std::byte*>(inputMax), sizeof(T), reinterpret_cast<std::byte*>(&maxVal));
        return currentVal >= minVal && currentVal <= maxVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_changed(const void* current, const void* previous)
    {
        T currentVal, prevVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(T), reinterpret_cast<std::byte*>(&prevVal));
        return currentVal != prevVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_unchanged(const void* current, const void* previous)
    {
        T currentVal, prevVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(T), reinterpret_cast<std::byte*>(&prevVal));
        return currentVal == prevVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_increased(const void* current, const void* previous)
    {
        T currentVal, prevVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(T), reinterpret_cast<std::byte*>(&prevVal));
        return currentVal > prevVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_decreased(const void* current, const void* previous)
    {
        T currentVal, prevVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(T), reinterpret_cast<std::byte*>(&prevVal));
        return currentVal < prevVal;
    }

    template<class T>
    [[nodiscard]] inline bool compare_increased_by(const void* current, const void* previous, const void* byAmount)
    {
        T currentVal, prevVal, amount;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(T), reinterpret_cast<std::byte*>(&prevVal));
        std::copy_n(static_cast<const std::byte*>(byAmount), sizeof(T), reinterpret_cast<std::byte*>(&amount));
        return currentVal == static_cast<T>(prevVal + amount);
    }

    template<class T>
    [[nodiscard]] inline bool compare_decreased_by(const void* current, const void* previous, const void* byAmount)
    {
        T currentVal, prevVal, amount;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(T), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(T), reinterpret_cast<std::byte*>(&prevVal));
        std::copy_n(static_cast<const std::byte*>(byAmount), sizeof(T), reinterpret_cast<std::byte*>(&amount));
        return currentVal == static_cast<T>(prevVal - amount);
    }

    template<>
    [[nodiscard]] inline bool compare_exact<float>(const void* current, const void* input)
    {
        float currentVal, inputVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(float), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(input), sizeof(float), reinterpret_cast<std::byte*>(&inputVal));
        return std::fabs(currentVal - inputVal) < 0.0001f;
    }

    template<>
    [[nodiscard]] inline bool compare_exact<double>(const void* current, const void* input)
    {
        double currentVal, inputVal;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(double), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(input), sizeof(double), reinterpret_cast<std::byte*>(&inputVal));
        return std::fabs(currentVal - inputVal) < 0.0000001;
    }

    template<>
    [[nodiscard]] inline bool compare_increased_by<float>(const void* current, const void* previous, const void* byAmount)
    {
        float currentVal, prevVal, amount;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(float), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(float), reinterpret_cast<std::byte*>(&prevVal));
        std::copy_n(static_cast<const std::byte*>(byAmount), sizeof(float), reinterpret_cast<std::byte*>(&amount));
        return std::fabs(currentVal - (prevVal + amount)) < 0.0001f;
    }

    template<>
    [[nodiscard]] inline bool compare_increased_by<double>(const void* current, const void* previous, const void* byAmount)
    {
        double currentVal, prevVal, amount;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(double), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(double), reinterpret_cast<std::byte*>(&prevVal));
        std::copy_n(static_cast<const std::byte*>(byAmount), sizeof(double), reinterpret_cast<std::byte*>(&amount));
        return std::fabs(currentVal - (prevVal + amount)) < 0.0000001;
    }

    template<>
    [[nodiscard]] inline bool compare_decreased_by<float>(const void* current, const void* previous, const void* byAmount)
    {
        float currentVal, prevVal, amount;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(float), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(float), reinterpret_cast<std::byte*>(&prevVal));
        std::copy_n(static_cast<const std::byte*>(byAmount), sizeof(float), reinterpret_cast<std::byte*>(&amount));
        return std::fabs(currentVal - (prevVal - amount)) < 0.0001f;
    }

    template<>
    [[nodiscard]] inline bool compare_decreased_by<double>(const void* current, const void* previous, const void* byAmount)
    {
        double currentVal, prevVal, amount;
        std::copy_n(static_cast<const std::byte*>(current), sizeof(double), reinterpret_cast<std::byte*>(&currentVal));
        std::copy_n(static_cast<const std::byte*>(previous), sizeof(double), reinterpret_cast<std::byte*>(&prevVal));
        std::copy_n(static_cast<const std::byte*>(byAmount), sizeof(double), reinterpret_cast<std::byte*>(&amount));
        return std::fabs(currentVal - (prevVal - amount)) < 0.0000001;
    }

    [[nodiscard]] inline bool string_compare_exact(const char* memory, std::size_t memorySize,
                                                    const char* needle, std::size_t needleSize)
    {
        if (memorySize < needleSize)
        {
            return false;
        }
        return std::equal(needle, needle + needleSize, memory);
    }

    [[nodiscard]] inline bool string_compare_contains(const char* memory, std::size_t memorySize,
                                                       const char* needle, std::size_t needleSize)
    {
        if (memorySize < needleSize || needleSize == 0)
        {
            return false;
        }

        const char* end = memory + memorySize - needleSize + 1;
        for (const char* p = memory; p < end; ++p)
        {
            if (std::equal(needle, needle + needleSize, p))
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] inline bool string_compare_begins_with(const char* memory, std::size_t memorySize,
                                                          const char* needle, std::size_t needleSize)
    {
        if (memorySize < needleSize)
        {
            return false;
        }
        return std::equal(needle, needle + needleSize, memory);
    }

    [[nodiscard]] inline bool string_compare_ends_with(const char* memory, std::size_t memorySize,
                                                        const char* needle, std::size_t needleSize)
    {
        if (memorySize < needleSize)
        {
            return false;
        }
        return std::equal(needle, needle + needleSize, memory + memorySize - needleSize);
    }

    template<class T>
    [[nodiscard]] inline bool compare_value(NumericScanMode mode, const void* current,
                                             const void* input, const void* input2 = nullptr,
                                             const void* previous = nullptr)
    {
        switch (mode)
        {
            case NumericScanMode::Exact:
                return compare_exact<T>(current, input);
            case NumericScanMode::GreaterThan:
                return compare_greater_than<T>(current, input);
            case NumericScanMode::LessThan:
                return compare_less_than<T>(current, input);
            case NumericScanMode::Between:
                return input2 ? compare_between<T>(current, input, input2) : false;
            case NumericScanMode::Unknown:
                return true;
            case NumericScanMode::Changed:
                return previous ? compare_changed<T>(current, previous) : false;
            case NumericScanMode::Unchanged:
                return previous ? compare_unchanged<T>(current, previous) : false;
            case NumericScanMode::Increased:
                return previous ? compare_increased<T>(current, previous) : false;
            case NumericScanMode::Decreased:
                return previous ? compare_decreased<T>(current, previous) : false;
            case NumericScanMode::IncreasedBy:
                return (previous && input) ? compare_increased_by<T>(current, previous, input) : false;
            case NumericScanMode::DecreasedBy:
                return (previous && input) ? compare_decreased_by<T>(current, previous, input) : false;
            default:
                return false;
        }
    }

    [[nodiscard]] inline bool compare_string(StringScanMode mode, const char* memory, std::size_t memorySize,
                                              const char* needle, std::size_t needleSize)
    {
        switch (mode)
        {
            case StringScanMode::Exact:
                return string_compare_exact(memory, memorySize, needle, needleSize);
            case StringScanMode::Contains:
                return string_compare_contains(memory, memorySize, needle, needleSize);
            case StringScanMode::BeginsWith:
                return string_compare_begins_with(memory, memorySize, needle, needleSize);
            case StringScanMode::EndsWith:
                return string_compare_ends_with(memory, memorySize, needle, needleSize);
            default:
                return false;
        }
    }

    [[nodiscard]] inline bool compare_numeric_value(ValueType type, NumericScanMode mode,
                                                     const void* current, const void* input,
                                                     const void* input2, const void* previous)
    {
        switch (type)
        {
            case ValueType::Int8:
                return compare_value<std::int8_t>(mode, current, input, input2, previous);
            case ValueType::Int16:
                return compare_value<std::int16_t>(mode, current, input, input2, previous);
            case ValueType::Int32:
                return compare_value<std::int32_t>(mode, current, input, input2, previous);
            case ValueType::Int64:
                return compare_value<std::int64_t>(mode, current, input, input2, previous);
            case ValueType::UInt8:
                return compare_value<std::uint8_t>(mode, current, input, input2, previous);
            case ValueType::UInt16:
                return compare_value<std::uint16_t>(mode, current, input, input2, previous);
            case ValueType::UInt32:
                return compare_value<std::uint32_t>(mode, current, input, input2, previous);
            case ValueType::UInt64:
                return compare_value<std::uint64_t>(mode, current, input, input2, previous);
            case ValueType::Float:
                return compare_value<float>(mode, current, input, input2, previous);
            case ValueType::Double:
                return compare_value<double>(mode, current, input, input2, previous);
            default:
                return false;
        }
    }

    using ScanComparatorFn = bool(*)(const void*, const void*, const void*, const void*);

    template<typename T>
    [[nodiscard]] inline ScanComparatorFn resolve_comparator_for_type(NumericScanMode mode)
    {
        switch (mode)
        {
            case NumericScanMode::Exact:
                return +[](const void* c, const void* i, const void*, const void*) -> bool
                {
                    return compare_exact<T>(c, i);
                };
            case NumericScanMode::GreaterThan:
                return +[](const void* c, const void* i, const void*, const void*) -> bool
                {
                    return compare_greater_than<T>(c, i);
                };
            case NumericScanMode::LessThan:
                return +[](const void* c, const void* i, const void*, const void*) -> bool
                {
                    return compare_less_than<T>(c, i);
                };
            case NumericScanMode::Between:
                return +[](const void* c, const void* i, const void* i2, const void*) -> bool
                {
                    return compare_between<T>(c, i, i2);
                };
            case NumericScanMode::Unknown:
                return +[](const void*, const void*, const void*, const void*) -> bool
                {
                    return true;
                };
            case NumericScanMode::Changed:
                return +[](const void* c, const void*, const void*, const void* p) -> bool
                {
                    return compare_changed<T>(c, p);
                };
            case NumericScanMode::Unchanged:
                return +[](const void* c, const void*, const void*, const void* p) -> bool
                {
                    return compare_unchanged<T>(c, p);
                };
            case NumericScanMode::Increased:
                return +[](const void* c, const void*, const void*, const void* p) -> bool
                {
                    return compare_increased<T>(c, p);
                };
            case NumericScanMode::Decreased:
                return +[](const void* c, const void*, const void*, const void* p) -> bool
                {
                    return compare_decreased<T>(c, p);
                };
            case NumericScanMode::IncreasedBy:
                return +[](const void* c, const void* i, const void*, const void* p) -> bool
                {
                    return compare_increased_by<T>(c, p, i);
                };
            case NumericScanMode::DecreasedBy:
                return +[](const void* c, const void* i, const void*, const void* p) -> bool
                {
                    return compare_decreased_by<T>(c, p, i);
                };
            default:
                return +[](const void*, const void*, const void*, const void*) -> bool
                {
                    return false;
                };
        }
    }

    [[nodiscard]] inline ScanComparatorFn resolve_scan_comparator(ValueType type, NumericScanMode mode)
    {
        switch (type)
        {
            case ValueType::Int8:
                return resolve_comparator_for_type<std::int8_t>(mode);
            case ValueType::Int16:
                return resolve_comparator_for_type<std::int16_t>(mode);
            case ValueType::Int32:
                return resolve_comparator_for_type<std::int32_t>(mode);
            case ValueType::Int64:
                return resolve_comparator_for_type<std::int64_t>(mode);
            case ValueType::UInt8:
                return resolve_comparator_for_type<std::uint8_t>(mode);
            case ValueType::UInt16:
                return resolve_comparator_for_type<std::uint16_t>(mode);
            case ValueType::UInt32:
                return resolve_comparator_for_type<std::uint32_t>(mode);
            case ValueType::UInt64:
                return resolve_comparator_for_type<std::uint64_t>(mode);
            case ValueType::Float:
                return resolve_comparator_for_type<float>(mode);
            case ValueType::Double:
                return resolve_comparator_for_type<double>(mode);
            default:
                return +[](const void*, const void*, const void*, const void*) -> bool
                {
                    return false;
                };
        }
    }

} // namespace Vertex::Scanner
