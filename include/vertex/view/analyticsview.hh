//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/timer.h>
#include <vertex/viewmodel/analyticsviewmodel.hh>
#include <vertex/language/ilanguage.hh>
#include <memory>

namespace Vertex::View
{
    class AnalyticsView final : public wxDialog
    {
      public:
        AnalyticsView(Language::ILanguage& languageService, std::unique_ptr<ViewModel::AnalyticsViewModel> viewModel);
        void initialize_view();
        void refresh_logs();

      private:
        void create_controls();
        void layout_controls();
        void bind_events();
        void setup_event_callback();
        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        [[nodiscard]] bool toggle_view();

        void on_clear_clicked(wxCommandEvent& event);
        void on_save_clicked(wxCommandEvent& event);
        void on_refresh_timer(wxTimerEvent& event);
        void append_log_entry(const Log::LogEntry& entry, bool isDarkMode) const;

        [[nodiscard]] static wxColour get_log_color(Log::LogLevel level, bool isDarkMode);

        Language::ILanguage& m_languageService;
        std::unique_ptr<ViewModel::AnalyticsViewModel> m_viewModel;
        wxRichTextCtrl* m_logTextCtrl{};
        wxButton* m_clearButton{};
        wxButton* m_saveButton{};
        wxBoxSizer* m_mainSizer {};
        wxBoxSizer* m_buttonSizer {};
        wxTimer* m_refreshTimer {};

        std::vector<Log::LogEntry> m_cachedEntries;
    };
}
