//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/valueeditdialog.hh>

namespace Vertex::CustomWidgets
{
    ValueEditDialog::ValueEditDialog(
        wxWindow* parent,
        const wxString& title,
        const wxString& label,
        const wxString& initialValue
    )
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        create_controls(label, initialValue);

        SetMinSize(FromDIP(wxSize(300, 120)));
        Fit();
        CenterOnParent();

        m_textCtrl->SetFocus();
        m_textCtrl->SelectAll();
    }

    void ValueEditDialog::create_controls(const wxString& label, const wxString& initialValue)
    {
        auto* mainSizer = new wxBoxSizer(wxVERTICAL);

        auto* labelText = new wxStaticText(this, wxID_ANY, label);
        mainSizer->Add(labelText, 0, wxALL, 10);

        m_textCtrl = new wxTextCtrl(this, wxID_ANY, initialValue,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxTE_PROCESS_ENTER);
        mainSizer->Add(m_textCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_okButton = new wxButton(this, wxID_OK, "OK");
        m_cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");

        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_okButton, 0, wxRIGHT, 5);
        buttonSizer->Add(m_cancelButton, 0);

        mainSizer->AddSpacer(10);
        mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 10);

        SetSizer(mainSizer);

        m_okButton->Bind(wxEVT_BUTTON, &ValueEditDialog::on_ok, this);
        m_cancelButton->Bind(wxEVT_BUTTON, &ValueEditDialog::on_cancel, this);
        m_textCtrl->Bind(wxEVT_TEXT_ENTER, &ValueEditDialog::on_text_enter, this);
    }

    wxString ValueEditDialog::get_value() const
    {
        return m_textCtrl->GetValue();
    }

    void ValueEditDialog::on_ok([[maybe_unused]] wxCommandEvent& event)
    {
        EndModal(wxID_OK);
    }

    void ValueEditDialog::on_cancel([[maybe_unused]] wxCommandEvent& event)
    {
        EndModal(wxID_CANCEL);
    }

    void ValueEditDialog::on_text_enter([[maybe_unused]] wxCommandEvent& event)
    {
        EndModal(wxID_OK);
    }
}
