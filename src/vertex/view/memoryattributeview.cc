//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/memoryattributeview.hh>
#include <wx/msgdlg.h>
#include <wx/app.h>

namespace Vertex::View
{
    MemoryAttributeView::MemoryAttributeView(std::unique_ptr<ViewModel::MemoryAttributeViewModel> viewModel,
                                             Language::ILanguage& languageService)
        : wxDialog(wxTheApp->GetTopWindow(), wxID_ANY,
                  wxString::FromUTF8(languageService.fetch_translation("memoryAttributeWindow.title"))),
          m_viewModel(std::move(viewModel)),
          m_languageService(languageService)
    {
        m_viewModel->set_event_callback([this](const Event::EventId eventId, const Event::VertexEvent& event)
        {
            vertex_event_callback(eventId, event);
        });

        create_ui_elements();
        layout_ui_elements();

        SetSizer(m_mainSizer);
        wxTopLevelWindowBase::Layout();
        wxTopLevelWindowBase::Fit();
    }

    void MemoryAttributeView::create_ui_elements()
    {
        m_protectionBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("memoryAttributeWindow.protectionGroup")));
        m_stateBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("memoryAttributeWindow.stateGroup")));
        m_typeBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("memoryAttributeWindow.typeGroup")));

        m_protectionGroupSizer = new wxStaticBoxSizer(m_protectionBox, wxVERTICAL);
        m_stateGroupSizer = new wxStaticBoxSizer(m_stateBox, wxVERTICAL);
        m_typeGroupSizer = new wxStaticBoxSizer(m_typeBox, wxVERTICAL);

        m_okButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("general.ok")));
        m_cancelButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("general.cancel")));

        m_okButton->Bind(wxEVT_BUTTON, &MemoryAttributeView::on_ok_clicked, this);
        m_cancelButton->Bind(wxEVT_BUTTON, &MemoryAttributeView::on_cancel_clicked, this);
    }

    void MemoryAttributeView::layout_ui_elements()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_memoryAttributeGroupSizer = new wxBoxSizer(wxHORIZONTAL);

        m_memoryAttributeGroupSizer->Add(m_protectionGroupSizer, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_memoryAttributeGroupSizer->Add(m_stateGroupSizer, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_memoryAttributeGroupSizer->Add(m_typeGroupSizer, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_mainSizer->Add(m_memoryAttributeGroupSizer, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_buttonSizer->AddStretchSpacer();
        m_buttonSizer->Add(m_okButton, 0, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->Add(m_cancelButton, 0, 0, 0);

        m_mainSizer->Add(m_buttonSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
    }

    void MemoryAttributeView::load_memory_attributes_from_viewmodel()
    {
        if (!m_viewModel->has_options())
        {
            show_error_and_close(
                m_languageService.fetch_translation("memoryAttributeWindow.noOptionsAvailable"));
            return;
        }

        m_loadedOptions.clear();
        if (!m_viewModel->get_memory_attribute_options(m_loadedOptions))
        {
            show_error_and_close(
                m_languageService.fetch_translation("memoryAttributeWindow.failedToLoadOptions"));
            return;
        }

        for (const auto& option : m_loadedOptions)
        {
            add_checkbox_for_attribute(option);
        }

        if (m_mainSizer)
        {
            m_mainSizer->Layout();
        }
    }

    void MemoryAttributeView::add_checkbox_for_attribute(const Model::MemoryAttributeOptionData& option)
    {
        wxStaticBox* parent{};
        wxStaticBoxSizer* sizer{};
        std::vector<wxCheckBox*>* checkboxList{};

        switch (option.type)
        {
        case VERTEX_PROTECTION:
            parent = m_protectionBox;
            sizer = m_protectionGroupSizer;
            checkboxList = &m_protectionCheckboxes;
            break;

        case VERTEX_STATE:
            parent = m_stateBox;
            sizer = m_stateGroupSizer;
            checkboxList = &m_stateCheckboxes;
            break;

        case VERTEX_TYPE:
            parent = m_typeBox;
            sizer = m_typeGroupSizer;
            checkboxList = &m_typeCheckboxes;
            break;

        default:
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("memoryAttributeWindow.invalidAttributeType")) +
                    ": " + wxString::FromUTF8(option.name),
                wxString::FromUTF8(m_languageService.fetch_translation("general.warning")),
                wxOK | wxICON_WARNING);
            return;
        }

        auto* checkbox = new wxCheckBox(parent, wxID_ANY, wxString::FromUTF8(option.name));
        checkbox->SetValue(option.currentState);
        m_stateFunctions.push_back(option.stateFunction);
        sizer->Add(checkbox, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        checkboxList->push_back(checkbox);
    }

    void MemoryAttributeView::show_error_and_close(const std::string_view message)
    {
        wxMessageBox(wxString::FromUTF8(std::string{message}),
                    wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                    wxOK | wxICON_ERROR);
        Hide();
    }

    void MemoryAttributeView::vertex_event_callback([[maybe_unused]] Event::EventId eventId, [[maybe_unused]] const Event::VertexEvent& event)
    {
        std::ignore = toggle_view();
    }

    bool MemoryAttributeView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        Show(true);
        Raise();
        return true;
    }

    void MemoryAttributeView::reset_attribute_checkboxes()
    {
        auto clearGroup = [](std::vector<wxCheckBox*>& checkboxes, wxStaticBoxSizer* sizer)
        {
            for (auto* cb : checkboxes)
            {
                if (sizer && cb)
                {
                    sizer->Detach(cb);
                }
                if (cb)
                {
                    cb->Destroy();
                }
            }
            checkboxes.clear();
        };

        clearGroup(m_protectionCheckboxes, m_protectionGroupSizer);
        clearGroup(m_stateCheckboxes, m_stateGroupSizer);
        clearGroup(m_typeCheckboxes, m_typeGroupSizer);

        m_stateFunctions.clear();
        m_loadedOptions.clear();
    }

    bool MemoryAttributeView::Show(const bool show)
    {
        if (show)
        {
            reset_attribute_checkboxes();
            load_memory_attributes_from_viewmodel();
        }

        const bool result = wxDialog::Show(show);

        if (show && m_mainSizer)
        {
            m_mainSizer->Layout();
            wxTopLevelWindowBase::Fit();
        }

        return result;
    }

    void MemoryAttributeView::on_ok_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        apply_checkbox_states();
        Hide();
    }

    void MemoryAttributeView::on_cancel_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        Hide();
    }

    void MemoryAttributeView::apply_checkbox_states()
    {
        std::vector<wxCheckBox*> allCheckboxes;
        allCheckboxes.insert(allCheckboxes.end(), m_protectionCheckboxes.begin(), m_protectionCheckboxes.end());
        allCheckboxes.insert(allCheckboxes.end(), m_stateCheckboxes.begin(), m_stateCheckboxes.end());
        allCheckboxes.insert(allCheckboxes.end(), m_typeCheckboxes.begin(), m_typeCheckboxes.end());

        for (std::size_t i = 0; i < allCheckboxes.size() && i < m_stateFunctions.size(); ++i)
        {
            if (allCheckboxes[i])
            {
                const bool checkboxState = allCheckboxes[i]->GetValue();

                if (i < m_loadedOptions.size())
                {
                    m_loadedOptions[i].currentState = checkboxState;
                }

                if (m_stateFunctions[i])
                {
                    m_stateFunctions[i](checkboxState);
                }
            }
        }

        // TODO: Handle
        std::ignore = m_viewModel->save_memory_attribute_states(m_loadedOptions);
    }
}
