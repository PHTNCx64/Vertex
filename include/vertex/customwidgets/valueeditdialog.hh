//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <string>

namespace Vertex::CustomWidgets
{
    class ValueEditDialog final : public wxDialog
    {
    public:
        explicit ValueEditDialog(
            wxWindow* parent,
            const wxString& title,
            const wxString& label,
            const wxString& initialValue
        );

        [[nodiscard]] wxString get_value() const;

    private:
        void create_controls(const wxString& label, const wxString& initialValue);
        void on_ok(wxCommandEvent& event);
        void on_cancel(wxCommandEvent& event);
        void on_text_enter(wxCommandEvent& event);

        wxTextCtrl* m_textCtrl{};
        wxButton* m_okButton{};
        wxButton* m_cancelButton{};
    };
}
