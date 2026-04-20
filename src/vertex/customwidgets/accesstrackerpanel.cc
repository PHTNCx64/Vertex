//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/accesstrackerpanel.hh>
#include <vertex/utility.hh>

#include <fmt/format.h>

#include <utility>

namespace Vertex::CustomWidgets
{
    AccessTrackerPanel::AccessTrackerPanel(wxWindow* parent,
                                           Language::ILanguage& languageService,
                                           const ViewModel::AccessTrackerViewModel& viewModel)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
    {
        create_controls();
        layout_controls();
        bind_events();
        apply_button_states(ViewModel::TrackingStatus::Idle);
    }

    void AccessTrackerPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

        m_trackingLabel = new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.tracking")));
        m_statusLabel = new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusIdle")));
        m_statusLabel->SetForegroundColour(wxColour(
            AccessTrackerValues::STATUS_LABEL_GRAY,
            AccessTrackerValues::STATUS_LABEL_GRAY,
            AccessTrackerValues::STATUS_LABEL_GRAY));

        m_stopButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.stop")));
        m_clearButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.clear")));
        m_closeButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.close")));

        m_control = new AccessTrackerControl(this, m_languageService, m_viewModel);
    }

    void AccessTrackerPanel::layout_controls()
    {
        m_toolbarSizer->Add(m_trackingLabel, StandardWidgetValues::NO_PROPORTION,
            wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(AccessTrackerValues::TOOLBAR_GAP_MEDIUM));
        m_toolbarSizer->Add(m_statusLabel, StandardWidgetValues::NO_PROPORTION,
            wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(AccessTrackerValues::TOOLBAR_GAP_LARGE));
        m_toolbarSizer->AddStretchSpacer();
        m_toolbarSizer->Add(m_stopButton, StandardWidgetValues::NO_PROPORTION,
            wxRIGHT, FromDIP(AccessTrackerValues::TOOLBAR_GAP_SMALL));
        m_toolbarSizer->Add(m_clearButton, StandardWidgetValues::NO_PROPORTION,
            wxRIGHT, FromDIP(AccessTrackerValues::TOOLBAR_GAP_SMALL));
        m_toolbarSizer->Add(m_closeButton, StandardWidgetValues::NO_PROPORTION);

        m_mainSizer->Add(m_toolbarSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_control, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        SetSizer(m_mainSizer);
    }

    void AccessTrackerPanel::bind_events() const
    {
        m_stopButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
        {
            if (m_stopCallback)
            {
                m_stopCallback();
            }
        });

        m_clearButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
        {
            if (m_clearCallback)
            {
                m_clearCallback();
            }
        });

        m_closeButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
        {
            if (m_closeCallback)
            {
                m_closeCallback();
            }
        });
    }

    void AccessTrackerPanel::apply_button_states(const ViewModel::TrackingStatus status) const
    {
        const bool canStop = status == ViewModel::TrackingStatus::Active
            || status == ViewModel::TrackingStatus::StopFailed;
        const bool canClear = status == ViewModel::TrackingStatus::Active
            || status == ViewModel::TrackingStatus::Idle
            || status == ViewModel::TrackingStatus::StartFailed
            || status == ViewModel::TrackingStatus::StopFailed;

        m_stopButton->Enable(canStop);
        m_clearButton->Enable(canClear);
    }

    void AccessTrackerPanel::set_state(const ViewModel::TrackingState& state)
    {
        if (state.status == ViewModel::TrackingStatus::Idle)
        {
            m_trackingLabel->SetLabel(
                wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.tracking")));
        }
        else
        {
            m_trackingLabel->SetLabel(wxString::FromUTF8(
                fmt::format("{}: 0x{:X} ({} B)",
                    m_languageService.fetch_translation("accessTracker.tracking"),
                    state.targetAddress,
                    state.targetSize)));
        }

        m_statusLabel->SetLabel(to_status_text(state.status));
        apply_button_states(state.status);
        Layout();
    }

    void AccessTrackerPanel::notify_entries_changed() const {
        m_control->notify_entries_changed();
    }

    void AccessTrackerPanel::set_stop_callback(ActionCallback callback)
    {
        m_stopCallback = std::move(callback);
    }

    void AccessTrackerPanel::set_clear_callback(ActionCallback callback)
    {
        m_clearCallback = std::move(callback);
    }

    void AccessTrackerPanel::set_close_callback(ActionCallback callback)
    {
        m_closeCallback = std::move(callback);
    }

    void AccessTrackerPanel::set_view_in_disassembly_callback(RowCallback callback)
    {
        m_control->set_view_in_disassembly_callback(std::move(callback));
    }

    void AccessTrackerPanel::set_show_call_stack_callback(RowCallback callback)
    {
        m_control->set_show_call_stack_callback(std::move(callback));
    }

    void AccessTrackerPanel::set_copy_address_callback(RowCallback callback)
    {
        m_control->set_copy_address_callback(std::move(callback));
    }

    void AccessTrackerPanel::set_copy_registers_callback(RowCallback callback)
    {
        m_control->set_copy_registers_callback(std::move(callback));
    }

    wxString AccessTrackerPanel::to_status_text(const ViewModel::TrackingStatus status) const
    {
        switch (status)
        {
            case ViewModel::TrackingStatus::Idle:
                return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusIdle"));
            case ViewModel::TrackingStatus::Starting:
                return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusStarting"));
            case ViewModel::TrackingStatus::Active:
                return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusActive"));
            case ViewModel::TrackingStatus::Stopping:
                return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusStopping"));
            case ViewModel::TrackingStatus::StartFailed:
                return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusStartFailed"));
            case ViewModel::TrackingStatus::StopFailed:
                return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusStopFailed"));
        }

        return wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.statusIdle"));
    }
}
