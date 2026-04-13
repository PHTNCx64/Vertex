//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/scriptingmodel.hh>

#include <vertex/configuration/filesystem.hh>

#include <fmt/format.h>

#include <filesystem>
#include <fstream>

namespace
{
    constexpr std::string_view MODEL_NAME{"ScriptingModel"};
    constexpr std::uintmax_t MAX_SCRIPT_FILE_SIZE{1024 * 1024};
    constexpr std::string_view RECENT_SCRIPTS_KEY{"scripting.recent_scripts"};
}

namespace Vertex::Model
{
    std::expected<Scripting::ContextId, StatusCode> ScriptingModel::execute_script(const std::string_view code)
    {
        if (code.empty())
        {
            return std::unexpected{StatusCode::STATUS_ERROR_INVALID_PARAMETER};
        }

        const auto tempDir = std::filesystem::temp_directory_path();
        const auto moduleName = fmt::format("__editor_{}", m_executeCounter++);
        const auto tempPath = tempDir / (moduleName + ".as");

        {
            std::ofstream file{tempPath, std::ios::out | std::ios::trunc};
            if (!file)
            {
                m_logService.log_error(fmt::format("{}: failed to create temp file {}", MODEL_NAME, tempPath.string()));
                return std::unexpected{StatusCode::STATUS_ERROR_FILE_CREATION_FAILED};
            }
            file << code;
        }

        auto result = m_scripting.create_context(moduleName, tempPath, Scripting::UseActive{});

        std::filesystem::remove(tempPath);

        if (!result.has_value())
        {
            m_logService.log_error(fmt::format("{}: failed to create script context (status={})", MODEL_NAME, static_cast<int>(result.error())));
        }

        return result;
    }

    StatusCode ScriptingModel::save_script(const std::filesystem::path& path, const std::string_view code) const
    {
        std::ofstream file{path, std::ios::out | std::ios::trunc};
        if (!file)
        {
            m_logService.log_error(fmt::format("{}: failed to save script to {}", MODEL_NAME, path.string()));
            return StatusCode::STATUS_ERROR_FS_FILE_COULD_NOT_BE_SAVED;
        }

        file << code;
        return StatusCode::STATUS_OK;
    }

    std::expected<std::string, StatusCode> ScriptingModel::load_script(const std::filesystem::path& path) const
    {
        std::error_code ec{};
        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec)
        {
            m_logService.log_error(fmt::format("{}: failed to get script file size for {} ({})",
                MODEL_NAME, path.string(), ec.message()));
            return std::unexpected{StatusCode::STATUS_ERROR_FILE_NOT_FOUND};
        }

        if (fileSize > MAX_SCRIPT_FILE_SIZE)
        {
            m_logService.log_error(fmt::format("{}: script file too large {} ({} bytes)",
                MODEL_NAME, path.string(), fileSize));
            return std::unexpected{StatusCode::STATUS_ERROR_FS_FILE_TOO_LARGE};
        }

        std::ifstream file{path, std::ios::in};
        if (!file)
        {
            m_logService.log_error(fmt::format("{}: failed to open script file {}", MODEL_NAME, path.string()));
            return std::unexpected{StatusCode::STATUS_ERROR_FS_FILE_OPEN_FAILED};
        }

        std::string content{};
        content.reserve(static_cast<std::size_t>(fileSize));
        content.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

        if (file.bad())
        {
            m_logService.log_error(fmt::format("{}: failed to read script file {}", MODEL_NAME, path.string()));
            return std::unexpected{StatusCode::STATUS_ERROR_FS_FILE_READ_FAILED};
        }

        return content;
    }

    StatusCode ScriptingModel::suspend_context(const Scripting::ContextId id)
    {
        const auto status = m_scripting.suspend_context(id);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("{}: failed to suspend context {} (status={})",
                MODEL_NAME, id, static_cast<int>(status)));
        }

        return status;
    }

    StatusCode ScriptingModel::resume_context(const Scripting::ContextId id)
    {
        const auto status = m_scripting.resume_context(id);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("{}: failed to resume context {} (status={})",
                MODEL_NAME, id, static_cast<int>(status)));
        }

        return status;
    }

    StatusCode ScriptingModel::remove_context(const Scripting::ContextId id)
    {
        const auto status = m_scripting.remove_context(id);
        if (status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("{}: failed to remove context {} (status={})",
                MODEL_NAME, id, static_cast<int>(status)));
        }

        return status;
    }

    StatusCode ScriptingModel::set_context_breakpoint(const Scripting::ContextId id, const int line)
    {
        const auto status = m_scripting.set_breakpoint(id, line);
        if (status != StatusCode::STATUS_OK &&
            status != StatusCode::STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS)
        {
            m_logService.log_error(fmt::format("{}: failed to set breakpoint for context {} at line {} (status={})",
                MODEL_NAME, id, line, static_cast<int>(status)));
        }

        return status;
    }

    StatusCode ScriptingModel::remove_context_breakpoint(const Scripting::ContextId id, const int line)
    {
        const auto status = m_scripting.remove_breakpoint(id, line);
        if (status != StatusCode::STATUS_OK &&
            status != StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND)
        {
            m_logService.log_error(fmt::format("{}: failed to remove breakpoint for context {} at line {} (status={})",
                MODEL_NAME, id, line, static_cast<int>(status)));
        }

        return status;
    }

    std::vector<Scripting::ContextInfo> ScriptingModel::get_context_list() const
    {
        return m_scripting.get_context_list();
    }

    std::expected<std::vector<Scripting::ContextVariable>, StatusCode> ScriptingModel::get_context_variables(
        const Scripting::ContextId id) const
    {
        auto variables = m_scripting.get_context_variables(id);
        if (!variables.has_value() &&
            variables.error() != StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE &&
            variables.error() != StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND)
        {
            m_logService.log_error(fmt::format("{}: failed to fetch variables for context {} (status={})",
                MODEL_NAME, id, static_cast<int>(variables.error())));
        }

        return variables;
    }

    std::filesystem::path ScriptingModel::get_default_script_directory() const
    {
        return Configuration::Filesystem::get_script_path();
    }

    std::vector<std::filesystem::path> ScriptingModel::get_recent_scripts() const
    {
        std::vector<std::filesystem::path> recentScripts{};

        const auto value = m_settingsService.get_value(std::string{RECENT_SCRIPTS_KEY});
        if (!value.is_array())
        {
            return recentScripts;
        }

        for (const auto& entry : value)
        {
            if (!entry.is_string())
            {
                continue;
            }

            const auto entryPath = std::filesystem::path{entry.get<std::string>()};
            if (entryPath.empty())
            {
                continue;
            }

            recentScripts.push_back(entryPath);
        }

        return recentScripts;
    }

    void ScriptingModel::set_recent_scripts(const std::vector<std::filesystem::path>& paths) const
    {
        nlohmann::json serializedPaths = nlohmann::json::array();

        for (const auto& path : paths)
        {
            if (path.empty())
            {
                continue;
            }

            serializedPaths.push_back(path.string());
        }

        m_settingsService.set_value(std::string{RECENT_SCRIPTS_KEY}, serializedPaths);
    }
}
