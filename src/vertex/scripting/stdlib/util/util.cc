//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/util.hh>

#include <angelscript.h>

namespace Vertex::Scripting::Stdlib
{
    namespace
    {
        StatusCode register_status_codes_enum(asIScriptEngine& engine)
        {
            if (engine.RegisterEnum("StatusCode") < 0)
            {
                return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
            }

            const auto reg = [&engine](const char* name, const StatusCode code)
            {
                return engine.RegisterEnumValue("StatusCode", name, code) >= 0;
            };

            if (!reg("STATUS_OK", StatusCode::STATUS_OK) ||
                !reg("STATUS_ERROR_GENERAL", StatusCode::STATUS_ERROR_GENERAL) ||
                !reg("STATUS_ERROR_INVALID_PARAMETER", StatusCode::STATUS_ERROR_INVALID_PARAMETER) ||
                !reg("STATUS_ERROR_NOT_IMPLEMENTED", StatusCode::STATUS_ERROR_NOT_IMPLEMENTED) ||
                !reg("STATUS_ERROR_FUNCTION_NOT_FOUND", StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND) ||
                !reg("STATUS_ERROR_PLUGIN_NOT_ACTIVE", StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE) ||
                !reg("STATUS_ERROR_PLUGIN_NOT_LOADED", StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED) ||
                !reg("STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED", StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED) ||
                !reg("STATUS_ERROR_PROCESS_ACCESS_DENIED", StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED) ||
                !reg("STATUS_ERROR_PROCESS_INVALID", StatusCode::STATUS_ERROR_PROCESS_INVALID) ||
                !reg("STATUS_ERROR_PROCESS_NOT_FOUND", StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND) ||
                !reg("STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL", StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL) ||
                !reg("STATUS_ERROR_MEMORY_OPERATION_ABORTED", StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED) ||
                !reg("STATUS_ERROR_MEMORY_READ", StatusCode::STATUS_ERROR_MEMORY_READ) ||
                !reg("STATUS_ERROR_MEMORY_WRITE", StatusCode::STATUS_ERROR_MEMORY_WRITE) ||
                !reg("STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED", StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED))
            {
                return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
            }

            return StatusCode::STATUS_OK;
        }
    }

    ScriptUtility::ScriptUtility(Runtime::ILoader& loader)
        : m_loader{loader}
    {
    }

    StatusCode ScriptUtility::register_api(asIScriptEngine& engine)
    {
        if (const auto status = register_status_codes_enum(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (engine.RegisterGlobalFunction(
                "string get_current_plugin()",
                asMETHOD(ScriptUtility, get_current_plugin),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "string get_host_os()",
                asMETHOD(ScriptUtility, get_host_os),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "string get_version()",
                asMETHOD(ScriptUtility, get_version),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "string get_vendor()",
                asMETHOD(ScriptUtility, get_vendor),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "string get_name()",
                asMETHOD(ScriptUtility, get_name),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    std::string ScriptUtility::get_current_plugin() const
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return {};
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef)
        {
            return {};
        }

        const auto& info = pluginRef->get().get_plugin_info();
        return info.pluginName ? info.pluginName : std::string{};
    }

    std::string ScriptUtility::get_host_os() const
    {
        // TODO: add a util header that has a consteval function that returns the actual platform, looks better than macros
#if defined(_WIN32) || defined(_WIN64)
        return "Windows";
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
        return "Linux";
#elif defined(__APPLE__) || defined(__MACH__)
        return "macOS";
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
        return "BSD";
#else
        return "Unknown";
#endif
    }

    std::string ScriptUtility::get_version() const
    {
        return ApplicationVersion;
    }

    std::string ScriptUtility::get_vendor() const
    {
        return ApplicationVendor;
    }

    std::string ScriptUtility::get_name() const
    {
        return ApplicationName;
    }
}
