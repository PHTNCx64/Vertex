//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include "vertex/resettable_call_once.hh"

#include <vertex/viewmodel/processlistviewmodel.hh>

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/radiobut.h>

#include <vertex/language/language.hh>
#include <vertex/customwidgets/processlistctrl.hh>

namespace Vertex::View
{
    class ProcessListView final : public wxDialog
    {
    public:
        ProcessListView(Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel);

    private:
        void vertex_event_callback(Event::EventId, const Event::VertexEvent& event);
        void create_controls(const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel);
        void layout_controls();
        [[nodiscard]] bool toggle_view();
        void bind_events();
        void on_show(wxShowEvent& event);
        void restore_ui_state();

        mutable std::once_flag m_timerFlag{};

        std::shared_ptr<ViewModel::ProcessListViewModel> m_viewModel{};

        Language::ILanguage& m_languageService;

        wxTimer* m_taskTimer{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_processListInformationTextSizer{};
        wxStaticText* m_processListInformationText{};

        wxStaticBoxSizer* m_processFilteringOptionsBoxSizer{};
        wxStaticBox* m_processFilteringOptionsBox{};

        wxBoxSizer* m_processFilteringInputSizer{};
        wxStaticText* m_processFilteringTextInformation{};
        wxTextCtrl* m_processFilteringText{};

        wxBoxSizer* m_radioButtonOptionsSizer{};
        wxRadioButton* m_filterByProcessNameRadioButton{};
        wxRadioButton* m_filterByProcessIDRadioButton{};
        wxRadioButton* m_filterByProcessOwnerRadioButton{};

        wxBoxSizer* m_processListSizer{};
        CustomWidgets::ProcessListControl* m_processList{};

        wxBoxSizer* m_buttonOptionsSizer{};
        wxButton* m_attachButton{};
        wxButton* m_cancelButton{};

        ResettableCallOnce m_resettableCallOnce{};
    };
}
