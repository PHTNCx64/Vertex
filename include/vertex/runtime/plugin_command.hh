//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/runtime/command.hh>

#include <cstddef>
#include <filesystem>
#include <string>
#include <variant>

namespace Vertex::Runtime::PluginRuntime::engine
{
    struct CmdLoad final
    {
        std::filesystem::path path{};
    };

    struct CmdUnload final
    {
        std::size_t index{};
    };

    struct CmdActivate final
    {
        std::size_t index{};
    };

    struct CmdDeactivate final
    {
    };

    using PluginCommand = std::variant<CmdLoad, CmdUnload, CmdActivate, CmdDeactivate>;

    struct CommandResult final
    {
        Runtime::CommandId id{Runtime::INVALID_COMMAND_ID};
        StatusCode code{STATUS_OK};
        std::size_t index{static_cast<std::size_t>(-1)};
        std::string filename{};
    };
}
