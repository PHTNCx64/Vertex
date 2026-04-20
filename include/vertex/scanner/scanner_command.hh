//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <variant>
#include <vector>

#include <sdk/memory.h>
#include <sdk/statuscode.h>
#include <vertex/runtime/command.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/scanconfig.hh>
#include <vertex/scanner/scanner_typeschema.hh>

namespace Vertex::Scanner::service
{
    struct CmdStartScan final
    {
        ScanConfiguration config{};
        std::vector<ScanRegion> regions{};
    };

    struct CmdNextScan final
    {
        ScanConfiguration config{};
    };

    struct CmdUndoScan final
    {
    };

    struct CmdStopScan final
    {
    };

    struct CmdCancel final
    {
        Runtime::CommandId target{Runtime::INVALID_COMMAND_ID};
    };

    struct CmdRefreshValues final
    {
    };

    struct CmdRegisterType final
    {
        const ::DataType* sdkType{nullptr};
        std::size_t sourcePluginIndex{std::numeric_limits<std::size_t>::max()};
        std::shared_ptr<Runtime::LibraryHandle> libraryKeepalive{};
    };

    struct CmdUnregisterType final
    {
        TypeId id{TypeId::Invalid};
    };

    struct CmdQueryTypes final
    {
    };

    using Command = std::variant<
        CmdStartScan,
        CmdNextScan,
        CmdUndoScan,
        CmdStopScan,
        CmdCancel,
        CmdRefreshValues,
        CmdRegisterType,
        CmdUnregisterType,
        CmdQueryTypes>;

    struct StartScanResultPayload final
    {
        std::uint64_t matchCount{};
        std::chrono::milliseconds elapsed{};
    };

    struct RegisterTypeResultPayload final
    {
        TypeId id{TypeId::Invalid};
    };

    struct QueryTypesResultPayload final
    {
        std::vector<TypeSchema> types{};
    };

    using CommandResultPayload = std::variant<
        std::monostate,
        StartScanResultPayload,
        RegisterTypeResultPayload,
        QueryTypesResultPayload>;

    struct CommandResult final
    {
        Runtime::CommandId id{Runtime::INVALID_COMMAND_ID};
        StatusCode code{STATUS_OK};
        CommandResultPayload payload{};
    };
}
