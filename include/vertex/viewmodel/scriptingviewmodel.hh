//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <expected>
#include <filesystem>

#include <vertex/event/eventbus.hh>
#include <vertex/model/scriptingmodel.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>

#include <sdk/statuscode.h>

namespace Vertex::ViewModel
{
    class ScriptingViewModel final
    {
    public:
        ScriptingViewModel(
            std::unique_ptr<Model::ScriptingModel> model,
            Event::EventBus& eventBus,
            Log::ILog& logService,
            std::string name = ViewModelName::SCRIPTING
        );

        ~ScriptingViewModel();

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback);

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
        void subscribe_to_events() const;
        void unsubscribe_from_events() const;

        std::string m_viewModelName{};
        std::unique_ptr<Model::ScriptingModel> m_model{};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback{};

        Event::EventBus& m_eventBus;
        Log::ILog& m_logService;
    };
}
