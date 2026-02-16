//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/resettable_call_once.hh>
#include <vertex/event/types/viewupdateevent.hh>
#include <vertex/scanner/valuetypes.hh>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <vertex/view/mainview.hh>
#include <vertex/view/aboutview.hh>
#include <wx/filedlg.h>
#include <ranges>

#include <wx/spinctrl.h>

namespace Vertex::View
{
    MainView::MainView(const wxString& title, std::unique_ptr<ViewModel::MainViewModel> viewModel, Language::ILanguage& languageService, Gui::IIconManager& iconManager)
        : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(StandardWidgetValues::STANDARD_X_DIP, StandardWidgetValues::STANDARD_Y_DIP)),
          m_viewModel(std::move(viewModel)),
          m_languageService(languageService),
          m_iconManager(iconManager)
    {
        wxTheApp->SetTopWindow(this);
        m_auiManager.SetManagedWindow(this);

        m_viewModel->set_event_callback(
          [this](const Event::EventId eventId, const Event::VertexEvent& event)
          {
              vertex_event_callback(eventId, event);
          });

        create_controls();
        layout_controls();
        bind_events();
        restore_ui_state();

        set_control_status(ControlStatus::NO_PROCESS_OPENED);
        update_view();
    }

    void MainView::create_controls()
    {
        m_mainPanel = new wxPanel(this, wxID_ANY);
        m_mainBoxSizer = new wxBoxSizer(wxVERTICAL);
        m_auiManager.AddPane(m_mainPanel, wxAuiPaneInfo().CenterPane().Name("m_mainPanel"));
        m_auiManager.SetManagedWindow(this);

        m_menuBar = new wxMenuBar();
        m_fileMenu = new wxMenu();
        m_helpMenu = new wxMenu();
        m_fileMenu->Append(StandardMenuIds::MainViewIds::ID_NEW_PROJECT, wxString::Format("&%s\tCTRL+N", wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.newProject"))),
                           wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.newProjectDescription")));
        m_fileMenu->Append(StandardMenuIds::MainViewIds::ID_OPEN_PROJECT, wxString::Format("&%s\tCTRL+O", wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.openProject"))),
                           wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.openProjectDescription")));
        m_fileMenu->AppendSeparator();
        m_fileMenu->Append(StandardMenuIds::MainViewIds::ID_EXIT_APPLICATION, wxString::Format("&%s\tALT+F4", wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.exitApplication"))),
                           wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.exitApplicationDescription")));
        m_helpMenu->Append(StandardMenuIds::MainViewIds::ID_HELP_ABOUT, wxString::Format("&%s", wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.about"))),
                           wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.aboutDescription")));

        m_auiToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL | wxAUI_TB_PLAIN_BACKGROUND);
        m_auiToolBar->SetToolBitmapSize(wxSize(StandardWidgetValues::ICON_SIZE, StandardWidgetValues::ICON_SIZE));
        const Theme theme = m_viewModel->get_theme();
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_PROCESS_LIST, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.processList")),
                              m_iconManager.get_icon("search", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.processListDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_KILL_PROCESS, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.killProcess")),
                              m_iconManager.get_icon("close", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.killProcessDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_NEW_PROCESS, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.newProcess")),
                              m_iconManager.get_icon("new_window", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.newProcessDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_CLOSE_PROCESS, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.closeProcess")),
                              m_iconManager.get_icon("close", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.closeProcessDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_DEBUGGER, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.debugger")),
                              m_iconManager.get_icon("memory", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.debuggerDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_SETTINGS, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.settings")),
                              m_iconManager.get_icon("settings", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.settingsDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_ANALYTICS, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.analytics")),
                              m_iconManager.get_icon("analytics", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.analyticsDescription")));
        m_auiToolBar->AddTool(StandardMenuIds::MainViewIds::ID_INJECTOR, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.injector")),
                              m_iconManager.get_icon("injector", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                              wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.toolbar.injectorDescription")));
        m_auiToolBar->Realize();

        m_scannedValuesAndScanOptionsSizer = new wxFlexGridSizer(1, 2, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::STANDARD_BORDER);
        m_processInformationAndStatusText = new wxStaticText(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.noProcessSelected")));
        m_initialScanButton = new wxButton(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.buttons.initialScan")));
        m_nextScanButton = new wxButton(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.buttons.nextScan")));
        m_undoScanButton = new wxButton(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.buttons.undoScan")));
        m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_scanProgressBar = new wxGauge(m_mainPanel, wxID_ANY, StandardWidgetValues::GAUGE_MAX_VALUE, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL);
        m_scannedValuesAmountText = new wxStaticText(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.valuesFound")));
        m_scannedValuesPanel = new CustomWidgets::ScannedValuesPanel(m_mainPanel, m_languageService,
                                                                     std::shared_ptr<ViewModel::MainViewModel>(m_viewModel.get(),
                                                                                                               [](auto*)
                                                                                                               {
                                                                                                               }));
        m_valuesSizer = new wxBoxSizer(wxVERTICAL);
        m_scanOptionsStaticBox = new wxStaticBox(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scanOptions")));
        m_scanOptionsSizer = new wxStaticBoxSizer(m_scanOptionsStaticBox, wxVERTICAL);
        m_valueInputSizer = new wxBoxSizer(wxVERTICAL);
        m_valueInputText = new wxStaticText(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.betweenValue")));
        m_valueInputControlsSizer = new wxBoxSizer(wxHORIZONTAL);
        m_valueInputTextControl = new wxTextCtrl(m_scanOptionsStaticBox, wxID_ANY, wxEmptyString);
        m_valueInputText2 = new wxStaticText(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.andValue")));
        m_valueInputTextControl2 = new wxTextCtrl(m_scanOptionsStaticBox, wxID_ANY, wxEmptyString);
        m_valueInputTextControl2->Show(false);
        m_valueInputText2->Show(false);
        m_hexadecimalValueCheckBox = new wxCheckBox(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.hexadecimal")));
        m_hexadecimalValueSizer = new wxBoxSizer(wxHORIZONTAL);
        m_valueTypeSizer = new wxBoxSizer(wxVERTICAL);
        m_valueTypeText = new wxStaticText(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.valueType")));
        m_valueTypeComboBox = new wxComboBox(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.valueTypes.byte")), wxDefaultPosition, wxDefaultSize,
                                             0, nullptr, wxCB_READONLY);
        m_scanTypeSizer = new wxBoxSizer(wxVERTICAL);
        m_scanTypeText = new wxStaticText(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scanType")));
        m_scanTypeComboBox = new wxComboBox(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.scanTypes.exact")), wxDefaultPosition, wxDefaultSize,
                                            0, nullptr, wxCB_READONLY);
        m_endiannessTypeSizer = new wxBoxSizer(wxVERTICAL);
        m_endiannessTypeText = new wxStaticText(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.endianness")));
        m_endiannessTypeComboBox = new wxComboBox(m_scanOptionsStaticBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        wxArrayString endiannessTypes{};
        endiannessTypes.Add(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.endianness.littleEndian")));
        endiannessTypes.Add(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.endianness.bigEndian")));
        endiannessTypes.Add(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.endianness.hostCpu")));
        m_endiannessTypeComboBox->Append(endiannessTypes);
        m_alignmentBoxSizer = new wxBoxSizer(wxVERTICAL);
        m_alignmentTopSizer = new wxBoxSizer(wxVERTICAL);
        m_alignmentInformationText = new wxStaticText(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.alignment")));
        m_alignmentValue = new wxSpinCtrl(m_scanOptionsStaticBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, StandardWidgetValues::SPIN_MIN_VALUE,
                                          StandardWidgetValues::SPIN_MAX_VALUE, StandardWidgetValues::SPIN_DEFAULT_VALUE);
        m_alignmentCheckBox = new wxCheckBox(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.alignedScan")));
        m_memoryRegionSettingsSizer = new wxBoxSizer(wxHORIZONTAL);
        m_memoryRegionSettingsButton = new wxButton(m_scanOptionsStaticBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.memoryRegionSettings")));
        m_addAddressManuallyButton = new wxButton(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.addAddressManually")));
        m_savedAddressesPanel = new CustomWidgets::SavedAddressesPanel(m_mainPanel, m_languageService,
                                                                       std::shared_ptr<ViewModel::MainViewModel>(m_viewModel.get(),
                                                                                                                 [](auto*)
                                                                                                                 {
                                                                                                                 }));
        m_processValidityCheck = new wxTimer(this, wxID_ANY);
        m_scanProgressTimer = new wxTimer(this, wxID_ANY);

        m_scannedValuesPanel->set_add_to_table_callback(
          [this]([[maybe_unused]] int index, const std::uint64_t address)
          {
              if (m_viewModel->has_saved_address(address))
              {
                  wxMessageBox(wxString::FromUTF8(wxString::Format(m_languageService.fetch_translation("mainWindow.errors.addressAlreadyAdded").c_str(), address)),
                               wxString::FromUTF8(m_languageService.fetch_translation("general.error")), wxOK | wxICON_ERROR);
                  return;
              }
              m_viewModel->add_saved_address(address);
              m_savedAddressesPanel->refresh_list();
              m_savedAddressesPanel->start_auto_refresh();
          });
    }

    void MainView::layout_controls()
    {
        m_mainPanel->SetSizer(m_mainBoxSizer);
        m_menuBar->Append(m_fileMenu, wxString::FromUTF8(fmt::format("&{}", m_languageService.fetch_translation("mainWindow.ui.file"))));
        m_menuBar->Append(m_helpMenu, wxString::FromUTF8(fmt::format("&{}", m_languageService.fetch_translation("mainWindow.ui.help"))));
        SetMenuBar(m_menuBar);
        m_auiManager.AddPane(m_auiToolBar, wxAuiPaneInfo()
                                             .Name("MainToolbar")
                                             .ToolbarPane()
                                             .Top()
                                             .Row(StandardWidgetValues::AUI_TOOLBAR_ROW)
                                             .Fixed()
                                             .Dockable(false)
                                             .Floatable(false)
                                             .Movable(false)
                                             .Gripper(false)
                                             .CaptionVisible(false)
                                             .CloseButton(false)
                                             .MaximizeButton(false)
                                             .MinimizeButton(false)
                                             .PinButton(false));

        m_topSectionSizer = new wxBoxSizer(wxVERTICAL);
        m_topSectionSizer->Add(m_processInformationAndStatusText, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_topSectionSizer->Add(m_scanProgressBar, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_mainBoxSizer->Add(m_topSectionSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_scannedValuesAndScanOptionsSizer->AddGrowableRow(StandardWidgetValues::NO_PROPORTION);
        m_scannedValuesAndScanOptionsSizer->AddGrowableCol(StandardWidgetValues::NO_PROPORTION, StandardWidgetValues::COLUMN_PROPORTION_LARGE);
        m_scannedValuesAndScanOptionsSizer->AddGrowableCol(StandardWidgetValues::STANDARD_PROPORTION, StandardWidgetValues::STANDARD_PROPORTION);
        m_valuesSizer->Add(m_scannedValuesAmountText, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_valuesSizer->Add(m_scannedValuesPanel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_scannedValuesAndScanOptionsSizer->Add(m_valuesSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_valueInputControlsSizer->Add(m_valueInputTextControl, StandardWidgetValues::STANDARD_PROPORTION, wxALIGN_CENTER_VERTICAL);
        m_valueInputControlsSizer->Add(m_valueInputText2, StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_valueInputControlsSizer->Add(m_valueInputTextControl2, StandardWidgetValues::STANDARD_PROPORTION, wxALIGN_CENTER_VERTICAL);
        m_valueInputSizer->Add(m_valueInputText, StandardWidgetValues::NO_PROPORTION, wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_valueInputSizer->Add(m_valueInputControlsSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_scanOptionsSizer->Add(m_valueInputSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_hexadecimalValueSizer->Add(m_hexadecimalValueCheckBox, StandardWidgetValues::NO_PROPORTION, StandardWidgetValues::NO_PROPORTION);
        m_scanOptionsSizer->Add(m_hexadecimalValueSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_valueTypeSizer->Add(m_valueTypeText, StandardWidgetValues::NO_PROPORTION, wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_valueTypeSizer->Add(m_valueTypeComboBox, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_scanOptionsSizer->Add(m_valueTypeSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_scanTypeSizer->Add(m_scanTypeText, StandardWidgetValues::NO_PROPORTION, wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_scanTypeSizer->Add(m_scanTypeComboBox, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_scanOptionsSizer->Add(m_scanTypeSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_endiannessTypeSizer->Add(m_endiannessTypeText, StandardWidgetValues::NO_PROPORTION, wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_endiannessTypeSizer->Add(m_endiannessTypeComboBox, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_scanOptionsSizer->Add(m_endiannessTypeSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_alignmentTopSizer->Add(m_alignmentInformationText, StandardWidgetValues::NO_PROPORTION, wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_alignmentTopSizer->Add(m_alignmentValue, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_alignmentBoxSizer->Add(m_alignmentTopSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_alignmentBoxSizer->Add(m_alignmentCheckBox, StandardWidgetValues::NO_PROPORTION, wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_scanOptionsSizer->Add(m_alignmentBoxSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_memoryRegionSettingsSizer->Add(m_memoryRegionSettingsButton, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_scanOptionsSizer->Add(m_memoryRegionSettingsSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->Add(m_initialScanButton, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->Add(m_nextScanButton, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->Add(m_undoScanButton, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_scanOptionsWithButtonsSizer = new wxBoxSizer(wxVERTICAL);
        m_scanOptionsWithButtonsSizer->Add(m_buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_scanOptionsWithButtonsSizer->Add(m_scanOptionsSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        m_scannedValuesAndScanOptionsSizer->Add(m_scanOptionsWithButtonsSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainBoxSizer->Add(m_scannedValuesAndScanOptionsSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainBoxSizer->Add(m_addAddressManuallyButton, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainBoxSizer->Add(m_savedAddressesPanel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_auiManager.Update();
    }

    void MainView::vertex_event_callback(const Event::EventId eventId, const Event::VertexEvent& event)
    {
        switch (eventId)
        {
        case Event::PROCESS_CLOSED_EVENT: handle_process_closed(); break;
        case Event::VIEW_UPDATE_EVENT:
        {
            const auto& viewUpdateEvent = static_cast<const Event::ViewUpdateEvent&>(event);
            update_view(viewUpdateEvent.get_update_flags());
        }
        break;
        case Event::PROCESS_OPEN_EVENT: update_view(ViewUpdateFlags::PROCESS_INFO); break;
        default:;
        }
    }

    void MainView::bind_events()
    {
        m_initialScanButton->Bind(wxEVT_BUTTON, &MainView::on_initial_scan_clicked, this);
        m_nextScanButton->Bind(wxEVT_BUTTON, &MainView::on_next_scan_clicked, this);
        m_undoScanButton->Bind(wxEVT_BUTTON, &MainView::on_undo_scan_clicked, this);
        m_addAddressManuallyButton->Bind(wxEVT_BUTTON, &MainView::on_add_address_manually_clicked, this);
        m_memoryRegionSettingsButton->Bind(wxEVT_BUTTON, &MainView::on_memory_region_settings_clicked, this);
        m_valueInputTextControl->Bind(wxEVT_TEXT, &MainView::on_value_input_changed, this);
        m_valueInputTextControl2->Bind(wxEVT_TEXT, &MainView::on_value_input2_changed, this);
        m_hexadecimalValueCheckBox->Bind(wxEVT_CHECKBOX, &MainView::on_hexadecimal_changed, this);
        m_valueTypeComboBox->Bind(wxEVT_COMBOBOX, &MainView::on_value_type_changed, this);
        m_scanTypeComboBox->Bind(wxEVT_COMBOBOX, &MainView::on_scan_type_changed, this);
        m_endiannessTypeComboBox->Bind(wxEVT_COMBOBOX, &MainView::on_endianness_type_changed, this);
        m_alignmentCheckBox->Bind(wxEVT_CHECKBOX, &MainView::on_alignment_enabled_changed, this);
        m_alignmentValue->Bind(wxEVT_SPINCTRL, &MainView::on_alignment_value_changed, this);

        Bind(wxEVT_TIMER, &MainView::on_process_validity_check, this, m_processValidityCheck->GetId());
        Bind(wxEVT_TIMER, &MainView::on_scan_progress_update, this, m_scanProgressTimer->GetId());
        Bind(wxEVT_CLOSE_WINDOW, &MainView::on_close, this);

        m_auiToolBar->Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              m_viewModel->open_process_list_window();
          },
          StandardMenuIds::MainViewIds::ID_PROCESS_LIST);

        m_auiToolBar->Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              if (!m_viewModel->is_process_opened())
              {
                  wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.noProcessOpenedMessage")),
                               wxString::FromUTF8(m_languageService.fetch_translation("general.error")), wxICON_ERROR | wxOK);
                  return;
              }

              m_viewModel->kill_process();
              handle_process_closed();
          },
          StandardMenuIds::MainViewIds::ID_KILL_PROCESS);

        m_auiToolBar->Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              m_viewModel->open_settings_window();
          },
          StandardMenuIds::MainViewIds::ID_SETTINGS);

        m_auiToolBar->Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              m_viewModel->open_debugger_window();
          },
          StandardMenuIds::MainViewIds::ID_DEBUGGER);

        m_auiToolBar->Bind(
            wxEVT_MENU,
            [this](wxCommandEvent&)
            {
                m_viewModel->open_injector_window();
            },
            StandardMenuIds::MainViewIds::ID_INJECTOR);

        m_auiToolBar->Bind(wxEVT_MENU, &MainView::on_activity_clicked, this, StandardMenuIds::MainViewIds::ID_ANALYTICS);

        m_auiToolBar->Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              std::vector<std::string> extensions{};
              m_viewModel->get_file_executable_extensions(extensions);

              std::string extensionFilter{};
              if (!extensions.empty())
              {
                  auto wildcarded = extensions | std::views::transform(
                                                   [](const std::string& ext)
                                                   {
                                                       return "*" + ext;
                                                   });
                  extensionFilter = fmt::format("Executable files|{}|All files|*.*", fmt::join(wildcarded, ";"));
              }
              else
              {
                  extensionFilter = "All files|*.*";
              }

              wxFileDialog fileDialog(this, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.selectExecutable")), wxEmptyString, wxEmptyString,
                                      wxString::FromUTF8(extensionFilter), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

              if (fileDialog.ShowModal() == wxID_OK)
              {
                  // TODO: Setup project file structures and do the handling here!!!
                  [[maybe_unused]] wxString path = fileDialog.GetPath();
              }
          },
          StandardMenuIds::MainViewIds::ID_NEW_PROCESS);

        m_auiToolBar->Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              if (!m_viewModel->is_process_opened())
              {
                  wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.noProcessOpenedMessage")),
                               wxString::FromUTF8(m_languageService.fetch_translation("general.error")), wxICON_ERROR | wxOK);
                  return;
              }

              m_viewModel->close_process_state();
              handle_process_closed();
          },
          StandardMenuIds::MainViewIds::ID_CLOSE_PROCESS);

        Bind(
          wxEVT_MENU,
          [this](wxCommandEvent&)
          {
              show_about_dialog();
          },
          StandardMenuIds::MainViewIds::ID_HELP_ABOUT);
    }

    void MainView::update_view(const ViewUpdateFlags flags)
    {
        if (has_flag(flags, ViewUpdateFlags::PROCESS_INFO))
        {
            m_processInformationAndStatusText->SetLabel(m_viewModel->get_process_information());
            if (m_viewModel->is_process_opened())
            {
                set_control_status(ControlStatus::PROCESS_OPENED);
                m_processValidityCheck->Start(StandardWidgetValues::TIMER_INTERVAL_MS);
                m_savedAddressesPanel->start_auto_refresh();
            }
        }

        if (has_flag(flags, ViewUpdateFlags::SCAN_PROGRESS))
        {
            const auto progress = m_viewModel->get_scan_progress();
            constexpr std::int64_t GAUGE_MAX = 10000;
            if (progress.total > 0)
            {
                const int scaledTotal = static_cast<int>(std::min(progress.total, GAUGE_MAX));
                const int scaledCurrent = static_cast<int>(progress.current * scaledTotal / progress.total);
                m_scanProgressBar->SetRange(scaledTotal);
                m_scanProgressBar->SetValue(std::min(scaledCurrent, scaledTotal));
            }
        }

        if (has_flag(flags, ViewUpdateFlags::SCANNED_VALUES))
        {
            const auto count = m_viewModel->get_scanned_values_count();
            m_scannedValuesAmountText->SetLabel(wxString::Format(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.valuesFound")), count));
        }

        if (has_flag(flags, ViewUpdateFlags::BUTTON_STATES))
        {
            m_initialScanButton->Enable(m_viewModel->is_initial_scan_ready());
            m_nextScanButton->Enable(m_viewModel->is_next_scan_ready());
            m_undoScanButton->Enable(m_viewModel->is_undo_scan_ready());
        }

        if (has_flag(flags, ViewUpdateFlags::INPUT_VISIBILITY))
        {
            update_input_visibility_based_on_scan_type(m_scanTypeComboBox->GetSelection());
        }

        if (has_flag(flags, ViewUpdateFlags::DATATYPES))
        {
            const int storedSelection = m_viewModel->get_value_type_index();
            m_valueTypeComboBox->Clear();

            const auto valueTypeNames = m_viewModel->get_value_type_names();
            for (const auto& typeName : valueTypeNames)
            {
                m_valueTypeComboBox->Append(wxString::FromUTF8(typeName));
            }

            if (storedSelection >= 0 && storedSelection < static_cast<int>(valueTypeNames.size()))
            {
                m_valueTypeComboBox->SetSelection(storedSelection);
            }
            else if (!valueTypeNames.empty())
            {
                m_valueTypeComboBox->SetSelection(2);
                m_viewModel->set_value_type_index(2);
            }

            update_view(ViewUpdateFlags::SCAN_MODES);
        }

        if (has_flag(flags, ViewUpdateFlags::SCAN_MODES))
        {
            const int storedScanModeSelection = m_viewModel->get_scan_type_index();
            m_scanTypeComboBox->Clear();

            const auto scanModes = m_viewModel->get_scan_mode_names();
            for (const auto& modeName : scanModes)
            {
                m_scanTypeComboBox->Append(wxString::FromUTF8(modeName));
            }

            if (storedScanModeSelection >= 0 && storedScanModeSelection < static_cast<int>(scanModes.size()))
            {
                m_scanTypeComboBox->SetSelection(storedScanModeSelection);
            }
            else if (!scanModes.empty())
            {
                m_scanTypeComboBox->SetSelection(0);
                m_viewModel->set_scan_type_index(0);
            }

            update_input_visibility_based_on_scan_type(m_scanTypeComboBox->GetSelection());
        }

        if (flags != ViewUpdateFlags::NONE)
        {
            Layout();
        }
    }

    void MainView::restore_ui_state()
    {
        const int valueTypeIndex = m_viewModel->get_value_type_index();
        if (valueTypeIndex >= 0 && valueTypeIndex < static_cast<int>(m_valueTypeComboBox->GetCount()))
        {
            m_valueTypeComboBox->SetSelection(valueTypeIndex);
        }

        const int scanTypeIndex = m_viewModel->get_scan_type_index();
        if (scanTypeIndex >= 0 && scanTypeIndex < static_cast<int>(m_scanTypeComboBox->GetCount()))
        {
            m_scanTypeComboBox->SetSelection(scanTypeIndex);
        }

        const int endiannessTypeIndex = m_viewModel->get_endianness_type_index();
        if (endiannessTypeIndex >= 0 && endiannessTypeIndex < static_cast<int>(m_endiannessTypeComboBox->GetCount()))
        {
            m_endiannessTypeComboBox->SetSelection(endiannessTypeIndex);
        }

        m_hexadecimalValueCheckBox->SetValue(m_viewModel->is_hexadecimal());
        m_alignmentCheckBox->SetValue(m_viewModel->is_alignment_enabled());

        m_alignmentValue->SetValue(m_viewModel->get_alignment_value());
        m_alignmentValue->Enable(m_viewModel->is_alignment_enabled());

        update_input_visibility_based_on_scan_type(scanTypeIndex);
    }

    void MainView::handle_process_closed() const
    {
        m_processInformationAndStatusText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.noProcessSelected")));
        m_initialScanButton->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.buttons.initialScan")));
        m_processValidityCheck->Stop();
        m_savedAddressesPanel->stop_auto_refresh();
        m_scannedValuesPanel->stop_auto_refresh();
        set_control_status(ControlStatus::NO_PROCESS_OPENED);
        m_viewModel->close_process_state();
    }

    void MainView::update_input_visibility_based_on_scan_type([[maybe_unused]] const int scanTypeIndex)
    {
        const auto valueType = m_viewModel->get_current_value_type();

        if (Scanner::is_string_type(valueType))
        {
            m_valueInputText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.value")));
            m_valueInputTextControl->Show(true);
            m_valueInputText2->Show(false);
            m_valueInputTextControl2->Show(false);
            Layout();
            return;
        }

        const auto actualMode = m_viewModel->get_actual_numeric_scan_mode();
        const bool needsInput = Scanner::scan_mode_needs_input(actualMode);
        const bool isInBetween = (actualMode == Scanner::NumericScanMode::Between);

        const auto labelKey = isInBetween ? "mainWindow.ui.betweenValue" : "mainWindow.ui.value";
        m_valueInputText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation(labelKey)));

        m_valueInputTextControl->Show(needsInput);
        m_valueInputText2->Show(isInBetween);
        m_valueInputTextControl2->Show(isInBetween);

        Layout();
    }

    void MainView::set_control_status(const ControlStatus controlStatus) const
    {
        switch (controlStatus)
        {
        case ControlStatus::NO_PROCESS_OPENED:
            m_initialScanButton->Disable();
            m_nextScanButton->Disable();
            m_undoScanButton->Disable();
            m_valueInputTextControl->Disable();
            m_valueInputTextControl2->Disable();
            m_hexadecimalValueCheckBox->Disable();
            m_valueTypeComboBox->Disable();
            m_scanTypeComboBox->Disable();
            m_endiannessTypeComboBox->Disable();
            m_alignmentCheckBox->Disable();
            m_alignmentValue->Disable();
            m_memoryRegionSettingsButton->Disable();
            m_addAddressManuallyButton->Disable();
            break;

        case ControlStatus::PROCESS_OPENED:
        case ControlStatus::INITIAL_SCAN_READY:
            m_initialScanButton->Enable();
            m_nextScanButton->Disable();
            m_undoScanButton->Disable();
            m_valueInputTextControl->Enable();
            m_valueInputTextControl2->Enable();
            m_hexadecimalValueCheckBox->Enable();
            m_valueTypeComboBox->Enable();
            m_scanTypeComboBox->Enable();
            m_endiannessTypeComboBox->Enable();
            m_alignmentCheckBox->Enable();
            m_alignmentValue->Enable();
            m_memoryRegionSettingsButton->Enable();
            m_addAddressManuallyButton->Enable();
            break;
        }
    }

    void MainView::on_initial_scan_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        if (m_viewModel->is_unknown_scan_mode())
        {
            m_viewModel->reset_scan();
            m_initialScanButton->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.buttons.initialScan")));
            m_scannedValuesPanel->stop_auto_refresh();
            m_scannedValuesPanel->clear_list();
            update_view(ViewUpdateFlags::SCAN_MODES | ViewUpdateFlags::BUTTON_STATES | ViewUpdateFlags::SCANNED_VALUES);
            return;
        }

        m_scannedValuesPanel->stop_auto_refresh();
        m_scannedValuesPanel->clear_list();
        m_viewModel->initial_scan();

        if (m_viewModel->is_unknown_scan_mode())
        {
            m_initialScanButton->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.buttons.newScan")));
            update_view(ViewUpdateFlags::SCAN_MODES);
        }

        m_timerReset.reset();
        m_scanProgressTimer->Start();
    }

    void MainView::on_next_scan_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_scannedValuesPanel->stop_auto_refresh();
        m_scannedValuesPanel->clear_list();
        m_viewModel->next_scan();
        m_timerReset.reset();
        m_scanProgressTimer->Start();
    }

    void MainView::on_undo_scan_clicked([[maybe_unused]] wxCommandEvent& event) { m_viewModel->undo_scan(); }

    void MainView::on_value_input_changed([[maybe_unused]] wxCommandEvent& event) { m_viewModel->set_value_input(m_valueInputTextControl->GetValue().ToStdString()); }

    void MainView::on_value_input2_changed([[maybe_unused]] wxCommandEvent& event) { m_viewModel->set_value_input2(m_valueInputTextControl2->GetValue().ToStdString()); }

    void MainView::on_hexadecimal_changed([[maybe_unused]] wxCommandEvent& event) { m_viewModel->set_hexadecimal(m_hexadecimalValueCheckBox->GetValue()); }

    void MainView::on_value_type_changed([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->set_value_type_index(m_valueTypeComboBox->GetSelection());
        update_view(ViewUpdateFlags::SCAN_MODES);
    }

    void MainView::on_scan_type_changed([[maybe_unused]] wxCommandEvent& event)
    {
        const int selection = m_scanTypeComboBox->GetSelection();
        m_viewModel->set_scan_type_index(selection);
        update_input_visibility_based_on_scan_type(selection);
    }

    void MainView::on_endianness_type_changed([[maybe_unused]] wxCommandEvent& event) { m_viewModel->set_endianness_type_index(m_endiannessTypeComboBox->GetSelection()); }

    void MainView::on_alignment_enabled_changed([[maybe_unused]] wxCommandEvent& event) { m_viewModel->set_alignment_enabled(m_alignmentCheckBox->GetValue()); }

    void MainView::on_alignment_value_changed([[maybe_unused]] wxSpinEvent& event) { m_viewModel->set_alignment_value(m_alignmentValue->GetValue()); }

    void MainView::on_add_address_manually_clicked([[maybe_unused]] wxCommandEvent& event) { m_viewModel->add_address_manually(); }

    void MainView::on_memory_region_settings_clicked([[maybe_unused]] wxCommandEvent& event) { m_viewModel->open_memory_region_settings(); }

    void MainView::on_open_project([[maybe_unused]] wxCommandEvent& event) { m_viewModel->open_project(); }

    void MainView::on_exit([[maybe_unused]] wxCommandEvent& event) { m_viewModel->exit_application(); }

    void MainView::on_activity_clicked([[maybe_unused]] wxCommandEvent& event) { m_viewModel->open_activity_window(); }

    void MainView::on_process_validity_check([[maybe_unused]] wxTimerEvent& event)
    {
        if (!m_viewModel->is_process_opened())
        {
            handle_process_closed();
        }
    }

    void MainView::on_scan_progress_update([[maybe_unused]] wxTimerEvent& event)
    {
        m_viewModel->update_scan_progress();
        const auto progress = m_viewModel->get_scan_progress();
        update_view(ViewUpdateFlags::SCAN_PROGRESS | ViewUpdateFlags::SCANNED_VALUES);

        const bool scanComplete = (progress.current >= progress.total && progress.total > 0) && m_viewModel->is_scan_complete();

        if (scanComplete)
        {
            m_scanProgressTimer->Stop();
            m_viewModel->finalize_scan_results();
            update_view(ViewUpdateFlags::SCANNED_VALUES | ViewUpdateFlags::BUTTON_STATES);

            m_scannedValuesPanel->refresh_list();
            m_scannedValuesPanel->start_auto_refresh();
        }

        m_timerReset.call(
          [this]
          {
              m_scanProgressTimer->Start(1);
          });
    }

    void MainView::on_close([[maybe_unused]] wxCloseEvent& event)
    {
        if (m_processValidityCheck)
        {
            m_processValidityCheck->Stop();
        }
        if (m_scanProgressTimer)
        {
            m_scanProgressTimer->Stop();
        }
        m_savedAddressesPanel->stop_auto_refresh();
        m_scannedValuesPanel->stop_auto_refresh();

        m_auiManager.UnInit();
        m_viewModel->exit_application();

        Destroy();
    }

    void MainView::show_about_dialog()
    {
        AboutInfo aboutInfo;
        aboutInfo.description = m_languageService.fetch_translation("aboutWindow.description");

        aboutInfo.add_developer("PHTNC<>", m_languageService.fetch_translation("aboutWindow.roles.leadDeveloper"))
          .add_tester("Dragon", "Testing and Feedback for Windows")
          .add_special_thanks("wxWidgets Team", m_languageService.fetch_translation("aboutWindow.thanks.uiFramework"))
          .add_special_thanks("Open Source Community", m_languageService.fetch_translation("aboutWindow.thanks.community"));

        AboutView aboutDialog(this, m_languageService, aboutInfo);
        aboutDialog.ShowModal();
    }

    void MainView::set_pointer_scan_callback(CustomWidgets::SavedAddressesControl::PointerScanCallback callback) const
    {
        if (m_savedAddressesPanel)
        {
            m_savedAddressesPanel->set_pointer_scan_callback(std::move(callback));
        }
    }

    void MainView::set_view_in_disassembly_callback(CustomWidgets::SavedAddressesControl::ViewInDisassemblyCallback callback) const
    {
        if (m_savedAddressesPanel)
        {
            m_savedAddressesPanel->set_view_in_disassembly_callback(std::move(callback));
        }
    }

    void MainView::set_find_access_callback(CustomWidgets::SavedAddressesControl::FindAccessCallback callback) const
    {
        if (m_savedAddressesPanel)
        {
            m_savedAddressesPanel->set_find_access_callback(std::move(callback));
        }
    }
} // namespace Vertex::View
