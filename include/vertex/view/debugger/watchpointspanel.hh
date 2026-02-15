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
    class WatchpointsPanel final : public wxPanel
    {
    public:
        using GotoWatchpointCallback = std::function<void(std::uint64_t address)>;
        using GotoAccessorCallback = std::function<void(std::uint64_t address)>;
        using RemoveWatchpointCallback = std::function<void(std::uint32_t id)>;
        using EnableWatchpointCallback = std::function<void(std::uint32_t id, bool enable)>;

        WatchpointsPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_watchpoints(const std::vector<::Vertex::Debugger::Watchpoint>& watchpoints);
        void add_watchpoint(const ::Vertex::Debugger::Watchpoint& watchpoint);

        void set_goto_callback(GotoWatchpointCallback callback);
        void set_goto_accessor_callback(GotoAccessorCallback callback);
        void set_remove_callback(RemoveWatchpointCallback callback);
        void set_enable_callback(EnableWatchpointCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_item_activated(wxListEvent& event);
        void on_item_right_click(wxListEvent& event);

        wxListCtrl* m_watchpointList{};
        wxBoxSizer* m_mainSizer{};

        std::vector<::Vertex::Debugger::Watchpoint> m_watchpoints{};

        GotoWatchpointCallback m_gotoCallback{};
        GotoAccessorCallback m_gotoAccessorCallback{};
        RemoveWatchpointCallback m_removeCallback{};
        EnableWatchpointCallback m_enableCallback{};

        Language::ILanguage& m_languageService;
    };
}
