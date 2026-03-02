//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/breakpointconditiondialog.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>

namespace Vertex::View::Debugger
{
    BreakpointConditionDialog::BreakpointConditionDialog(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const ::Vertex::Debugger::Breakpoint& breakpoint
    )
        : wxDialog(parent, wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("debugger.breakpoints.conditionDialogTitle")),
                   wxDefaultPosition,
                   wxSize(FromDIP(BreakpointConditionDialogValues::DIALOG_WIDTH),
                          FromDIP(BreakpointConditionDialogValues::DIALOG_HEIGHT)),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_languageService(languageService)
        , m_breakpoint(breakpoint)
    {
        create_controls();
        layout_controls();
        bind_events();
        update_field_states();
    }

    void BreakpointConditionDialog::create_controls()
    {
        m_conditionTypeChoice = new wxChoice(this, wxID_ANY);
        m_conditionTypeChoice->Append(wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.conditionNone")));
        m_conditionTypeChoice->Append(wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.conditionExpression")));
        m_conditionTypeChoice->Append(wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.conditionHitCountEqual")));
        m_conditionTypeChoice->Append(wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.conditionHitCountGreater")));
        m_conditionTypeChoice->Append(wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.conditionHitCountMultiple")));

        int selectionIndex{};
        switch (m_breakpoint.conditionType)
        {
            case VERTEX_BP_COND_EXPRESSION:        selectionIndex = 1; break;
            case VERTEX_BP_COND_HIT_COUNT_EQUAL:   selectionIndex = 2; break;
            case VERTEX_BP_COND_HIT_COUNT_GREATER: selectionIndex = 3; break;
            case VERTEX_BP_COND_HIT_COUNT_MULTIPLE: selectionIndex = 4; break;
            default:                                selectionIndex = 0; break;
        }
        m_conditionTypeChoice->SetSelection(selectionIndex);

        m_expressionInput = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(m_breakpoint.condition));

        const int hitCountValue = m_breakpoint.hitCountTarget > 0
            ? static_cast<int>(m_breakpoint.hitCountTarget) : 1;
        m_hitCountTargetSpin = new wxSpinCtrl(this, wxID_ANY, wxString::Format("%d", hitCountValue),
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 999999, hitCountValue);

        m_currentHitCountLabel = new wxStaticText(this, wxID_ANY,
            wxString::Format("%u", m_breakpoint.hitCount));

        m_logMessageCheck = new wxCheckBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.logMessage")));
        m_logMessageCheck->Enable(false);

        m_continueExecutionCheck = new wxCheckBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.continueExecution")));
        m_continueExecutionCheck->Enable(false);

        m_notImplementedLabel = new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.notImplementedWarning")));
        m_notImplementedLabel->SetForegroundColour(wxColour(0xD4, 0xA0, 0x17));

        m_okButton = new wxButton(this, wxID_OK,
            wxString::FromUTF8(m_languageService.fetch_translation("general.ok")));
        m_cancelButton = new wxButton(this, wxID_CANCEL,
            wxString::FromUTF8(m_languageService.fetch_translation("general.cancel")));
    }

    void BreakpointConditionDialog::layout_controls()
    {
        auto* topSizer = new wxBoxSizer(wxVERTICAL);

        m_mainSizer = new wxFlexGridSizer(2, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->AddGrowableCol(1, 1);

        m_mainSizer->Add(new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.conditionType"))),
            0, wxALIGN_CENTER_VERTICAL);
        m_mainSizer->Add(m_conditionTypeChoice, 1, wxEXPAND);

        m_mainSizer->Add(new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.expressionLabel"))),
            0, wxALIGN_CENTER_VERTICAL);
        m_mainSizer->Add(m_expressionInput, 1, wxEXPAND);

        m_mainSizer->Add(new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.hitCountTargetLabel"))),
            0, wxALIGN_CENTER_VERTICAL);
        m_mainSizer->Add(m_hitCountTargetSpin, 1, wxEXPAND);

        m_mainSizer->Add(new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.currentHitCountLabel"))),
            0, wxALIGN_CENTER_VERTICAL);
        m_mainSizer->Add(m_currentHitCountLabel, 0, wxALIGN_CENTER_VERTICAL);

        topSizer->Add(m_mainSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::BORDER_TWICE);

        auto* actionsBox = new wxStaticBoxSizer(wxVERTICAL,
            this, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.actionsLabel")));
        actionsBox->Add(m_logMessageCheck, 0, wxALL, StandardWidgetValues::STANDARD_BORDER);
        actionsBox->Add(m_continueExecutionCheck, 0, wxALL, StandardWidgetValues::STANDARD_BORDER);
        topSizer->Add(actionsBox, 0, wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::BORDER_TWICE);

        topSizer->Add(m_notImplementedLabel, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, StandardWidgetValues::BORDER_TWICE);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_okButton, 0, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_cancelButton, 0);
        topSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::BORDER_TWICE);

        SetSizer(topSizer);
    }

    void BreakpointConditionDialog::bind_events()
    {
        m_conditionTypeChoice->Bind(wxEVT_CHOICE, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            update_field_states();
        });
    }

    void BreakpointConditionDialog::update_field_states()
    {
        const int selection = m_conditionTypeChoice->GetSelection();
        m_expressionInput->Enable(selection == 1);
        m_hitCountTargetSpin->Enable(selection >= 2);
        m_notImplementedLabel->Show(selection == 1);
        m_okButton->Enable(selection != 1);
        Layout();
    }

    int BreakpointConditionDialog::get_condition_type() const
    {
        return m_conditionTypeChoice->GetSelection();
    }

    wxString BreakpointConditionDialog::get_expression() const
    {
        return m_expressionInput->GetValue();
    }

    int BreakpointConditionDialog::get_hit_count_target() const
    {
        return m_hitCountTargetSpin->GetValue();
    }
}
