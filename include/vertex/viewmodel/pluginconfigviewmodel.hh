//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <vertex/event/eventbus.hh>
#include <vertex/model/pluginconfigmodel.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>

#include <sdk/ui.h>
#include <vertex/runtime/iuiregistry.hh>

namespace Vertex::ViewModel
{
    class PluginConfigViewModel final
    {
    public:
        PluginConfigViewModel(
            std::unique_ptr<Model::PluginConfigModel> model,
            Event::EventBus& eventBus,
            Log::ILog& logService
        );

        ~PluginConfigViewModel();

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback);

        [[nodiscard]] std::vector<Runtime::PanelSnapshot> get_panels() const;
        [[nodiscard]] bool has_panels() const;

        [[nodiscard]] std::optional<UIValue> get_field_value(std::string_view panelId, std::string_view fieldId) const;

        [[nodiscard]] StatusCode apply_field(std::string_view panelId, std::string_view fieldId, const UIValue& value) const;
        [[nodiscard]] StatusCode apply_all(std::string_view panelId);
        [[nodiscard]] StatusCode reset_panel(std::string_view panelId);

        [[nodiscard]] StatusCode persist(std::string_view panelId) const;
        [[nodiscard]] StatusCode load_persisted(std::string_view panelId) const;

        void set_pending_value(std::string_view panelId, std::string_view fieldId, const UIValue& value);
        void clear_pending_values();
        [[nodiscard]] bool has_pending_changes() const;

    private:
        void subscribe_to_events() const;
        void unsubscribe_from_events() const;

        std::unique_ptr<Model::PluginConfigModel> m_model{};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback{};

        Event::EventBus& m_eventBus;
        Log::ILog& m_logService;

        std::unordered_map<std::string, std::unordered_map<std::string, UIValue>> m_pendingValues{};
    };
}
