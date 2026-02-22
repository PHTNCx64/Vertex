//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/listctrl.h>

#include <memory>
#include <functional>
#include <vertex/language/language.hh>
#include <vertex/viewmodel/settingsviewmodel.hh>
#include <vertex/event/eventbus.hh>

namespace Vertex::View
{
    class PluginConfigView;

    using PluginConfigViewFactory = std::function<PluginConfigView*(wxWindow*)>;

    class SettingsView final : public wxDialog
    {
      public:
        SettingsView(Language::ILanguage& languageService, std::unique_ptr<ViewModel::SettingsViewModel> viewModel,
                     PluginConfigViewFactory pluginConfigFactory = {});

      private:
        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        void create_controls();
        void layout_controls();
        void bind_events();
        [[nodiscard]] bool toggle_view();
        void load_settings_from_viewmodel();

        void create_general_tab_controls();
        void create_plugin_tab_controls();
        void create_language_tab_controls();
        void create_memory_scanner_tab_controls();
        void layout_general_tab() const;
        void layout_plugin_tab();
        void layout_language_tab() const;
        void layout_memory_scanner_tab() const;

        void update_plugin_config_tab();
        void refresh_plugin_list() const;
        void load_plugin_info(int pluginIndex) const;
        void clear_plugin_info() const;
        void on_plugin_selected(const wxListEvent& event);
        void on_plugin_deselected(wxListEvent& event);
        void on_load_plugin_clicked(wxCommandEvent& event);
        void on_unload_plugin_clicked(wxCommandEvent& event);
        void on_set_active_plugin_clicked(wxCommandEvent& event);
        void on_refresh_plugins_clicked(wxCommandEvent& event);

        void refresh_plugin_paths_list();
        void on_add_plugin_path_clicked(wxCommandEvent& event);
        void on_remove_plugin_path_clicked(wxCommandEvent& event);
        void on_plugin_path_selected(wxListEvent& event);
        void on_plugin_path_deselected(wxListEvent& event);

        void refresh_language_choice();
        void on_language_changed(const wxCommandEvent& event);

        void refresh_language_paths_list();
        void on_add_language_path_clicked(wxCommandEvent& event);
        void on_remove_language_path_clicked(wxCommandEvent& event);
        void on_language_path_selected(wxListEvent& event);
        void on_language_path_deselected(wxListEvent& event);

        wxNotebook* m_tabNotebook{};
        wxPanel* m_generalPanel{};
        wxPanel* m_pluginPanel{};
        wxPanel* m_languagePanel{};
        wxPanel* m_memoryScannerPanel{};

        wxButton* m_okButton{};
        wxButton* m_cancelButton{};
        wxButton* m_applyButton{};
        wxButton* m_resetButton{};

        wxCheckBox* m_autoSaveCheckbox{};
        wxCheckBox* m_rememberWindowPosCheckbox{};
        wxCheckBox* m_enableLoggingCheckbox{};
        wxSpinCtrl* m_autoSaveIntervalSpinCtrl{};
        wxChoice* m_themeChoice{};
        wxStaticBoxSizer* m_loggingGroup{};
        wxBoxSizer* m_themeSizer{};
        wxStaticBoxSizer* m_appGroup{};
        wxBoxSizer* m_autoSaveIntervalSizer{};
        wxStaticBoxSizer* m_langGroup{};
        wxBoxSizer* m_interfaceLangSizer{};
        wxBoxSizer* m_pluginRightSideSizer{};
        wxBoxSizer* m_pluginButtonSizer{};
        wxBoxSizer* m_generalTabMainSizer{};
        wxBoxSizer* m_settingsMainSizer{};

        wxListCtrl* m_pluginListCtrl{};
        wxPanel* m_pluginInfoPanel{};
        wxStaticText* m_informationText{};
        wxStaticText* m_pluginNameLabel{};
        wxStaticText* m_pluginVersionLabel{};
        wxStaticText* m_pluginAuthorLabel{};
        wxStaticText* m_pluginDescriptionLabel{};
        wxButton* m_unloadPluginButton{};
        wxButton* m_refreshPluginsButton{};
        wxButton* m_setActivePluginButton{};
        wxButton* m_loadPluginButton{};

        wxListCtrl* m_pluginPathsListCtrl{};
        wxButton* m_addPluginPathButton{};
        wxButton* m_removePluginPathButton{};

        wxChoice* m_interfaceLanguageChoice{};
        wxListCtrl* m_languagePathsListCtrl{};
        wxButton* m_addLanguagePathButton{};
        wxButton* m_removeLanguagePathButton{};

        wxSpinCtrl* m_readerThreadsSpinCtrl{};
        wxSpinCtrl* m_threadBufferSizeSpinCtrl{};
        wxStaticBoxSizer* m_readerThreadsGroup{};
        wxStaticBoxSizer* m_threadBufferSizeGroup{};
        wxStaticBox* m_readerThreadsStaticBox{};
        wxStaticBox* m_threadBufferSizeStaticBox{};
        wxBoxSizer* m_memoryScannerMainSizer{};

        wxStaticBoxSizer* m_languagePathsGroup{};
        wxBoxSizer* m_languagePathsButtonSizer{};

        wxBoxSizer* m_pluginMainSizer{};
        wxBoxSizer* m_pluginLeftSizer{};
        wxStaticBoxSizer* m_pluginInfoGroup{};
        wxFlexGridSizer* m_pluginInfoGrid{};
        wxBoxSizer* m_languageMainSizer{};
        wxBoxSizer* m_settingsButtonSizer{};
        wxStaticBoxSizer* m_pluginPathsGroup{};
        wxBoxSizer* m_pluginPathsButtonSizer{};
        wxStaticBox* m_loggingStaticBox{};
        wxStaticBox* m_appStaticBox{};
        wxStaticBox* m_pluginPathsStaticBox{};
        wxStaticBox* m_langStaticBox{};
        wxStaticBox* m_languagePathsStaticBox{};
        wxBoxSizer* m_topSizer{};

        std::vector<std::reference_wrapper<const std::string>> m_themeChoices{};

        std::vector<std::filesystem::path> m_pluginPaths{};
        std::vector<std::filesystem::path> m_languagePaths{};
        std::unordered_map<std::string, std::filesystem::path> m_availableLanguages{};

        std::unique_ptr<ViewModel::SettingsViewModel> m_viewModel{};
        Language::ILanguage& m_languageService;

        wxPanel* m_pluginConfigPanel{};
        PluginConfigView* m_pluginConfigView{};
        PluginConfigViewFactory m_pluginConfigFactory{};
    };
}
