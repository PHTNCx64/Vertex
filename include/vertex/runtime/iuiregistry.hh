//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/ui.h>
#include <sdk/statuscode.h>

#include <string_view>
#include <vector>
#include <optional>

namespace Vertex::Runtime
{
    struct PanelSnapshot
    {
        UIPanel panel{};
        std::vector<UIOption> options{};
        std::vector<UIField> fields{};
        std::vector<UISection> sections{};

        PanelSnapshot() = default;
        PanelSnapshot(PanelSnapshot&&) noexcept = default;
        PanelSnapshot& operator=(PanelSnapshot&&) noexcept = default;
        PanelSnapshot(const PanelSnapshot&) = delete;
        PanelSnapshot& operator=(const PanelSnapshot&) = delete;
    };

    class IUIRegistry
    {
    public:
        virtual ~IUIRegistry() = default;

        virtual StatusCode register_panel(const UIPanel& panel) = 0;
        [[nodiscard]] virtual std::vector<PanelSnapshot> get_panels() const = 0;
        [[nodiscard]] virtual std::optional<PanelSnapshot> get_panel(std::string_view panelId) const = 0;

        virtual StatusCode set_value(std::string_view panelId, std::string_view fieldId, const UIValue& value) = 0;
        [[nodiscard]] virtual std::optional<UIValue> get_value(std::string_view panelId, std::string_view fieldId) const = 0;

        virtual void clear() = 0;
        [[nodiscard]] virtual bool has_panels() const = 0;
    };
}
