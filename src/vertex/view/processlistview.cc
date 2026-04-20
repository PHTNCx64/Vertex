//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/utility.hh>
#include <vertex/event/eventid.hh>
#include <vertex/event/types/processopenevent.hh>
#include <vertex/customwidgets/processlistctrl.hh>
#include <vertex/view/processlistview.hh>

#include <algorithm>
#include <string>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/statbox.h>

namespace Vertex::View
{
    ProcessListView::ProcessListView(Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel)
        : wxDialog(wxTheApp->GetTopWindow(),
                   wxID_ANY,
                   languageService.fetch_translation("processListView.ui.title"),
                   wxDefaultPosition,
                   wxWindowBase::FromDIP(wxSize(StandardWidgetValues::STANDARD_X_DIP, StandardWidgetValues::STANDARD_Y_DIP),
                                         wxTheApp->GetTopWindow()),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX),
          m_viewModel(viewModel),
        m_languageService(languageService)
    {
        m_viewModel->set_event_callback([this](const Event::EventId id, const Event::VertexEvent& event)
        {
            vertex_event_callback(id, event);
        });

        create_controls(m_viewModel);
        layout_controls();

        bind_events();
        restore_ui_state();

        m_attachButton->Disable();
        m_directPidOpenButton->Disable();
    }

