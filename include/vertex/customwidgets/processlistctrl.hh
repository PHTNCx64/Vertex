//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/listctrl.h>
#include <wx/timer.h>

#include <vertex/viewmodel/processlistviewmodel.hh>
#include <vertex/gui/events/processlistfilterevent.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class ProcessListControl final : public wxListCtrl
    {
    public:
        explicit ProcessListControl(wxWindow* parent, Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel);
        ~ProcessListControl() override;

        void refresh_list();

    private:
        [[nodiscard]] wxString OnGetItemText(long item, long column) const override;
        [[nodiscard]] int GetItemCount() const override;

        static constexpr long PROCESS_ID_COLUMN{};
        static constexpr long PROCESS_NAME_COLUMN{1};
        static constexpr long PROCESS_OWNER_COLUMN{2};

        Language::ILanguage& m_languageService;

        std::shared_ptr<ViewModel::ProcessListViewModel> m_viewModel{};
        wxTimer m_intervalTimer{};
    };
}
