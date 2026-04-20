//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/customwidgets/accesstrackercontrol.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <cstddef>
#include <functional>

namespace Vertex::CustomWidgets
{
    class AccessTrackerPanel final : public wxPanel
    {
    public:
        using ActionCallback = std::function<void()>;
        using RowCallback = std::function<void(std::size_t rowIndex)>;

        AccessTrackerPanel(wxWindow* parent,
                           Language::ILanguage& languageService,
                           const ViewModel::AccessTrackerViewModel& viewModel);

        void set_state(const ViewModel::TrackingState& state);
        void notify_entries_changed() const;

        void set_stop_callback(ActionCallback callback);
        void set_clear_callback(ActionCallback callback);
        void set_close_callback(ActionCallback callback);
        void set_view_in_disassembly_callback(RowCallback callback);
        void set_show_call_stack_callback(RowCallback callback);
        void set_copy_address_callback(RowCallback callback);
        void set_copy_registers_callback(RowCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events() const;
        void apply_button_states(ViewModel::TrackingStatus status) const;
        [[nodiscard]] wxString to_status_text(ViewModel::TrackingStatus status) const;

        Language::ILanguage& m_languageService;
        const ViewModel::AccessTrackerViewModel& m_viewModel;
        wxStaticText* m_trackingLabel {};
        wxStaticText* m_statusLabel {};
        wxButton* m_stopButton {};
        wxButton* m_clearButton {};
        wxButton* m_closeButton {};
        AccessTrackerControl* m_control {};

        wxBoxSizer* m_mainSizer {};
        wxBoxSizer* m_toolbarSizer {};

        ActionCallback m_stopCallback {};
        ActionCallback m_clearCallback {};
        ActionCallback m_closeCallback {};
    };
}
