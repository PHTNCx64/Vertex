//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <vertex/log/ilog.hh>

#include <sdk/statuscode.h>

#include <functional>
#include <string>

class asIScriptEngine;

namespace Vertex::Event
{
    class EventBus;
}

namespace Vertex::Scripting::Stdlib
{
    class ScriptLogger final
    {
    public:
        ScriptLogger(Log::ILog& logService, Event::EventBus& eventBus);

        [[nodiscard]] StatusCode register_api(asIScriptEngine& engine);

        void log_info(const std::string& msg) const;
        void log_warn(const std::string& msg) const;
        void log_error(const std::string& msg) const;

    private:
        std::reference_wrapper<Log::ILog> m_logService;
        std::reference_wrapper<Event::EventBus> m_eventBus;
    };
}
