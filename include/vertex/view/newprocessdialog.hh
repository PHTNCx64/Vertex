//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/sizer.h>

#include <vertex/language/language.hh>

#include <string>
#include <vector>

namespace Vertex::View
{
    class NewProcessDialog final : public wxDialog
    {
    public:
        NewProcessDialog(
            wxWindow* parent,
            Language::ILanguage& languageService,
            std::vector<std::string> executableExtensions
        );

        [[nodiscard]] wxString get_process_path() const;
        [[nodiscard]] wxString get_start_arguments() const;

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        Language::ILanguage& m_languageService;
        std::vector<std::string> m_executableExtensions;

        wxTextCtrl* m_pathInput{};
        wxButton* m_browseButton{};
        wxTextCtrl* m_argumentsInput{};
        wxButton* m_okButton{};
        wxButton* m_cancelButton{};
    };
}
