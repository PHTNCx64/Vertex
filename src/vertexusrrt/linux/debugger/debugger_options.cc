//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/linux/debugger_options.hh>
#include <vertexusrrt/linux/lldb_backend.hh>
#include <sdk/api.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

extern Runtime* g_pluginRuntime;

namespace
{
    constexpr auto DEBUGGER_OPTIONS_PANEL_ID = "debugger_options";
    constexpr auto CONTINUE_ON_WP_HIT_FIELD_ID = "continue_on_watchpoint_hit";
    constexpr std::size_t FIELD_COUNT = 1;

    template <std::size_t N>
    void fill_c_string(char (&dest)[N], std::string_view src)
    {
        const auto count = std::min(src.size(), N - 1);
        std::copy_n(src.data(), count, dest);
        dest[count] = '\0';
    }

    void VERTEX_API on_debugger_options_apply(const char* fieldId, const UIValue* value, [[maybe_unused]] void* userData)
    {
        if (!fieldId || !value)
        {
            return;
        }

        const std::string_view field{fieldId};

        if (field == CONTINUE_ON_WP_HIT_FIELD_ID)
        {
            auto& state = Debugger::get_backend_state();
            state.continueOnWatchpointHit.store(value->boolValue != 0, std::memory_order_relaxed);

            if (g_pluginRuntime)
            {
                g_pluginRuntime->vertex_log_info("Debugger: continue on watchpoint hit %s",
                    value->boolValue ? "enabled" : "disabled");
            }
        }
    }

    void VERTEX_API on_debugger_options_reset([[maybe_unused]] void* userData)
    {
        auto& state = Debugger::get_backend_state();
        state.continueOnWatchpointHit.store(false, std::memory_order_relaxed);
    }
}

namespace debugger_options
{
    void register_ui_panel()
    {
        if (!g_pluginRuntime || !g_pluginRuntime->vertex_register_ui_panel)
        {
            return;
        }

        static std::array<UIField, FIELD_COUNT> fields{};

        auto& continueOnWpField = fields[0];
        fill_c_string(continueOnWpField.fieldId, CONTINUE_ON_WP_HIT_FIELD_ID);
        fill_c_string(continueOnWpField.label, "Continue on Watchpoint Hit");
        fill_c_string(continueOnWpField.tooltip, "When enabled, the debugger will automatically continue execution after a watchpoint is hit instead of pausing.");
        continueOnWpField.type = VERTEX_UI_FIELD_CHECKBOX;
        continueOnWpField.defaultValue.boolValue = 0;
        continueOnWpField.layoutOrientation = VERTEX_UI_LAYOUT_VERTICAL;
        continueOnWpField.layoutBorder = 5;
        continueOnWpField.layoutProportion = 0;

        static UISection section{};
        fill_c_string(section.title, "Watchpoints");
        section.fields = fields.data();
        section.fieldCount = FIELD_COUNT;

        static UIPanel panel{};
        fill_c_string(panel.panelId, DEBUGGER_OPTIONS_PANEL_ID);
        fill_c_string(panel.title, "Debugger");
        panel.sections = &section;
        panel.sectionCount = 1;
        panel.onApply = on_debugger_options_apply;
        panel.onReset = on_debugger_options_reset;
        panel.userData = nullptr;

        g_pluginRuntime->vertex_register_ui_panel(&panel);
    }
}
