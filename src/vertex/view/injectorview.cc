//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/injectorview.hh>

#include <vertex/utility.hh>
#include <vertex/event/types/viewevent.hh>
#include <wx/app.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <ranges>

#include <fmt/format.h>

namespace Vertex::View
{
    InjectorView::InjectorView(Language::ILanguage& languageService, std::unique_ptr<ViewModel::InjectorViewModel> viewModel)
        : wxDialog(wxTheApp->GetTopWindow(),
                   wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("injectorView.ui.title")),
                   wxDefaultPosition,
                   wxSize(FromDIP(StandardWidgetValues::INJECTOR_X_DIP), FromDIP(StandardWidgetValues::INJECTOR_Y_DIP)),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_viewModel{std::move(viewModel)},
          m_languageService{languageService}
    {
        m_viewModel->set_event_callback(
          [this](const Event::EventId eventId, const Event::VertexEvent& event)
          {
              vertex_event_callback(eventId, event);
          });

        create_controls();
        layout_controls();
        bind_events();
    }

    void InjectorView::vertex_event_callback([[maybe_unused]] const Event::EventId eventId, [[maybe_unused]] const Event::VertexEvent& event)
    {
        std::ignore = toggle_view();
    }

    bool InjectorView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        m_viewModel->load_injection_methods();
        m_viewModel->load_library_extensions();
        populate_methods();

        Show();
        Raise();
        return true;
    }

    void InjectorView::populate_methods() const
    {
        m_methodComboBox->Clear();
        m_viewModel->set_selected_method_index(-1);

        for (const auto& [i, method] : m_viewModel->get_injection_methods() | std::views::enumerate)
        {
            m_methodComboBox->Append(wxString::FromUTF8(method.methodName));
        }

        m_descriptionText->SetLabel(EMPTY_STRING);
        m_injectButton->Disable();
    }

    void InjectorView::update_description()
    {
        const auto description = m_viewModel->get_selected_method_description();
        m_descriptionText->SetLabel(wxString::FromUTF8(std::string(description)));
        m_descriptionText->Wrap(GetClientSize().GetWidth() - FromDIP(StandardWidgetValues::BORDER_TWICE * 2));
        Layout();
    }

    wxString InjectorView::build_file_filter() const
    {
        const auto& extensions = m_viewModel->get_library_extensions();

        if (extensions.empty())
        {
            return wxString::FromUTF8(
                fmt::format("{}|*.*",
                    m_languageService.fetch_translation("injectorView.ui.allFiles")));
        }

        wxString wildcards{};
        for (const auto& [i, ext] : extensions | std::views::enumerate)
        {
            if (i > 0)
            {
                wildcards += ";";
            }
            wildcards += wxString::Format("*%s", wxString::FromUTF8(ext));
        }

        return wxString::Format("%s (%s)|%s",
            wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.libraryFiles")),
            wildcards, wildcards);
    }

    void InjectorView::on_inject_clicked()
    {
        const wxString filter = build_file_filter();

        wxFileDialog fileDialog(this,
            wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.selectLibrary")),
            wxEmptyString,
            wxEmptyString,
            filter,
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (fileDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const std::string selectedPath = fileDialog.GetPath().utf8_string();
        const auto status = m_viewModel->inject(selectedPath);

        if (status == StatusCode::STATUS_OK) [[likely]]
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.injectionSuccess")),
                wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.title")),
                wxOK | wxICON_INFORMATION, this);
            Hide();
        }
        else
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.injectionFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
        }
    }

    void InjectorView::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_methodLabel = new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.methodLabel")));
        m_methodComboBox = new wxComboBox(this, wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        m_descriptionLabel = new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.descriptionLabel")));
        m_descriptionText = new wxStaticText(this, wxID_ANY, EMPTY_STRING);
        m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_injectButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("injectorView.ui.inject")));
        m_cancelButton = new wxButton(this, wxID_CANCEL, wxString::FromUTF8(m_languageService.fetch_translation("general.cancel")));
        m_injectButton->Disable();
    }

    void InjectorView::layout_controls()
    {
        m_mainSizer->Add(m_methodLabel, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_methodComboBox, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->AddSpacer(StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_descriptionLabel, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_descriptionText, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->AddStretchSpacer();
        m_buttonSizer->Add(m_injectButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->Add(m_cancelButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
        Layout();
    }

    void InjectorView::bind_events()
    {
        m_methodComboBox->Bind(wxEVT_COMBOBOX,
            [this](const wxCommandEvent& event)
            {
                m_viewModel->set_selected_method_index(event.GetSelection());
                update_description();
                m_injectButton->Enable(event.GetSelection() != wxNOT_FOUND);
            });

        m_injectButton->Bind(wxEVT_BUTTON,
            [this]([[maybe_unused]] wxCommandEvent& event)
            {
                on_inject_clicked();
            });

        m_cancelButton->Bind(wxEVT_BUTTON,
            [this]([[maybe_unused]] wxCommandEvent& event)
            {
                Hide();
            });
    }
}
