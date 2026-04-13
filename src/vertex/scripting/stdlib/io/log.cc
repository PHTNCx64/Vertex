//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/io.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/event/types/scriptoutputevent.hh>

#include <angelscript.h>

namespace Vertex::Scripting::Stdlib
{
    ScriptLogger::ScriptLogger(Log::ILog& logService, Event::EventBus& eventBus)
        : m_logService{logService},
          m_eventBus{eventBus}
    {
    }

    StatusCode ScriptLogger::register_api(asIScriptEngine& engine)
    {
        if (engine.RegisterGlobalFunction("void log_info(const string &in)", asMETHOD(ScriptLogger, log_info), asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction("void log_warn(const string &in)", asMETHOD(ScriptLogger, log_warn), asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction("void log_error(const string &in)", asMETHOD(ScriptLogger, log_error), asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    void ScriptLogger::log_info(const std::string& msg) const
    {
        std::ignore = m_logService.get().log_info(msg);
        m_eventBus.get().broadcast(Event::ScriptOutputEvent{Event::ScriptOutputSeverity::Info, msg});
    }

    void ScriptLogger::log_warn(const std::string& msg) const
    {
        std::ignore = m_logService.get().log_warn(msg);
        m_eventBus.get().broadcast(Event::ScriptOutputEvent{Event::ScriptOutputSeverity::Warning, msg});
    }

    void ScriptLogger::log_error(const std::string& msg) const
    {
        std::ignore = m_logService.get().log_error(msg);
        m_eventBus.get().broadcast(Event::ScriptOutputEvent{Event::ScriptOutputSeverity::Error, msg});
    }
} // namespace Vertex::Scripting::Stdlib
