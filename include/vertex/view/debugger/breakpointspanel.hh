//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>

#include <functional>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class BreakpointsPanel final : public wxPanel
    {
    public:
        using GotoBreakpointCallback = std::function<void(std::uint64_t address)>;
        using RemoveBreakpointCallback = std::function<void(std::uint32_t id)>;
        using EnableBreakpointCallback = std::function<void(std::uint32_t id, bool enable)>;

        BreakpointsPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_breakpoints(const std::vector<::Vertex::Debugger::Breakpoint>& breakpoints);

        void set_goto_callback(GotoBreakpointCallback callback);
        void set_remove_callback(RemoveBreakpointCallback callback);
        void set_enable_callback(EnableBreakpointCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_item_activated(const wxListEvent& event);
        void on_item_right_click(const wxListEvent& event);

        wxListCtrl* m_breakpointList{};
        wxBoxSizer* m_mainSizer{};

        std::vector<::Vertex::Debugger::Breakpoint> m_breakpoints{};

        GotoBreakpointCallback m_gotoCallback{};
        RemoveBreakpointCallback m_removeCallback{};
        EnableBreakpointCallback m_enableCallback{};

        Language::ILanguage& m_languageService;
    };
}
