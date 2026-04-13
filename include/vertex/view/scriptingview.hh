//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/bookctrl.h>
#include <wx/notebook.h>
#include <wx/splitter.h>
#include <wx/stc/stc.h>
#include <wx/timer.h>
#include <wx/treectrl.h>
#include <wx/dataview.h>
#include <wx/toolbar.h>

#include <memory>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vertex/viewmodel/scriptingviewmodel.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/gui/iconmanager/iiconmanager.hh>
#include <vertex/gui/theme/ithemeprovider.hh>
#include <vertex/event/eventbus.hh>

namespace Vertex::View
{
    class ScriptingView final : public wxDialog
    {
    public:
        ScriptingView(Language::ILanguage& languageService,
                      std::unique_ptr<ViewModel::ScriptingViewModel> viewModel,
                      Gui::IIconManager& iconManager,
                      Gui::IThemeProvider& themeProvider);

    private:
        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        void create_controls();
        void layout_controls();
        void bind_events();
        void configure_editor() const;
        void apply_theme() const;
        [[nodiscard]] bool toggle_view();
        void refresh_context_list();

        void on_clear_clicked(wxCommandEvent& event);
        void on_clear_output_clicked(wxCommandEvent& event);
        void on_open_clicked(const wxCommandEvent& event);
        void on_save_clicked(wxCommandEvent& event);
        void on_save_as_clicked(wxCommandEvent& event);
        void on_find_clicked(wxCommandEvent& event);
        void on_replace_clicked(wxCommandEvent& event);
        void on_go_to_line_clicked(wxCommandEvent& event);
        void on_new_from_template(const wxCommandEvent& event);
        void on_stop_context_clicked(wxCommandEvent& event);
        void on_suspend_context_clicked(wxCommandEvent& event);
        void on_resume_context_clicked(wxCommandEvent& event);
        void on_new_tab_clicked(const wxCommandEvent& event);
        void on_close_tab_clicked(wxCommandEvent& event);
        void on_execute_clicked(wxCommandEvent& event);
        void on_refresh_timer(wxTimerEvent& event);
        void on_editor_page_changed(wxBookCtrlEvent& event);
        void on_script_browser_item_activated(const wxTreeEvent& event);
        void on_script_browser_right_click(const wxTreeEvent& event);
        void on_editor_char_added(wxStyledTextEvent& event);
        void on_editor_key_down(wxKeyEvent& event);
        void on_editor_margin_click(wxStyledTextEvent& event);
        void on_editor_update_ui(wxStyledTextEvent& event);
        void on_editor_modified(wxStyledTextEvent& event);
        void on_context_list_selection_changed(wxListEvent& event);
        void on_context_list_right_click(const wxListEvent& event);
        void save_script(bool forceSaveAs);
        void bind_editor_events(wxStyledTextCtrl* editor);
        void create_editor_tab(const std::string& content,
                               const std::filesystem::path& filePath,
                               bool selectTab);
        [[nodiscard]] bool close_editor_tab(std::size_t tabIndex, bool promptUnsavedChanges);
        [[nodiscard]] int active_tab_index() const;
        [[nodiscard]] bool has_dirty_tabs() const;
        [[nodiscard]] int find_tab_index_by_path(const std::filesystem::path& filePath) const;
        [[nodiscard]] bool open_script_file(const std::filesystem::path& filePath);
        [[nodiscard]] bool prompt_open_script_dialog();
        [[nodiscard]] std::filesystem::path resolve_template_path(std::string_view templateFileName) const;
        void create_tab_from_template(int commandId);
        [[nodiscard]] std::optional<Scripting::ContextId> selected_context_for_toolbar_action() const;
        void refresh_script_browser();
        void populate_script_browser_tree(const std::filesystem::path& rootDirectory);
        void append_script_browser_children(const wxTreeItemId& parent, const std::filesystem::path& directory);
        void create_script_from_browser(const std::filesystem::path& directory);
        void rename_script_from_browser(const wxTreeItemId& itemId);
        void delete_script_from_browser(const wxTreeItemId& itemId);
        void load_recent_scripts();
        void add_recent_script(const std::filesystem::path& filePath);
        void remove_recent_script(const std::filesystem::path& filePath);
        void sync_active_editor_from_tab_selection();
        void sync_minimap_content() const;
        void update_tab_title(std::size_t tabIndex) const;
        void initialize_autocomplete();
        void initialize_call_tips();
        void show_autocomplete_for_prefix(const std::string& prefix) const;
        void show_member_autocomplete(const std::string& symbol);
        void show_call_tip_for_word(const std::string& functionName);
        [[nodiscard]] std::string execution_error_message(StatusCode status) const;
        void append_output_line(int style, std::string message) const;
        void refresh_variable_inspector();

        wxString state_to_string(Scripting::ScriptState state) const;
        wxColour state_to_colour(Scripting::ScriptState state) const;

        struct EditorTab final
        {
            wxStyledTextCtrl* editor{};
            std::filesystem::path filePath{};
            bool isDirty{};
            std::string tabTitle{};
        };

        wxBoxSizer* m_mainSizer{};
        wxToolBar* m_toolbar{};
        wxSplitterWindow* m_horizontalSplitter{};
        wxPanel* m_scriptBrowserPanel{};
        wxPanel* m_editorPanel{};
        wxTreeCtrl* m_scriptBrowser{};
        wxNotebook* m_editorNotebook{};
        wxStyledTextCtrl* m_minimap{};
        wxStyledTextCtrl* m_codeEditor{};
        wxListCtrl* m_contextList{};
        wxDataViewListCtrl* m_variableInspector{};
        wxStyledTextCtrl* m_outputPanel{};
        wxTimer m_refreshTimer;
        std::filesystem::path m_currentScriptPath{};
        std::size_t m_untitledCounter{1};
        std::size_t m_scriptBrowserRefreshTicks{};
        std::vector<EditorTab> m_tabs{};
        std::vector<std::filesystem::path> m_recentScripts{};
        std::vector<Scripting::ContextInfo> m_contextCache{};
        std::optional<Scripting::ContextId> m_selectedContextId{};
        std::vector<std::string> m_completionWords{};
        std::unordered_map<std::string, std::vector<std::string>> m_memberCompletionWords{};
        std::unordered_map<std::string, std::string> m_callTipSignatures{};
        std::string m_lastSearchQuery{};
        bool m_isDirty{};

        std::unique_ptr<ViewModel::ScriptingViewModel> m_viewModel{};
        Language::ILanguage& m_languageService;
        Gui::IIconManager& m_iconManager;
        Gui::IThemeProvider& m_themeProvider;
    };
}
