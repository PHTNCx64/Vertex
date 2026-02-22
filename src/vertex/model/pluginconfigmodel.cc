//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/pluginconfigmodel.hh>

#include <fmt/format.h>
#include <span>

namespace Vertex::Model
{
    static constexpr std::string_view MODEL_NAME{"PluginConfigModel"};

    PluginConfigModel::PluginConfigModel(
        Runtime::IUIRegistry& uiRegistry,
        Configuration::IPluginConfig& pluginConfigService,
        Log::ILog& loggerService)
        : m_uiRegistry{uiRegistry},
          m_pluginConfigService{pluginConfigService},
          m_loggerService{loggerService}
    {
    }

    std::vector<Runtime::PanelSnapshot> PluginConfigModel::get_panels() const
    {
        return m_uiRegistry.get_panels();
    }

    bool PluginConfigModel::has_panels() const
    {
        return m_uiRegistry.has_panels();
    }

    std::optional<UIValue> PluginConfigModel::get_field_value(const std::string_view panelId, const std::string_view fieldId) const
    {
        return m_uiRegistry.get_value(panelId, fieldId);
    }

    StatusCode PluginConfigModel::set_field_value(const std::string_view panelId, const std::string_view fieldId, const UIValue& value) const
    {
        return m_uiRegistry.set_value(panelId, fieldId, value);
    }

    StatusCode PluginConfigModel::apply_field(const std::string_view panelId, const std::string_view fieldId, const UIValue& value) const
    {
        auto snapshot = m_uiRegistry.get_panel(panelId);
        if (!snapshot.has_value())
        {
            m_loggerService.log_warn(fmt::format("{}: Panel '{}' not found for apply", MODEL_NAME, panelId));
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        auto result = m_uiRegistry.set_value(panelId, fieldId, value);
        if (result != StatusCode::STATUS_OK)
        {
            return result;
        }

        if (snapshot->panel.onApply)
        {
            snapshot->panel.onApply(std::string{fieldId}.c_str(), &value, snapshot->panel.userData);
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PluginConfigModel::reset_panel(const std::string_view panelId) const
    {
        auto snapshot = m_uiRegistry.get_panel(panelId);
        if (!snapshot.has_value())
        {
            m_loggerService.log_warn(fmt::format("{}: Panel '{}' not found for reset", MODEL_NAME, panelId));
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        for (const auto& section : std::span{snapshot->panel.sections, snapshot->panel.sectionCount})
        {
            for (const auto& field : std::span{section.fields, section.fieldCount})
            {
                m_uiRegistry.set_value(panelId, field.fieldId, field.defaultValue);
            }
        }

        if (snapshot->panel.onReset)
        {
            snapshot->panel.onReset(snapshot->panel.userData);
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode PluginConfigModel::persist_values(const std::string_view panelId) const
    {
        auto snapshot = m_uiRegistry.get_panel(panelId);
        if (!snapshot.has_value())
        {
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        const std::string panelKey{panelId};

        for (const auto& section : std::span{snapshot->panel.sections, snapshot->panel.sectionCount})
        {
            for (const auto& field : std::span{section.fields, section.fieldCount})
            {
                auto value = m_uiRegistry.get_value(panelId, field.fieldId);
                if (value.has_value())
                {
                    m_pluginConfigService.set_ui_value(panelKey, field.fieldId, *value, field.type);
                }
            }
        }

        return m_pluginConfigService.save_config();
    }

    StatusCode PluginConfigModel::load_persisted_values(const std::string_view panelId) const
    {
        auto snapshot = m_uiRegistry.get_panel(panelId);
        if (!snapshot.has_value())
        {
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        const std::string panelKey{panelId};

        for (const auto& section : std::span{snapshot->panel.sections, snapshot->panel.sectionCount})
        {
            for (const auto& field : std::span{section.fields, section.fieldCount})
            {
                auto persisted = m_pluginConfigService.get_ui_value(panelKey, field.fieldId, field.type);
                if (persisted.has_value())
                {
                    m_uiRegistry.set_value(panelId, field.fieldId, *persisted);
                }
            }
        }

        return StatusCode::STATUS_OK;
    }
}
