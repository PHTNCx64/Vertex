//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/scriptingviewmodel.hh>

#include <utility>

#include <vertex/event/types/viewevent.hh>
#include <vertex/event/types/scriptoutputevent.hh>
#include <vertex/event/types/scriptdiagnosticevent.hh>

namespace Vertex::ViewModel
{
    ScriptingViewModel::ScriptingViewModel(std::unique_ptr<Model::ScriptingModel> model, Event::EventBus& eventBus, Log::ILog& logService, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_logService{logService}
    {
        subscribe_to_events();
    }

    ScriptingViewModel::~ScriptingViewModel()
    {
        unsubscribe_from_events();
    }

    void ScriptingViewModel::subscribe_to_events() const
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   if (m_eventCallback)
                                                   {
                                                       m_eventCallback(Event::VIEW_EVENT, event);
                                                   }
                                               });

        m_eventBus.subscribe<Event::ScriptOutputEvent>(m_viewModelName, Event::SCRIPT_OUTPUT_EVENT,
                                                       [this](const Event::ScriptOutputEvent& event)
                                                       {
                                                           if (m_eventCallback)
                                                           {
                                                               m_eventCallback(Event::SCRIPT_OUTPUT_EVENT, event);
                                                           }
                                                       });

        m_eventBus.subscribe<Event::ScriptDiagnosticEvent>(m_viewModelName, Event::SCRIPT_DIAGNOSTIC_EVENT,
                                                           [this](const Event::ScriptDiagnosticEvent& event)
                                                           {
                                                               if (m_eventCallback)
                                                               {
                                                                   m_eventCallback(Event::SCRIPT_DIAGNOSTIC_EVENT, event);
                                                               }
                                                           });
    }

    void ScriptingViewModel::unsubscribe_from_events() const
    {
        m_eventBus.unsubscribe(m_viewModelName, Event::VIEW_EVENT);
        m_eventBus.unsubscribe(m_viewModelName, Event::SCRIPT_OUTPUT_EVENT);
        m_eventBus.unsubscribe(m_viewModelName, Event::SCRIPT_DIAGNOSTIC_EVENT);
    }

    void ScriptingViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback)
    {
        m_eventCallback = std::move(eventCallback);
    }

    std::expected<Scripting::ContextId, StatusCode> ScriptingViewModel::execute_script(const std::string_view code)
    {
        return m_model->execute_script(code);
    }

    std::expected<std::string, StatusCode> ScriptingViewModel::load_script(const std::filesystem::path& path) const
    {
        return m_model->load_script(path);
    }

    StatusCode ScriptingViewModel::save_script(const std::filesystem::path& path, const std::string_view code) const
    {
        return m_model->save_script(path, code);
    }

    StatusCode ScriptingViewModel::suspend_context(const Scripting::ContextId id)
    {
        return m_model->suspend_context(id);
    }

    StatusCode ScriptingViewModel::resume_context(const Scripting::ContextId id)
    {
        return m_model->resume_context(id);
    }

    StatusCode ScriptingViewModel::remove_context(const Scripting::ContextId id)
    {
        return m_model->remove_context(id);
    }

    StatusCode ScriptingViewModel::set_context_breakpoint(const Scripting::ContextId id, const int line)
    {
        return m_model->set_context_breakpoint(id, line);
    }

    StatusCode ScriptingViewModel::remove_context_breakpoint(const Scripting::ContextId id, const int line)
    {
        return m_model->remove_context_breakpoint(id, line);
    }

    std::vector<Scripting::ContextInfo> ScriptingViewModel::get_context_list() const
    {
        return m_model->get_context_list();
    }

    std::expected<std::vector<Scripting::ContextVariable>, StatusCode> ScriptingViewModel::get_context_variables(
        const Scripting::ContextId id) const
    {
        return m_model->get_context_variables(id);
    }

    std::filesystem::path ScriptingViewModel::get_default_script_directory() const
    {
        return m_model->get_default_script_directory();
    }

    std::vector<std::filesystem::path> ScriptingViewModel::get_recent_scripts() const
    {
        return m_model->get_recent_scripts();
    }

    void ScriptingViewModel::set_recent_scripts(const std::vector<std::filesystem::path>& paths) const
    {
        m_model->set_recent_scripts(paths);
    }
}
