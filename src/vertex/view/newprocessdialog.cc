//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/newprocessdialog.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <wx/filedlg.h>
#include <ranges>

namespace Vertex::View
{
    NewProcessDialog::NewProcessDialog(
        wxWindow* parent,
        Language::ILanguage& languageService,
        std::vector<std::string> executableExtensions
    )
        : wxDialog(parent, wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.newProcessDialogTitle")),
                   wxDefaultPosition,
                   wxWindowBase::FromDIP(wxSize(NewProcessDialogValues::DIALOG_WIDTH,
                                                NewProcessDialogValues::DIALOG_HEIGHT),
                                         parent),
                   wxDEFAULT_DIALOG_STYLE)
        , m_languageService(languageService)
        , m_executableExtensions(std::move(executableExtensions))
    {
        create_controls();
        layout_controls();
        bind_events();
        CenterOnParent();
    }

    void NewProcessDialog::create_controls()
    {
        m_pathInput = new wxTextCtrl(this, wxID_ANY);
        m_browseButton = new wxButton(this, wxID_ANY, "...");
        m_argumentsInput = new wxTextCtrl(this, wxID_ANY);
        m_okButton = new wxButton(this, wxID_OK,
            wxString::FromUTF8(m_languageService.fetch_translation("general.ok")));
        m_cancelButton = new wxButton(this, wxID_CANCEL,
            wxString::FromUTF8(m_languageService.fetch_translation("general.cancel")));
    }

    void NewProcessDialog::layout_controls()
    {
        auto* topSizer = new wxBoxSizer(wxVERTICAL);

        auto* gridSizer = new wxFlexGridSizer(2, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::STANDARD_BORDER);
        gridSizer->AddGrowableCol(1, 1);

        gridSizer->Add(new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.newProcessPathLabel"))),
            StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);

        auto* pathRow = new wxBoxSizer(wxHORIZONTAL);
        pathRow->Add(m_pathInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        pathRow->Add(m_browseButton, StandardWidgetValues::NO_PROPORTION);
        gridSizer->Add(pathRow, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

        gridSizer->Add(new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.newProcessArgumentsLabel"))),
            StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
        gridSizer->Add(m_argumentsInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

        topSizer->Add(gridSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::BORDER_TWICE);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_okButton, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_cancelButton, StandardWidgetValues::NO_PROPORTION);
        topSizer->Add(buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::BORDER_TWICE);

        SetSizer(topSizer);
    }

    void NewProcessDialog::bind_events()
    {
        m_browseButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            std::string extensionFilter{};
            if (!m_executableExtensions.empty())
            {
                auto wildcarded = m_executableExtensions | std::views::transform(
                    [](const std::string& ext)
                    {
                        return "*" + ext;
                    });
                extensionFilter = fmt::format("{}|{}|{}|*.*",
                    m_languageService.fetch_translation("mainWindow.ui.newProcessExecutableFiles"),
                    fmt::join(wildcarded, ";"),
                    m_languageService.fetch_translation("mainWindow.ui.newProcessAllFiles"));
            }
            else
            {
                extensionFilter = fmt::format("{}|*.*",
                    m_languageService.fetch_translation("mainWindow.ui.newProcessAllFiles"));
            }

            wxFileDialog fileDialog(this,
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.selectExecutable")),
                wxEmptyString, wxEmptyString,
                wxString::FromUTF8(extensionFilter),
                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

            if (fileDialog.ShowModal() == wxID_OK)
            {
                m_pathInput->SetValue(fileDialog.GetPath());
            }
        });

        m_okButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            if (m_pathInput->GetValue().IsEmpty())
            {
                m_pathInput->SetFocus();
                return;
            }
            EndModal(wxID_OK);
        });

        m_cancelButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            EndModal(wxID_CANCEL);
        });
    }

    wxString NewProcessDialog::get_process_path() const
    {
        return m_pathInput->GetValue();
    }

    wxString NewProcessDialog::get_start_arguments() const
    {
        return m_argumentsInput->GetValue();
    }
}
