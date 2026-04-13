//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/treectrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/splitter.h>

#include <functional>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class WatchPanel final : public wxPanel
    {
    public:
        using AddWatchCallback = std::function<void(const std::string& expression)>;
        using RemoveWatchCallback = std::function<void(std::uint32_t id)>;
        using ModifyWatchCallback = std::function<void(std::uint32_t id, const std::string& newValue)>;
        using ExpandWatchCallback = std::function<void(std::uint32_t id, bool expand)>;
        using ShowInMemoryCallback = std::function<void(std::uint64_t address)>;

        WatchPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_watches(const std::vector<::Vertex::Debugger::WatchVariable>& watches);
        void update_locals(const std::vector<::Vertex::Debugger::LocalVariable>& locals);

        void set_add_watch_callback(AddWatchCallback callback);
        void set_remove_watch_callback(RemoveWatchCallback callback);
        void set_modify_watch_callback(ModifyWatchCallback callback);
        void set_expand_watch_callback(ExpandWatchCallback callback);
        void set_show_in_memory_callback(ShowInMemoryCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_add_watch(wxCommandEvent& event);
        void on_tree_item_activated(wxTreeEvent& event);
        void on_tree_item_right_click(const wxTreeEvent& event);
        void on_tree_item_expanding(const wxTreeEvent& event);
        void on_tree_item_collapsing(const wxTreeEvent& event);

        void populate_tree(wxTreeCtrl* tree, wxTreeItemId parent,
                          const std::vector<::Vertex::Debugger::WatchVariable>& vars);
        void populate_locals_tree(wxTreeCtrl* tree, wxTreeItemId parent,
                                  const std::vector<::Vertex::Debugger::LocalVariable>& vars);

        wxSplitterWindow* m_splitter{};
        wxPanel* m_watchesPanel{};
        wxPanel* m_localsPanel{};

        wxTreeCtrl* m_watchTree{};
        wxTreeCtrl* m_localsTree{};

        wxTextCtrl* m_expressionInput{};
        wxButton* m_addButton{};

        wxBoxSizer* m_mainSizer{};

        std::vector<::Vertex::Debugger::WatchVariable> m_watches{};
        std::vector<::Vertex::Debugger::LocalVariable> m_locals{};
        bool m_rebuildingWatchTree{};

        AddWatchCallback m_addWatchCallback{};
        RemoveWatchCallback m_removeWatchCallback{};
        ModifyWatchCallback m_modifyWatchCallback{};
        ExpandWatchCallback m_expandWatchCallback{};
        ShowInMemoryCallback m_showInMemoryCallback{};

        Language::ILanguage& m_languageService;
    };
}
