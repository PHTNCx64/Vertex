//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/gauge.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/statbox.h>
#include <wx/aui/aui.h>

#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/gui/iconmanager/iconmanager.hh>
#include <vertex/language/language.hh>
#include <vertex/customwidgets/scannedvaluespanel.hh>
#include <vertex/customwidgets/savedaddressespanel.hh>
#include <vertex/event/eventid.hh>
#include <vertex/event/vertexevent.hh>

namespace Vertex::View
{
    class MainView final : public wxFrame
    {
    public:
        MainView(
            const wxString& title,
            std::unique_ptr<ViewModel::MainViewModel> viewModel,
            Language::ILanguage& languageService,
            Gui::IIconManager& iconManager
        );

        void set_pointer_scan_callback(CustomWidgets::SavedAddressesControl::PointerScanCallback callback) const;
        void set_view_in_disassembly_callback(CustomWidgets::SavedAddressesControl::ViewInDisassemblyCallback callback) const;
        void set_find_access_callback(CustomWidgets::SavedAddressesControl::FindAccessCallback callback) const;

    private:
        enum class ControlStatus
        {
            NO_PROCESS_OPENED,
            PROCESS_OPENED,
            INITIAL_SCAN_READY,
        };

        void create_controls();
        void layout_controls();
        void bind_events();
        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        void update_view(ViewUpdateFlags flags = ViewUpdateFlags::ALL);
        void set_control_status(ControlStatus controlStatus) const;
        void update_input_visibility_based_on_scan_type(int scanTypeIndex);
        void handle_process_closed() const;
        void restore_ui_state();

        void on_initial_scan_clicked(wxCommandEvent& event);
        void on_next_scan_clicked(wxCommandEvent& event);
        void on_undo_scan_clicked(wxCommandEvent& event);
        void on_value_input_changed(wxCommandEvent& event);
        void on_value_input2_changed(wxCommandEvent& event);
        void on_hexadecimal_changed(wxCommandEvent& event);
        void on_value_type_changed(wxCommandEvent& event);
        void on_scan_type_changed(wxCommandEvent& event);
        void on_endianness_type_changed(wxCommandEvent& event);
        void on_alignment_enabled_changed(wxCommandEvent& event);
        void on_alignment_value_changed(wxSpinEvent& event);
        void on_add_address_manually_clicked(wxCommandEvent& event);
        void on_memory_region_settings_clicked(wxCommandEvent& event);

        void on_open_project(wxCommandEvent& event);
        void on_exit(wxCommandEvent& event);

        void on_close(wxCloseEvent& event);

        void on_process_validity_check(wxTimerEvent& event);
        void on_scan_progress_update(wxTimerEvent& event);

        void on_activity_clicked(wxCommandEvent& event);

        void show_about_dialog();

        wxPanel* m_mainPanel{};
        wxBoxSizer* m_mainBoxSizer{};
        wxBoxSizer* m_buttonSizer{};
        wxBoxSizer* m_valuesSizer{};
        wxMenuBar* m_menuBar{};
        wxAuiManager m_auiManager{};
        wxAuiToolBar* m_auiToolBar{};
        wxFlexGridSizer* m_scannedValuesAndScanOptionsSizer{};
        wxBoxSizer* m_scanOptionsWithButtonsSizer{};
        wxBoxSizer* m_topSectionSizer{};
        wxStaticText* m_processInformationAndStatusText{};
        wxGauge* m_scanProgressBar{};

        wxButton* m_initialScanButton{};
        wxButton* m_nextScanButton{};
        wxButton* m_undoScanButton{};

        wxStaticText* m_scannedValuesAmountText{};
        CustomWidgets::ScannedValuesPanel* m_scannedValuesPanel{};

        wxStaticBoxSizer* m_scanOptionsSizer{};
        wxStaticBox* m_scanOptionsStaticBox{};

        wxStaticText* m_valueInputText{};
        wxTextCtrl* m_valueInputTextControl{};
        wxBoxSizer* m_valueInputSizer{};
        wxBoxSizer* m_valueInputControlsSizer{};
        wxStaticText* m_valueInputText2{};
        wxTextCtrl* m_valueInputTextControl2{};

        wxBoxSizer* m_hexadecimalValueSizer{};
        wxCheckBox* m_hexadecimalValueCheckBox{};

        wxBoxSizer* m_valueTypeSizer{};
        wxStaticText* m_valueTypeText{};
        wxComboBox* m_valueTypeComboBox{};

        wxBoxSizer* m_scanTypeSizer{};
        wxStaticText* m_scanTypeText{};
        wxComboBox* m_scanTypeComboBox{};

        wxBoxSizer* m_endiannessTypeSizer{};
        wxStaticText* m_endiannessTypeText{};
        wxComboBox* m_endiannessTypeComboBox{};

        wxBoxSizer* m_alignmentTopSizer{};
        wxBoxSizer* m_alignmentBoxSizer{};
        wxCheckBox* m_alignmentCheckBox{};
        wxStaticText* m_alignmentInformationText{};
        wxSpinCtrl* m_alignmentValue{};

        wxBoxSizer* m_memoryRegionSettingsSizer{};
        wxButton* m_memoryRegionSettingsButton{};

        wxBoxSizer* m_minAddressSizer{};
        wxStaticText* m_minAddressLabel{};
        wxTextCtrl* m_minAddressTextControl{};
        wxBoxSizer* m_maxAddressSizer{};
        wxStaticText* m_maxAddressLabel{};
        wxTextCtrl* m_maxAddressTextControl{};

        wxButton* m_addAddressManuallyButton{};

        CustomWidgets::SavedAddressesPanel* m_savedAddressesPanel{};

        wxMenu* m_fileMenu{};
        wxMenu* m_helpMenu{};

        wxTimer* m_processValidityCheck{};
        wxTimer* m_scanProgressTimer{};

        std::unique_ptr<ViewModel::MainViewModel> m_viewModel{};
        Language::ILanguage& m_languageService;
        Gui::IIconManager& m_iconManager;

        ResettableCallOnce m_timerReset{};
    };
}
