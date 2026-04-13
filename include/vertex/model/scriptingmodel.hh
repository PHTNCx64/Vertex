//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scripting/iangelscript.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/log/ilog.hh>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <sdk/statuscode.h>

namespace Vertex::Model
{
    class ScriptingModel final
    {
    public:
        explicit ScriptingModel(Scripting::IAngelScript& scripting,
                                Configuration::ISettings& settingsService,
                                Log::ILog& logService)
            : m_scripting{scripting},
              m_settingsService{settingsService},
              m_logService{logService}
        {
        }

        [[nodiscard]] std::expected<Scripting::ContextId, StatusCode> execute_script(std::string_view code);
        [[nodiscard]] std::expected<std::string, StatusCode> load_script(const std::filesystem::path& path) const;
        [[nodiscard]] StatusCode save_script(const std::filesystem::path& path, std::string_view code) const;
        [[nodiscard]] StatusCode suspend_context(Scripting::ContextId id);
        [[nodiscard]] StatusCode resume_context(Scripting::ContextId id);
        [[nodiscard]] StatusCode remove_context(Scripting::ContextId id);
        [[nodiscard]] StatusCode set_context_breakpoint(Scripting::ContextId id, int line);
        [[nodiscard]] StatusCode remove_context_breakpoint(Scripting::ContextId id, int line);
        [[nodiscard]] std::vector<Scripting::ContextInfo> get_context_list() const;
        [[nodiscard]] std::expected<std::vector<Scripting::ContextVariable>, StatusCode> get_context_variables(Scripting::ContextId id) const;
        [[nodiscard]] std::filesystem::path get_default_script_directory() const;
        [[nodiscard]] std::vector<std::filesystem::path> get_recent_scripts() const;
        void set_recent_scripts(const std::vector<std::filesystem::path>& paths) const;

    private:
        Scripting::IAngelScript& m_scripting;
        Configuration::ISettings& m_settingsService;
        Log::ILog& m_logService;
        std::uint32_t m_executeCounter{};
    };
}
