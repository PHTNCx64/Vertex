//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/settingsview.hh>
#include <vertex/view/pluginconfigview.hh>

#include <vertex/utility.hh>
#include <vertex/event/types/viewevent.hh>
#include <wx/app.h>

#include <wx/spinctrl.h>
#include <wx/button.h>
#include <wx/splitter.h>
#include <wx/statbox.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <ranges>

namespace Vertex::View
{
    SettingsView::SettingsView(Language::ILanguage& languageService, std::unique_ptr<ViewModel::SettingsViewModel> viewModel,
                               PluginConfigViewFactory pluginConfigFactory)
        : wxDialog(wxTheApp->GetTopWindow(),
                   wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("settingsWindow.title")),
                   wxDefaultPosition,
                   wxSize(FromDIP(StandardWidgetValues::STANDARD_X_DIP), FromDIP(StandardWidgetValues::STANDARD_Y_DIP)),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX),
          m_viewModel(std::move(viewModel)),
          m_languageService(languageService),
          m_pluginConfigFactory(std::move(pluginConfigFactory))
    {
        m_viewModel->set_event_callback(
          [this](const Event::EventId eventId, const Event::VertexEvent& event)
          {
              vertex_event_callback(eventId, event);
          });

        create_controls();
        layout_controls();
        bind_events();
        load_settings_from_viewmodel();
    }

    void SettingsView::vertex_event_callback([[maybe_unused]] Event::EventId eventId, [[maybe_unused]] const Event::VertexEvent& event)
    {
        std::ignore = toggle_view();
    }

    bool SettingsView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        Show();
        Raise();
        load_settings_from_viewmodel();
        return true;
    }

    void SettingsView::load_settings_from_viewmodel()
    {
        m_autoSaveCheckbox->SetValue(m_viewModel->get_gui_saving_enabled());
        m_autoSaveIntervalSpinCtrl->SetValue(m_viewModel->get_save_interval());
        m_rememberWindowPosCheckbox->SetValue(m_viewModel->get_remember_window_position());
        m_enableLoggingCheckbox->SetValue(m_viewModel->get_logging_status());
        m_themeChoice->SetSelection(m_viewModel->get_theme());

        m_readerThreadsSpinCtrl->SetValue(m_viewModel->get_reader_threads());
        m_threadBufferSizeSpinCtrl->SetValue(m_viewModel->get_thread_buffer_size());

        refresh_plugin_list();
        refresh_plugin_paths_list();

        refresh_language_choice();
        refresh_language_paths_list();

        m_applyButton->Enable(m_viewModel->has_pending_changes());

        const int lastTabIndex = m_viewModel->get_last_tab_index();
        if (lastTabIndex >= 0 && lastTabIndex < static_cast<int>(m_tabNotebook->GetPageCount()))
        {
            m_tabNotebook->SetSelection(lastTabIndex);
        }
    }

    void SettingsView::bind_events()
    {
        m_okButton->Bind(wxEVT_BUTTON,
                         [this]([[maybe_unused]] wxCommandEvent& event)
                         {
                             m_viewModel->save_settings();
                             Hide();
                         });

        m_cancelButton->Bind(wxEVT_BUTTON,
                             [this]([[maybe_unused]] wxCommandEvent& event)
                             {
                                 Hide();
                             });

        m_applyButton->Bind(wxEVT_BUTTON,
                            [this]([[maybe_unused]] wxCommandEvent& event)
                            {
                                m_viewModel->apply_settings();
                                m_applyButton->Enable(m_viewModel->has_pending_changes());
                            });

        m_resetButton->Bind(wxEVT_BUTTON,
                            [this]([[maybe_unused]] wxCommandEvent& event)
                            {
                                m_viewModel->reset_to_defaults();
                                load_settings_from_viewmodel();
                            });

        m_autoSaveCheckbox->Bind(wxEVT_CHECKBOX,
                                 [this](const wxCommandEvent& event)
                                 {
                                     m_viewModel->set_gui_saving_enabled(event.IsChecked());
                                     m_applyButton->Enable(true);
                                 });

        m_autoSaveIntervalSpinCtrl->Bind(wxEVT_SPINCTRL,
                                         [this](const wxSpinEvent& event)
                                         {
                                             m_viewModel->set_save_interval(event.GetValue());
                                             m_applyButton->Enable(true);
                                         });

        m_rememberWindowPosCheckbox->Bind(wxEVT_CHECKBOX,
                                          [this](const wxCommandEvent& event)
                                          {
                                              m_viewModel->set_remember_window_position(event.IsChecked());
                                              m_applyButton->Enable(true);
                                          });

        m_enableLoggingCheckbox->Bind(wxEVT_CHECKBOX,
                                      [this](const wxCommandEvent& event)
                                      {
                                          m_viewModel->set_logging_status(event.IsChecked());
                                          m_applyButton->Enable(true);
                                      });

        m_themeChoice->Bind(wxEVT_CHOICE,
                            [this](const wxCommandEvent& event)
                            {
                                m_viewModel->set_theme(event.GetSelection());
                                m_applyButton->Enable(true);
                            });

        m_readerThreadsSpinCtrl->Bind(wxEVT_SPINCTRL,
                                      [this](const wxSpinEvent& event)
                                      {
                                          m_viewModel->set_reader_threads(event.GetValue());
                                          m_applyButton->Enable(true);
                                      });

        m_threadBufferSizeSpinCtrl->Bind(wxEVT_SPINCTRL,
                                         [this](const wxSpinEvent& event)
                                         {
                                             m_viewModel->set_thread_buffer_size(event.GetValue());
                                             m_applyButton->Enable(true);
                                         });

        m_pluginListCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &SettingsView::on_plugin_selected, this);
        m_pluginListCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &SettingsView::on_plugin_deselected, this);
        m_loadPluginButton->Bind(wxEVT_BUTTON, &SettingsView::on_load_plugin_clicked, this);
        m_setActivePluginButton->Bind(wxEVT_BUTTON, &SettingsView::on_set_active_plugin_clicked, this);
        m_refreshPluginsButton->Bind(wxEVT_BUTTON, &SettingsView::on_refresh_plugins_clicked, this);

        m_unloadPluginButton->Bind(wxEVT_BUTTON, &SettingsView::on_unload_plugin_clicked, this);

        m_pluginPathsListCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &SettingsView::on_plugin_path_selected, this);
        m_pluginPathsListCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &SettingsView::on_plugin_path_deselected, this);
        m_addPluginPathButton->Bind(wxEVT_BUTTON, &SettingsView::on_add_plugin_path_clicked, this);
        m_removePluginPathButton->Bind(wxEVT_BUTTON, &SettingsView::on_remove_plugin_path_clicked, this);

        m_interfaceLanguageChoice->Bind(wxEVT_CHOICE, &SettingsView::on_language_changed, this);
        m_languagePathsListCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &SettingsView::on_language_path_selected, this);
        m_languagePathsListCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &SettingsView::on_language_path_deselected, this);
        m_addLanguagePathButton->Bind(wxEVT_BUTTON, &SettingsView::on_add_language_path_clicked, this);
        m_removeLanguagePathButton->Bind(wxEVT_BUTTON, &SettingsView::on_remove_language_path_clicked, this);

        m_tabNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED,
                            [this](wxBookCtrlEvent& event)
                            {
                                m_viewModel->set_last_tab_index(event.GetSelection());
                                event.Skip();
                            });
    }

    void SettingsView::create_controls()
    {
        m_settingsMainSizer = new wxBoxSizer(wxVERTICAL);
        m_tabNotebook = new wxNotebook(this, wxID_ANY);
        m_generalPanel = new wxPanel(m_tabNotebook);
        m_pluginPanel = new wxPanel(m_tabNotebook);
        m_languagePanel = new wxPanel(m_tabNotebook);
        m_memoryScannerPanel = new wxPanel(m_tabNotebook);

        if (m_pluginConfigFactory)
        {
            m_pluginConfigPanel = new wxPanel(m_tabNotebook);
        }

        m_resetButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("general.resetToDefaults")));
        m_applyButton = new wxButton(this, wxID_APPLY, wxString::FromUTF8(m_languageService.fetch_translation("general.apply")));
        m_cancelButton = new wxButton(this, wxID_CANCEL, wxString::FromUTF8(m_languageService.fetch_translation("general.cancel")));
        m_okButton = new wxButton(this, wxID_OK, wxString::FromUTF8(m_languageService.fetch_translation("general.ok")));
        m_applyButton->Enable(false);
        m_settingsButtonSizer = new wxBoxSizer(wxHORIZONTAL);

        create_general_tab_controls();
        create_plugin_tab_controls();
        create_language_tab_controls();
        create_memory_scanner_tab_controls();
    }

    void SettingsView::layout_controls()
    {
        m_tabNotebook->AddPage(m_generalPanel, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.general")));
        m_tabNotebook->AddPage(m_pluginPanel, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.plugins")));
        m_tabNotebook->AddPage(m_languagePanel, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.language")));
        m_tabNotebook->AddPage(m_memoryScannerPanel, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.memoryScanner")));

        if (m_pluginConfigPanel && m_pluginConfigFactory)
        {
            m_pluginConfigView = m_pluginConfigFactory(m_pluginConfigPanel);
            auto* configSizer = new wxBoxSizer(wxVERTICAL);
            configSizer->Add(m_pluginConfigView, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
            m_pluginConfigPanel->SetSizer(configSizer);

            if (m_pluginConfigView->has_panels())
            {
                m_tabNotebook->AddPage(m_pluginConfigPanel, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginConfig")));
            }
        }

        m_settingsButtonSizer->Add(m_resetButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_settingsButtonSizer->AddStretchSpacer();
        m_settingsButtonSizer->Add(m_applyButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_settingsButtonSizer->Add(m_cancelButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_settingsButtonSizer->Add(m_okButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_settingsMainSizer->Add(m_tabNotebook, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_settingsMainSizer->Add(m_settingsButtonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        layout_general_tab();
        layout_plugin_tab();
        layout_language_tab();
        layout_memory_scanner_tab();

        SetSizer(m_settingsMainSizer);
        Layout();
    }

    void SettingsView::create_general_tab_controls()
    {
        m_generalTabMainSizer = new wxBoxSizer(wxVERTICAL);
        m_appStaticBox = new wxStaticBox(m_generalPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.application")));
        m_appGroup = new wxStaticBoxSizer(m_appStaticBox, wxVERTICAL);
        m_autoSaveCheckbox = new wxCheckBox(m_appStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.enableAutoSave")));
        m_autoSaveIntervalSizer = new wxBoxSizer(wxHORIZONTAL);
        m_autoSaveIntervalSpinCtrl = new wxSpinCtrl(m_appStaticBox, wxID_ANY, "5", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 3600, 5);
        m_rememberWindowPosCheckbox = new wxCheckBox(m_appStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.rememberWindowPos")));
        m_themeSizer = new wxBoxSizer(wxHORIZONTAL);
        m_themeChoice = new wxChoice(m_appStaticBox, wxID_ANY);
        m_themeChoices.emplace_back(m_languageService.fetch_translation("settingsWindow.generalTab.themes.auto"));
        m_themeChoices.emplace_back(m_languageService.fetch_translation("settingsWindow.generalTab.themes.light"));
        m_themeChoices.emplace_back(m_languageService.fetch_translation("settingsWindow.generalTab.themes.dark"));
        m_themeChoice->Append(wxString::FromUTF8(m_themeChoices[ApplicationAppearance::SYSTEM]));
        m_themeChoice->Append(wxString::FromUTF8(m_themeChoices[ApplicationAppearance::LIGHT]));
        m_themeChoice->Append(wxString::FromUTF8(m_themeChoices[ApplicationAppearance::DARK]));
        m_loggingStaticBox = new wxStaticBox(m_generalPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.logging")));
        m_loggingGroup = new wxStaticBoxSizer(m_loggingStaticBox, wxVERTICAL);

        m_enableLoggingCheckbox = new wxCheckBox(m_loggingStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.enableLogging")));
    }

    void SettingsView::create_plugin_tab_controls()
    {
        m_pluginMainSizer = new wxBoxSizer(wxVERTICAL);
        m_pluginLeftSizer = new wxBoxSizer(wxVERTICAL);
        m_pluginRightSideSizer = new wxBoxSizer(wxVERTICAL);
        m_pluginListCtrl = new wxListCtrl(m_pluginPanel, wxID_ANY, wxDefaultPosition, wxSize(250, 200), wxLC_REPORT | wxLC_SINGLE_SEL);
        m_pluginListCtrl->AppendColumn(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.columnName")), wxLIST_FORMAT_LEFT, 150);
        m_pluginListCtrl->AppendColumn(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.columnStatus")), wxLIST_FORMAT_LEFT, 150);
        m_refreshPluginsButton = new wxButton(m_pluginPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.refreshList")));
        m_pluginInfoPanel = new wxPanel(m_pluginPanel);
        m_pluginInfoGroup = new wxStaticBoxSizer(wxVERTICAL, m_pluginInfoPanel, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.pluginInformation")));
        m_informationText = new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, EMPTY_STRING);
        m_pluginInfoGrid = new wxFlexGridSizer(4, 2, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::BORDER_TWICE);
        m_pluginInfoGrid->AddGrowableCol(StandardWidgetValues::STANDARD_PROPORTION);
        m_pluginNameLabel = new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, EMPTY_STRING);
        m_pluginVersionLabel = new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, EMPTY_STRING);
        m_pluginAuthorLabel = new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, EMPTY_STRING);
        m_pluginDescriptionLabel = new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
        m_pluginButtonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_loadPluginButton = new wxButton(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.loadPlugin")));
        m_setActivePluginButton = new wxButton(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.setAsActivePlugin")));
        m_unloadPluginButton = new wxButton(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.unloadPlugin")));
        m_pluginPathsStaticBox = new wxStaticBox(m_pluginPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.pluginPaths")));
        m_pluginPathsGroup = new wxStaticBoxSizer(m_pluginPathsStaticBox, wxVERTICAL);
        m_pluginPathsListCtrl = new wxListCtrl(m_pluginPathsStaticBox, wxID_ANY, wxDefaultPosition, wxSize(-1, 120), wxLC_REPORT | wxLC_SINGLE_SEL);
        m_pluginPathsListCtrl->AppendColumn(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.pathColumn")), wxLIST_FORMAT_LEFT, 600);
        m_pluginPathsButtonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_addPluginPathButton = new wxButton(m_pluginPathsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.addPath")));
        m_removePluginPathButton = new wxButton(m_pluginPathsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.removePath")));
        m_setActivePluginButton->Disable();
        m_unloadPluginButton->Disable();
        m_loadPluginButton->Disable();
        m_removePluginPathButton->Disable();
    }

    void SettingsView::create_language_tab_controls()
    {
        m_languageMainSizer = new wxBoxSizer(wxVERTICAL);
        m_langStaticBox = new wxStaticBox(m_languagePanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.interfaceLanguage")));
        m_langGroup = new wxStaticBoxSizer(m_langStaticBox, wxVERTICAL);
        m_interfaceLangSizer = new wxBoxSizer(wxHORIZONTAL);
        m_interfaceLanguageChoice = new wxChoice(m_langStaticBox, wxID_ANY);
        m_languagePathsStaticBox = new wxStaticBox(m_languagePanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.languagePaths")));
        m_languagePathsGroup = new wxStaticBoxSizer(m_languagePathsStaticBox, wxVERTICAL);
        m_languagePathsListCtrl = new wxListCtrl(m_languagePathsStaticBox, wxID_ANY, wxDefaultPosition, wxSize(-1, 120), wxLC_REPORT | wxLC_SINGLE_SEL);
        m_languagePathsListCtrl->AppendColumn(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.pathColumn")), wxLIST_FORMAT_LEFT, 600);
        m_languagePathsButtonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_addLanguagePathButton = new wxButton(m_languagePathsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.addPath")));
        m_removeLanguagePathButton = new wxButton(m_languagePathsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.removePath")));
    }

    void SettingsView::layout_general_tab() const
    {
        m_appGroup->Add(m_autoSaveCheckbox, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_autoSaveIntervalSizer->Add(new wxStaticText(m_appGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.autoSaveInterval"))), StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_autoSaveIntervalSizer->Add(m_autoSaveIntervalSpinCtrl, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_appGroup->Add(m_autoSaveIntervalSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_appGroup->Add(m_rememberWindowPosCheckbox, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_themeSizer->Add(new wxStaticText(m_appGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.generalTab.theme"))), StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_themeSizer->Add(m_themeChoice, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_appGroup->Add(m_themeSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_generalTabMainSizer->Add(m_appGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_loggingGroup->Add(m_enableLoggingCheckbox, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_generalTabMainSizer->Add(m_loggingGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_generalPanel->SetSizer(m_generalTabMainSizer);
    }

    void SettingsView::layout_plugin_tab()
    {
        m_topSizer = new wxBoxSizer(wxHORIZONTAL);
        m_pluginLeftSizer->Add(new wxStaticText(m_pluginPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.availablePlugins"))),
                               StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginLeftSizer->Add(m_pluginListCtrl, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginLeftSizer->Add(m_refreshPluginsButton, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_topSizer->Add(m_pluginLeftSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginInfoGroup->Add(m_informationText, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginInfoGrid->Add(new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.info.name"))),
                              StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
        m_pluginInfoGrid->Add(m_pluginNameLabel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_pluginInfoGrid->Add(new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.info.version"))),
                              StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
        m_pluginInfoGrid->Add(m_pluginVersionLabel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_pluginInfoGrid->Add(new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.info.author"))),
                              StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
        m_pluginInfoGrid->Add(m_pluginAuthorLabel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_pluginInfoGrid->Add(new wxStaticText(m_pluginInfoGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.info.description"))),
                              StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxALIGN_TOP);
        m_pluginInfoGrid->Add(m_pluginDescriptionLabel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_pluginInfoGroup->Add(m_pluginInfoGrid, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginButtonSizer->Add(m_loadPluginButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginButtonSizer->Add(m_setActivePluginButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginButtonSizer->Add(m_unloadPluginButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginInfoGroup->Add(m_pluginButtonSizer, StandardWidgetValues::NO_PROPORTION, wxALIGN_LEFT);
        m_pluginInfoPanel->SetSizer(m_pluginInfoGroup);
        m_pluginRightSideSizer->Add(m_pluginInfoPanel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_topSizer->Add(m_pluginRightSideSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginMainSizer->Add(m_topSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_pluginPathsGroup->Add(m_pluginPathsListCtrl, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginPathsButtonSizer->Add(m_addPluginPathButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginPathsButtonSizer->Add(m_removePluginPathButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginPathsButtonSizer->AddStretchSpacer();
        m_pluginPathsGroup->Add(m_pluginPathsButtonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_pluginMainSizer->Add(m_pluginPathsGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_pluginPanel->SetSizer(m_pluginMainSizer);
    }

    void SettingsView::layout_language_tab() const
    {
        m_interfaceLangSizer->Add(new wxStaticText(m_langGroup->GetStaticBox(), wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.language"))),
                                  StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_interfaceLangSizer->Add(m_interfaceLanguageChoice, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_langGroup->Add(m_interfaceLangSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_languageMainSizer->Add(m_langGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_languagePathsGroup->Add(m_languagePathsListCtrl, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_languagePathsButtonSizer->Add(m_addLanguagePathButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_languagePathsButtonSizer->Add(m_removeLanguagePathButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_languagePathsButtonSizer->AddStretchSpacer();
        m_languagePathsGroup->Add(m_languagePathsButtonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_languageMainSizer->Add(m_languagePathsGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_languagePanel->SetSizer(m_languageMainSizer);
    }

    void SettingsView::refresh_plugin_list() const
    {
        m_pluginListCtrl->DeleteAllItems();

        const auto& plugins = m_viewModel->get_plugins();
        for (const auto& [i, plugin] : plugins | std::views::enumerate)
        {
            const std::string dllName = std::filesystem::path(plugin.get_path()).filename().string();
            const long itemIndex = m_pluginListCtrl->InsertItem(static_cast<long>(i), wxString::FromUTF8(dllName));

            wxString status;
            if (m_viewModel->is_plugin_active(static_cast<int>(i)))
            {
                status = wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.currentlyActive"));
            }
            else if (m_viewModel->is_plugin_loaded(static_cast<int>(i)))
            {
                status = wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.loaded"));
            }
            else
            {
                status = wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.notLoaded"));
            }

            m_pluginListCtrl->SetItem(itemIndex, 1, status);
        }
    }

    void SettingsView::load_plugin_info(const int pluginIndex) const
    {
        const auto& plugins = m_viewModel->get_plugins();

        if (pluginIndex < 0 || pluginIndex >= static_cast<int>(plugins.size()))
        {
            clear_plugin_info();
            return;
        }

        const auto& plugin = plugins[pluginIndex];
        const bool isLoaded = m_viewModel->is_plugin_loaded(pluginIndex);
        const bool isActive = m_viewModel->is_plugin_active(pluginIndex);

        if (!isLoaded)
        {
            m_pluginVersionLabel->SetLabel(EMPTY_STRING);
            m_pluginAuthorLabel->SetLabel(EMPTY_STRING);
            m_pluginDescriptionLabel->SetLabel(EMPTY_STRING);
            m_pluginNameLabel->SetLabel(EMPTY_STRING);
            m_informationText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.unloadedMsgInfo")));
        }
        else
        {
            m_pluginNameLabel->SetLabel(wxString::FromUTF8(plugin.get_plugin_info().pluginName));
            m_pluginVersionLabel->SetLabel(std::to_string(plugin.get_plugin_info().apiVersion));
            m_pluginAuthorLabel->SetLabel(wxString::FromUTF8(plugin.get_plugin_info().pluginAuthor));
            m_pluginDescriptionLabel->SetLabel(wxString::FromUTF8(plugin.get_plugin_info().pluginDescription));
            m_informationText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.loadedMsgInfo")));
        }

        m_loadPluginButton->Enable(!isLoaded);
        m_setActivePluginButton->Enable(isLoaded && !isActive);
        m_unloadPluginButton->Enable(isLoaded && !isActive);
    }

    void SettingsView::clear_plugin_info() const
    {
        m_informationText->SetLabel(EMPTY_STRING);
        m_pluginNameLabel->SetLabel(EMPTY_STRING);
        m_pluginVersionLabel->SetLabel(EMPTY_STRING);
        m_pluginAuthorLabel->SetLabel(EMPTY_STRING);
        m_pluginDescriptionLabel->SetLabel(EMPTY_STRING);
        m_loadPluginButton->Enable(false);
        m_setActivePluginButton->Enable(false);
        m_unloadPluginButton->Enable(false);
    }

    void SettingsView::on_plugin_selected(const wxListEvent& event)
    {
        const int selectedIndex = static_cast<int>(event.GetIndex());
        load_plugin_info(selectedIndex);
    }

    void SettingsView::on_plugin_deselected([[maybe_unused]] wxListEvent& event)
    {
        const long selectedIndex = m_pluginListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

        if (selectedIndex == -1)
        {
            clear_plugin_info();
        }
    }

    void SettingsView::on_load_plugin_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const long selectedIndex = m_pluginListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == -1)
        {
            return;
        }

        m_viewModel->load_plugin(static_cast<std::size_t>(selectedIndex));
        refresh_plugin_list();
        m_pluginListCtrl->SetItemState(selectedIndex, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);

        load_plugin_info(static_cast<int>(selectedIndex));
    }

    void SettingsView::on_unload_plugin_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const long selectedIndex = m_pluginListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == -1)
        {
            return;
        }

        m_viewModel->unload_plugin(static_cast<std::size_t>(selectedIndex));
        refresh_plugin_list();
        clear_plugin_info();
    }

    void SettingsView::update_plugin_config_tab()
    {
        if (!m_pluginConfigFactory || !m_pluginConfigPanel || !m_pluginConfigView)
        {
            return;
        }

        m_pluginConfigView->rebuild_ui();

        const auto pageIndex = m_tabNotebook->FindPage(m_pluginConfigPanel);
        const bool hasPanels = m_pluginConfigView->has_panels();

        if (hasPanels && pageIndex == wxNOT_FOUND)
        {
            m_tabNotebook->AddPage(m_pluginConfigPanel,
                wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginConfig")));
        }
        else if (!hasPanels && pageIndex != wxNOT_FOUND)
        {
            m_tabNotebook->RemovePage(pageIndex);
        }
    }

    void SettingsView::on_set_active_plugin_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const long selectedIndex = m_pluginListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == -1)
        {
            return;
        }

        m_viewModel->set_active_plugin(static_cast<std::size_t>(selectedIndex));
        refresh_plugin_list();
        load_plugin_info(static_cast<int>(selectedIndex));
        update_plugin_config_tab();
        m_applyButton->Enable(true);
    }

    void SettingsView::on_refresh_plugins_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        refresh_plugin_list();
    }

    void SettingsView::refresh_plugin_paths_list()
    {
        m_pluginPathsListCtrl->DeleteAllItems();
        m_pluginPaths = m_viewModel->get_plugin_paths();

        for (const auto& [i, path] : m_pluginPaths | std::views::enumerate)
        {
            m_pluginPathsListCtrl->InsertItem(static_cast<long>(i), wxString::FromUTF8(path.string()));
        }
    }

    void SettingsView::on_add_plugin_path_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxDirDialog dialog(this, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.selectPluginPath")));

        if (dialog.ShowModal() == wxID_OK)
        {
            const std::filesystem::path selectedPath = dialog.GetPath().ToStdString();

            if (m_viewModel->add_plugin_path(selectedPath))
            {
                refresh_plugin_paths_list();
                m_applyButton->Enable(true);
            }
            else
            {
                wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.pluginsTab.pathAlreadyExists")), m_languageService.fetch_translation("general.error"),
                             wxOK | wxICON_WARNING, this);
            }
        }
    }

    void SettingsView::on_remove_plugin_path_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const long selectedIndex = m_pluginPathsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

        if (selectedIndex == -1 || selectedIndex >= static_cast<long>(m_pluginPaths.size()))
        {
            return;
        }

        const auto& pathToRemove = m_pluginPaths[selectedIndex];

        if (m_viewModel->remove_plugin_path(pathToRemove))
        {
            refresh_plugin_paths_list();
            m_applyButton->Enable(true);
            m_removePluginPathButton->Enable(false);
        }
    }

    void SettingsView::on_plugin_path_selected([[maybe_unused]] wxListEvent& event)
    {
        const long selectedIndex = m_pluginPathsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        m_removePluginPathButton->Enable(selectedIndex != -1);
    }

    void SettingsView::on_plugin_path_deselected([[maybe_unused]] wxListEvent& event)
    {
        const long selectedIndex = m_pluginPathsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        m_removePluginPathButton->Enable(selectedIndex != -1);
    }

    void SettingsView::refresh_language_choice()
    {
        m_interfaceLanguageChoice->Clear();
        m_availableLanguages = m_viewModel->get_available_languages();

        int activeIndex = 0;

        for (const auto& [i, entry] : m_availableLanguages | std::views::enumerate)
        {
            const auto& [languageKey, languagePath] = entry;
            m_interfaceLanguageChoice->Append(wxString::FromUTF8(languageKey));

            if (m_viewModel->is_active_language(languageKey))
            {
                activeIndex = static_cast<int>(i);
            }
        }

        if (m_interfaceLanguageChoice->GetCount() > 0)
        {
            m_interfaceLanguageChoice->SetSelection(activeIndex);
        }
    }

    void SettingsView::on_language_changed(const wxCommandEvent& event)
    {
        const int selection = event.GetSelection();
        if (selection == wxNOT_FOUND)
        {
            return;
        }

        const wxString selectedLanguage = m_interfaceLanguageChoice->GetString(selection);
        m_viewModel->set_active_language(selectedLanguage.ToStdString());
        m_applyButton->Enable(true);
    }

    void SettingsView::refresh_language_paths_list()
    {
        m_languagePathsListCtrl->DeleteAllItems();
        m_languagePaths = m_viewModel->get_language_paths();

        for (const auto& [i, path] : m_languagePaths | std::views::enumerate)
        {
            m_languagePathsListCtrl->InsertItem(static_cast<long>(i), wxString::FromUTF8(path.string()));
        }
    }

    void SettingsView::on_add_language_path_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxDirDialog dialog(this, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.selectLanguagePath")));

        if (dialog.ShowModal() == wxID_OK)
        {
            const std::filesystem::path selectedPath = dialog.GetPath().ToStdString();

            if (m_viewModel->add_language_path(selectedPath))
            {
                refresh_language_paths_list();
                refresh_language_choice();
                m_applyButton->Enable(true);
            }
            else
            {
                wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.languageTab.pathAlreadyExists")),
                             wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                             wxOK | wxICON_WARNING, this);
            }
        }
    }

    void SettingsView::on_remove_language_path_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const long selectedIndex = m_languagePathsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

        if (selectedIndex == -1 || selectedIndex >= static_cast<long>(m_languagePaths.size()))
        {
            return;
        }

        const auto& pathToRemove = m_languagePaths[selectedIndex];

        if (m_viewModel->remove_language_path(pathToRemove))
        {
            refresh_language_paths_list();
            refresh_language_choice();
            m_applyButton->Enable(true);
            m_removeLanguagePathButton->Enable(false);
        }
    }

    void SettingsView::on_language_path_selected([[maybe_unused]] wxListEvent& event)
    {
        const long selectedIndex = m_languagePathsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        m_removeLanguagePathButton->Enable(selectedIndex != -1);
    }

    void SettingsView::on_language_path_deselected([[maybe_unused]] wxListEvent& event)
    {
        const long selectedIndex = m_languagePathsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        m_removeLanguagePathButton->Enable(selectedIndex != -1);
    }

    void SettingsView::create_memory_scanner_tab_controls()
    {
        m_memoryScannerMainSizer = new wxBoxSizer(wxVERTICAL);
        m_readerThreadsStaticBox = new wxStaticBox(m_memoryScannerPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.memoryScannerTab.readerThreads")));
        m_readerThreadsGroup = new wxStaticBoxSizer(m_readerThreadsStaticBox, wxVERTICAL);
        m_readerThreadsSpinCtrl = new wxSpinCtrl(m_readerThreadsStaticBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 64, 1);
        m_threadBufferSizeStaticBox = new wxStaticBox(m_memoryScannerPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.memoryScannerTab.threadBufferSize")));
        m_threadBufferSizeGroup = new wxStaticBoxSizer(m_threadBufferSizeStaticBox, wxVERTICAL);
        m_threadBufferSizeSpinCtrl = new wxSpinCtrl(m_threadBufferSizeStaticBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 512, 32);
        m_threadBufferSizeSpinCtrl->SetToolTip(wxString::FromUTF8(m_languageService.fetch_translation("settingsWindow.memoryScannerTab.threadBufferSizeDescription")));
    }

    void SettingsView::layout_memory_scanner_tab() const
    {
        m_readerThreadsGroup->Add(m_readerThreadsSpinCtrl, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_memoryScannerMainSizer->Add(m_readerThreadsGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_threadBufferSizeGroup->Add(m_threadBufferSizeSpinCtrl, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_memoryScannerMainSizer->Add(m_threadBufferSizeGroup, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_memoryScannerPanel->SetSizer(m_memoryScannerMainSizer);
    }
}
