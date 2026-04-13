//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/choice.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/sizer.h>

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class BreakpointConditionDialog final : public wxDialog
    {
    public:
        BreakpointConditionDialog(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const ::Vertex::Debugger::Breakpoint& breakpoint
        );

        [[nodiscard]] int get_condition_type() const;
        [[nodiscard]] wxString get_expression() const;
        [[nodiscard]] int get_hit_count_target() const;

    private:
        void create_controls();
        void layout_controls();
        void bind_events();
        void update_field_states();
        void on_ok_clicked(wxCommandEvent& event);
        [[nodiscard]] bool is_expression_valid(const wxString& expression) const;

        Language::ILanguage& m_languageService;
        ::Vertex::Debugger::Breakpoint m_breakpoint;

        wxChoice* m_conditionTypeChoice{};
        wxTextCtrl* m_expressionInput{};
        wxSpinCtrl* m_hitCountTargetSpin{};
        wxStaticText* m_currentHitCountLabel{};
        wxCheckBox* m_logMessageCheck{};
        wxCheckBox* m_continueExecutionCheck{};
        wxStaticText* m_notImplementedLabel{};
        wxButton* m_okButton{};
        wxButton* m_cancelButton{};
        wxFlexGridSizer* m_mainSizer{};
    };
}
