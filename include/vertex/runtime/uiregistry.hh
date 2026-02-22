//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/iuiregistry.hh>

#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

namespace Vertex::Runtime
{
    class UIRegistry final : public IUIRegistry
    {
    public:
        UIRegistry() = default;
        ~UIRegistry() override = default;

        StatusCode register_panel(const UIPanel& panel) override;
        [[nodiscard]] std::vector<PanelSnapshot> get_panels() const override;
        [[nodiscard]] std::optional<PanelSnapshot> get_panel(std::string_view panelId) const override;

        StatusCode set_value(std::string_view panelId, std::string_view fieldId, const UIValue& value) override;
        [[nodiscard]] std::optional<UIValue> get_value(std::string_view panelId, std::string_view fieldId) const override;

        void clear() override;
        [[nodiscard]] bool has_panels() const override;

    private:
        struct OwnedField final
        {
            UIField header{};
            std::vector<UIOption> options{};
        };

        struct OwnedSection final
        {
            char title[VERTEX_UI_MAX_SECTION_TITLE_LENGTH]{};
            std::vector<OwnedField> fields{};
        };

        struct OwnedPanel final
        {
            char panelId[VERTEX_UI_MAX_PANEL_ID_LENGTH]{};
            char title[VERTEX_UI_MAX_PANEL_TITLE_LENGTH]{};
            std::vector<OwnedSection> sections{};
            VertexOnUIApply_t onApply{};
            VertexOnUIReset_t onReset{};
            void* userData{};
        };

        [[nodiscard]] static OwnedPanel copy_panel(const UIPanel& panel);
        [[nodiscard]] static PanelSnapshot build_snapshot(const OwnedPanel& owned);

        mutable std::mutex m_mutex{};
        std::unordered_map<std::string, OwnedPanel> m_panels{};
        std::unordered_map<std::string, std::unordered_map<std::string, UIValue>> m_values{};
    };
}
