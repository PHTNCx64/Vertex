//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/pluginconfigviewmodel.hh>

#include <vertex/event/types/settingsevent.hh>
#include <fmt/format.h>
#include <ranges>

namespace Vertex::ViewModel
{
    PluginConfigViewModel::PluginConfigViewModel(std::unique_ptr<Model::PluginConfigModel> model, Event::EventBus& eventBus, Log::ILog& logService)
        : m_model{std::move(model)},
          m_eventBus{eventBus},
          m_logService{logService}
    {
        subscribe_to_events();
    }

    PluginConfigViewModel::~PluginConfigViewModel()
    {
        unsubscribe_from_events();
    }

    void PluginConfigViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback)
    {
        m_eventCallback = std::move(eventCallback);
    }

    std::vector<Runtime::PanelSnapshot> PluginConfigViewModel::get_panels() const
    {
        return m_model->get_panels();
    }

    bool PluginConfigViewModel::has_panels() const
    {
        return m_model->has_panels();
    }

    std::optional<UIValue> PluginConfigViewModel::get_field_value(const std::string_view panelId, const std::string_view fieldId) const
    {
        const std::string panelKey{panelId};
        const std::string fieldKey{fieldId};

        const auto panelIt = m_pendingValues.find(panelKey);
        if (panelIt != m_pendingValues.end())
        {
            const auto fieldIt = panelIt->second.find(fieldKey);
            if (fieldIt != panelIt->second.end())
            {
                return fieldIt->second;
            }
        }

        return m_model->get_field_value(panelId, fieldId);
    }

    StatusCode PluginConfigViewModel::apply_field(const std::string_view panelId, const std::string_view fieldId, const UIValue& value) const
    {
        return m_model->apply_field(panelId, fieldId, value);
    }

    StatusCode PluginConfigViewModel::apply_all(const std::string_view panelId)
    {
        const std::string panelKey{panelId};
        const auto panelIt = m_pendingValues.find(panelKey);
        if (panelIt == m_pendingValues.end())
        {
            return StatusCode::STATUS_OK;
        }

        for (const auto& [fieldId, value] : panelIt->second)
        {
            const auto result = m_model->apply_field(panelId, fieldId, value);
            if (result != StatusCode::STATUS_OK)
            {
                m_logService.log_warn(fmt::format("{}: Failed to apply field '{}'", ViewModelName::PLUGINCONFIG, fieldId));
            }
        }

        m_pendingValues.erase(panelIt);

        const auto persistResult = m_model->persist_values(panelId);
        if (persistResult != StatusCode::STATUS_OK)
        {
            m_logService.log_warn(fmt::format("{}: Failed to persist values for panel '{}'", ViewModelName::PLUGINCONFIG, panelId));
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PluginConfigViewModel::reset_panel(const std::string_view panelId)
    {
        m_pendingValues.erase(std::string{panelId});
        return m_model->reset_panel(panelId);
    }

    StatusCode PluginConfigViewModel::persist(const std::string_view panelId) const
    {
        return m_model->persist_values(panelId);
    }

    StatusCode PluginConfigViewModel::load_persisted(const std::string_view panelId) const
    {
        return m_model->load_persisted_values(panelId);
    }

    void PluginConfigViewModel::set_pending_value(const std::string_view panelId, const std::string_view fieldId, const UIValue& value)
    {
        m_pendingValues[std::string{panelId}][std::string{fieldId}] = value;
    }

    void PluginConfigViewModel::clear_pending_values()
    {
        m_pendingValues.clear();
    }

    bool PluginConfigViewModel::has_pending_changes() const
    {
        return !m_pendingValues.empty();
    }

    void PluginConfigViewModel::subscribe_to_events() const
    {
        m_eventBus.subscribe<Event::SettingsEvent>(ViewModelName::PLUGINCONFIG, Event::SETTINGS_CHANGED_EVENT,
                                                   [this](const Event::SettingsEvent& event)
                                                   {
                                                       if (m_eventCallback)
                                                       {
                                                           m_eventCallback(Event::SETTINGS_CHANGED_EVENT, event);
                                                       }
                                                   });
    }

    void PluginConfigViewModel::unsubscribe_from_events() const
    {
        m_eventBus.unsubscribe(ViewModelName::PLUGINCONFIG, Event::SETTINGS_CHANGED_EVENT);
    }
}
