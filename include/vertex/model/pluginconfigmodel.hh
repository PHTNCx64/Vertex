//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/iuiregistry.hh>
#include <vertex/configuration/ipluginconfig.hh>
#include <vertex/log/ilog.hh>

#include <sdk/statuscode.h>
#include <sdk/ui.h>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Vertex::Model
{
    class PluginConfigModel final
    {
    public:
        explicit PluginConfigModel(
            Runtime::IUIRegistry& uiRegistry,
            Configuration::IPluginConfig& pluginConfigService,
            Log::ILog& loggerService
        );

        [[nodiscard]] std::vector<Runtime::PanelSnapshot> get_panels() const;
        [[nodiscard]] bool has_panels() const;

        [[nodiscard]] std::optional<UIValue> get_field_value(std::string_view panelId, std::string_view fieldId) const;
        [[nodiscard]] StatusCode set_field_value(std::string_view panelId, std::string_view fieldId, const UIValue& value) const;

        [[nodiscard]] StatusCode apply_field(std::string_view panelId, std::string_view fieldId, const UIValue& value) const;
        [[nodiscard]] StatusCode reset_panel(std::string_view panelId) const;

        [[nodiscard]] StatusCode persist_values(std::string_view panelId) const;
        [[nodiscard]] StatusCode load_persisted_values(std::string_view panelId) const;

    private:
        Runtime::IUIRegistry& m_uiRegistry;
        Configuration::IPluginConfig& m_pluginConfigService;
        Log::ILog& m_loggerService;
    };
}
