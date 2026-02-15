//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/listctrl.h>
#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class ScannedValuesListCtrl final : public wxListCtrl
    {
    public:
        explicit ScannedValuesListCtrl(
            wxWindow* parent,
            Language::ILanguage& languageService, const std::shared_ptr<ViewModel::MainViewModel>& viewModel
        );
        ~ScannedValuesListCtrl() override;

        void refresh_list();
        void clear_list();
        void start_auto_refresh();
        void stop_auto_refresh();

    private:
        [[nodiscard]] wxString OnGetItemText(long item, long column) const override;
        [[nodiscard]] int OnGetItemImage(long item) const override;
        [[nodiscard]] wxListItemAttr* OnGetItemAttr(long item) const override;

        void on_scroll(wxScrollWinEvent& event);
        void on_refresh_timer(wxTimerEvent& event);
        void on_scroll_timer(wxTimerEvent& event);
        void refresh_visible_items();

        static constexpr long ADDRESS_COLUMN{};
        static constexpr long VALUE_COLUMN{1};
        static constexpr long PREVIOUS_VALUE_COLUMN{2};
        static constexpr long TYPE_COLUMN{3};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};

        mutable wxListItemAttr m_attr1{};
        mutable wxListItemAttr m_attr2{};

        wxTimer* m_refreshTimer{};
        wxTimer* m_scrollStopTimer{};
        bool m_isScrolling{};
    };
}
