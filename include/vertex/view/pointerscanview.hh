//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/treectrl.h>

#include <memory>
#include <vertex/language/ilanguage.hh>
#include <vertex/viewmodel/pointerscanviewmodel.hh>

namespace Vertex::View
{
    class PointerScanView final : public wxDialog
    {
    public:
        PointerScanView(
            Language::ILanguage& languageService,
            std::unique_ptr<ViewModel::PointerScanViewModel> viewModel
        );

        void start_scan(const Scanner::PointerScanConfig& config);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();
        void setup_event_callback();
        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        [[nodiscard]] bool toggle_view();

        void on_scan_progress_update(wxTimerEvent& event);
        void on_stop_clicked(wxCommandEvent& event);
        void on_save_clicked(wxCommandEvent& event);
        void on_load_clicked(wxCommandEvent& event);
        void on_rescan_clicked(wxCommandEvent& event);
        void on_compare_clicked(wxCommandEvent& event);
        void on_close(wxCloseEvent& event);
        void on_tree_item_expanding(const wxTreeEvent& event);

        void update_button_states() const;
        void populate_stable_paths();

        void populate_roots();
        void add_children_to_item(wxTreeItemId parentItem, std::uint32_t nodeId);

        Language::ILanguage& m_languageService;
        std::unique_ptr<ViewModel::PointerScanViewModel> m_viewModel{};

        wxBoxSizer* m_mainSizer{};
        wxTreeCtrl* m_tree{};
        wxGauge* m_progressBar{};
        wxStaticText* m_statusText{};
        wxStaticText* m_statsText{};
        wxButton* m_stopButton{};
        wxButton* m_saveButton{};
        wxButton* m_loadButton{};
        wxButton* m_rescanButton{};
        wxButton* m_compareButton{};
        wxTimer* m_progressTimer{};

        wxTreeItemId m_rootItem{};
    };
}
