//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/runtime/command.hh>

#include <string>
#include <variant>

namespace Vertex::Scripting::engine
{
    struct CmdExecute final
    {
        std::string moduleName{};
    };

    struct CmdStop final
    {
        std::string moduleName{};
    };

    struct CmdReload final
    {
        std::string moduleName{};
    };

    struct CmdEvaluate final
    {
        std::string moduleName{};
        std::string snippet{};
    };

    using ScriptingCommand = std::variant<CmdExecute, CmdStop, CmdReload, CmdEvaluate>;

    struct CommandResult final
    {
        Runtime::CommandId id{Runtime::INVALID_COMMAND_ID};
        StatusCode code{STATUS_OK};
        std::string moduleName{};
    };
}
