//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <vertex/runtime/iloader.hh>

#include <sdk/statuscode.h>

#include <functional>
#include <string>

class asIScriptEngine;

namespace Vertex::Scripting::Stdlib
{
    class ScriptUtility final
    {
    public:
        explicit ScriptUtility(Runtime::ILoader& loader);

        [[nodiscard]] StatusCode register_api(asIScriptEngine& engine);

        [[nodiscard]] std::string get_current_plugin() const;
        [[nodiscard]] std::string get_host_os() const;
        [[nodiscard]] std::string get_version() const;
        [[nodiscard]] std::string get_vendor() const;
        [[nodiscard]] std::string get_name() const;

    private:
        std::reference_wrapper<Runtime::ILoader> m_loader;
    };
}
