//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/runtime/command.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

namespace Vertex::Scanner
{
    enum class ScannerEventKind : std::uint32_t
    {
        None                = 0,
        ScanProgress        = 1u << 0,
        ScanComplete        = 1u << 1,
        ScanError           = 1u << 2,
        ValuesChanged       = 1u << 3,
        Cancelled           = 1u << 4,
        TypeRegistered      = 1u << 5,
        TypeUnregistered    = 1u << 6,
        TypeSchemaChanged   = 1u << 7,
        RegistryInvalidated = 1u << 8
    };

    using ScannerEventKindMask = std::underlying_type_t<ScannerEventKind>;

    [[nodiscard]] inline constexpr ScannerEventKindMask operator|(ScannerEventKind lhs, ScannerEventKind rhs) noexcept
    {
        return static_cast<ScannerEventKindMask>(lhs) | static_cast<ScannerEventKindMask>(rhs);
    }

    [[nodiscard]] inline constexpr ScannerEventKindMask operator|(ScannerEventKindMask lhs, ScannerEventKind rhs) noexcept
    {
        return lhs | static_cast<ScannerEventKindMask>(rhs);
    }

    [[nodiscard]] inline constexpr ScannerEventKindMask operator&(ScannerEventKindMask lhs, ScannerEventKind rhs) noexcept
    {
        return lhs & static_cast<ScannerEventKindMask>(rhs);
    }

    struct ScanProgressInfo final
    {
        std::uint8_t percentComplete{};
        std::uint64_t addressesScanned{};
        std::uint64_t matchesSoFar{};
    };

    struct ScanCompleteInfo final
    {
        std::uint64_t matchCount{};
        std::chrono::milliseconds elapsed{};
        TypeId typeId{TypeId::Invalid};
    };

    struct ScanErrorInfo final
    {
        StatusCode code{};
        std::string description{};
    };

    struct ValuesChangedInfo final
    {
        std::uint64_t generation{};
        std::uint32_t changedCount{};
    };

    struct CancelledInfo final
    {
        Runtime::CommandId cancelledId{Runtime::INVALID_COMMAND_ID};
    };

    struct TypeRegisteredInfo final
    {
        TypeSchema schema{};
    };

    struct TypeUnregisteredInfo final
    {
        TypeId id{TypeId::Invalid};
        std::string name{};
    };

    struct TypeSchemaChangedInfo final
    {
        TypeSchema schema{};
    };

    struct RegistryInvalidatedInfo final
    {
        std::size_t sourcePluginIndex{std::numeric_limits<std::size_t>::max()};
        std::uint32_t removedCount{};
    };

    struct ScannerEvent final
    {
        ScannerEventKind kind{ScannerEventKind::None};
        std::variant<std::monostate,
                     ScanProgressInfo,
                     ScanCompleteInfo,
                     ScanErrorInfo,
                     ValuesChangedInfo,
                     CancelledInfo,
                     TypeRegisteredInfo,
                     TypeUnregisteredInfo,
                     TypeSchemaChangedInfo,
                     RegistryInvalidatedInfo> detail{};
    };
}