    void ProcessListView::create_controls(const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel)
    {
        m_mainPanel = new wxPanel(this, wxID_ANY);

        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_processListInformationTextSizer = new wxBoxSizer(wxHORIZONTAL);
        m_processListInformationText = new wxStaticText(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.informationText")));
        m_processFilteringOptionsBox = new wxStaticBox(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.filterOptionsGroup")));
        m_processFilteringOptionsBoxSizer = new wxStaticBoxSizer(m_processFilteringOptionsBox, wxVERTICAL);
        m_processFilteringInputSizer = new wxBoxSizer(wxHORIZONTAL);
        m_processFilteringTextInformation = new wxStaticText(m_processFilteringOptionsBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.filterLabel")));
        m_processFilteringText = new wxTextCtrl(m_processFilteringOptionsBox, wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxDefaultSize);
        m_radioButtonOptionsSizer = new wxBoxSizer(wxHORIZONTAL);
        m_filterByProcessNameRadioButton = new wxRadioButton(m_processFilteringOptionsBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.filterByProcessName")), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        m_filterByProcessIDRadioButton = new wxRadioButton(m_processFilteringOptionsBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.filterByProcessId")));
        m_filterByProcessOwnerRadioButton = new wxRadioButton(m_processFilteringOptionsBox, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.filterByProcessOwner")));
        m_processListSizer = new wxBoxSizer(wxVERTICAL);
        m_processList = new CustomWidgets::ProcessListControl(m_mainPanel, m_languageService, viewModel);
        m_directPidSizer = new wxBoxSizer(wxHORIZONTAL);
        m_directPidLabel = new wxStaticText(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.directPidLabel")));
        m_directPidInput = new wxTextCtrl(m_mainPanel, wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        m_directPidOpenButton = new wxButton(m_mainPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.directPidOpenButton")));
        m_buttonOptionsSizer = new wxBoxSizer(wxHORIZONTAL);
        m_attachButton = new wxButton(m_mainPanel, wxID_OK, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.openProcessButton")));
        m_cancelButton = new wxButton(m_mainPanel, wxID_CANCEL, wxString::FromUTF8(m_languageService.fetch_translation("processListView.ui.cancelButton")));
        m_taskTimer = new wxTimer(this);
    }

    void ProcessListView::layout_controls()
    {
        m_processListInformationTextSizer->Add(m_processListInformationText, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_processFilteringInputSizer->Add(m_processFilteringTextInformation, StandardWidgetValues::NO_PROPORTION, wxALL | wxALIGN_CENTER_VERTICAL, StandardWidgetValues::STANDARD_BORDER);
        m_processFilteringInputSizer->Add(m_processFilteringText, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_radioButtonOptionsSizer->Add(m_filterByProcessNameRadioButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_radioButtonOptionsSizer->Add(m_filterByProcessIDRadioButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_radioButtonOptionsSizer->Add(m_filterByProcessOwnerRadioButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_processFilteringOptionsBoxSizer->Add(m_processFilteringInputSizer, StandardWidgetValues::NO_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_processFilteringOptionsBoxSizer->Add(m_radioButtonOptionsSizer, StandardWidgetValues::NO_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_processListSizer->Add(m_processList, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_directPidSizer->Add(m_directPidLabel, StandardWidgetValues::NO_PROPORTION, wxALL | wxALIGN_CENTER_VERTICAL, StandardWidgetValues::STANDARD_BORDER);
        m_directPidSizer->Add(m_directPidInput, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::STANDARD_BORDER);
        m_directPidSizer->Add(m_directPidOpenButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_buttonOptionsSizer->AddStretchSpacer();
        m_buttonOptionsSizer->Add(m_attachButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_buttonOptionsSizer->Add(m_cancelButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_processListInformationTextSizer, StandardWidgetValues::NO_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::BORDER_TWICE);
        m_mainSizer->Add(m_processFilteringOptionsBoxSizer, StandardWidgetValues::NO_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::BORDER_TWICE);
        m_mainSizer->Add(m_processListSizer, StandardWidgetValues::STANDARD_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::BORDER_TWICE);
        m_mainSizer->Add(m_directPidSizer, StandardWidgetValues::NO_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::BORDER_TWICE);
        m_mainSizer->Add(m_buttonOptionsSizer, StandardWidgetValues::NO_PROPORTION, wxALL | wxEXPAND, StandardWidgetValues::BORDER_TWICE);

        m_mainPanel->SetSizer(m_mainSizer);

        auto* dialogSizer = new wxBoxSizer(wxVERTICAL);
        dialogSizer->Add(m_mainPanel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        SetSizer(dialogSizer);
    }

    void ProcessListView::bind_events()
    {
        Bind(wxEVT_SHOW, &ProcessListView::on_show, this);

        Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            std::ignore = toggle_view();
        }, m_cancelButton->GetId());

        Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            m_viewModel->open_process();
        }, m_attachButton->GetId());

        Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this]([[maybe_unused]] wxDataViewEvent& event)
        {
            m_viewModel->open_process();
        }, m_processList->GetId());

        const auto checkboxClicked = [this](const Enums::FilterType filter)
        {
            m_viewModel->set_filter_type_with_persist(filter);
            m_viewModel->filter_list();
            m_processList->refresh_list();
        };

        Bind(wxEVT_DATAVIEW_COLUMN_HEADER_CLICK, [this](const wxDataViewEvent& event)
        {
            m_viewModel->set_sort_order();
            m_viewModel->set_clicked_column(event.GetColumn());
            m_viewModel->sort_list();
            m_processList->refresh_list();
        }, m_processList->GetId());

        Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this]([[maybe_unused]] wxDataViewEvent& event)
        {
            const auto nodeIndex = m_processList->get_selected_node_index();
            if (nodeIndex != ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
            {
                m_viewModel->set_selected_process_from_node(nodeIndex);
                m_attachButton->Enable();
            }
            else
            {
                m_viewModel->clear_selected_process();
                m_attachButton->Disable();
            }
        }, m_processList->GetId());

        Bind(wxEVT_RADIOBUTTON, [checkboxClicked]([[maybe_unused]] const wxCommandEvent& event)
        {
            checkboxClicked(Enums::FilterType::PROCESSID);
        }, m_filterByProcessIDRadioButton->GetId());

        Bind(wxEVT_RADIOBUTTON, [checkboxClicked]([[maybe_unused]] const wxCommandEvent& event)
        {
            checkboxClicked(Enums::FilterType::PROCESSNAME);
        }, m_filterByProcessNameRadioButton->GetId());

        Bind(wxEVT_RADIOBUTTON, [checkboxClicked]([[maybe_unused]] const wxCommandEvent& event)
        {
            checkboxClicked(Enums::FilterType::PROCESSOWNER);
        }, m_filterByProcessOwnerRadioButton->GetId());

        Bind(wxEVT_TEXT, [this](const wxCommandEvent& event)
        {
            const bool isEmpty = event.GetString().IsEmpty();

            m_viewModel->set_filter_text(event.GetString().utf8_string());
            m_viewModel->set_should_filter(!isEmpty);
            m_viewModel->filter_list();

            m_processList->refresh_list();
        }, m_processFilteringText->GetId());

        const auto validateAndOpenPid = [this]()
        {
            const auto pidText = m_directPidInput->GetValue().utf8_string();
            if (pidText.empty())
            {
                return;
            }

            const auto isDigitsOnly = std::ranges::all_of(pidText, [](const char c) { return c >= '0' && c <= '9'; });
            if (!isDigitsOnly)
            {
                return;
            }

            const auto pid = static_cast<std::uint32_t>(std::stoul(pidText));
            m_viewModel->open_process_by_pid(pid);
        };

        Bind(wxEVT_TEXT, [this]([[maybe_unused]] const wxCommandEvent& event)
        {
            const auto text = m_directPidInput->GetValue().utf8_string();
            const bool isValid = !text.empty() && std::ranges::all_of(text, [](const char c) { return c >= '0' && c <= '9'; });
            m_directPidOpenButton->Enable(isValid);
        }, m_directPidInput->GetId());

        Bind(wxEVT_BUTTON, [validateAndOpenPid]([[maybe_unused]] wxCommandEvent& event)
        {
            validateAndOpenPid();
        }, m_directPidOpenButton->GetId());

        Bind(wxEVT_TEXT_ENTER, [validateAndOpenPid]([[maybe_unused]] wxCommandEvent& event)
        {
            validateAndOpenPid();
        }, m_directPidInput->GetId());

        Bind(wxEVT_TIMER, [this]([[maybe_unused]] wxTimerEvent& event)
        {
            m_viewModel->update_process_list([this]()
            {
                CallAfter([this]() { m_processList->refresh_list(); });
            });

            m_resettableCallOnce.call([this]()
            {
                m_taskTimer->Start(StandardWidgetValues::TIMER_INTERVAL_MS);
            });

            event.Skip();
        }, m_taskTimer->GetId());
    }

    void ProcessListView::on_show(wxShowEvent& event)
    {
        if (event.IsShown())
        {
            m_viewModel->update_process_list([this]()
            {
                CallAfter([this]() { m_processList->refresh_list(); });
            });

            m_resettableCallOnce.reset();
            m_taskTimer->Start();
        }
        else
        {
            m_taskTimer->Stop();
        }

        event.Skip();
    }

    void ProcessListView::vertex_event_callback(const Event::EventId eventId, [[maybe_unused]] const Event::VertexEvent& event)
    {
        switch (eventId)
        {
        case Event::VIEW_EVENT:
            CallAfter([this]()
            {
                std::ignore = toggle_view();
            });
            break;
        case Event::PROCESS_OPEN_EVENT:
        {
            const auto& openEvent = static_cast<const Event::ProcessOpenEvent&>(event);
            const auto processId = openEvent.get_process_id();
            const std::string processName{openEvent.get_process_name()};
            CallAfter([this, processId, processName]()
            {
                std::ignore = toggle_view();
                m_viewModel->broadcast_process_opened(processId, processName);
            });
            break;
        }
        default: break;
        }
    }

    bool ProcessListView::toggle_view()
    {
        return IsShown() ? Hide() : Show();
    }

    void ProcessListView::restore_ui_state() const
    {
        const int filterTypeIndex = m_viewModel->get_filter_type_index();

        switch (filterTypeIndex)
        {
        case static_cast<int>(Enums::FilterType::PROCESSID):
            m_filterByProcessIDRadioButton->SetValue(true);
            m_viewModel->set_filter_type(Enums::FilterType::PROCESSID);
            break;
        case static_cast<int>(Enums::FilterType::PROCESSOWNER):
            m_filterByProcessOwnerRadioButton->SetValue(true);
            m_viewModel->set_filter_type(Enums::FilterType::PROCESSOWNER);
            break;
        case static_cast<int>(Enums::FilterType::PROCESSNAME):
        default:
            m_filterByProcessNameRadioButton->SetValue(true);
            m_viewModel->set_filter_type(Enums::FilterType::PROCESSNAME);
            break;
        }
    }
}
