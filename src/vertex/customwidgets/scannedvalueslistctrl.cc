//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/scannedvalueslistctrl.hh>
#include <fmt/format.h>

namespace Vertex::CustomWidgets
{
    ScannedValuesListCtrl::ScannedValuesListCtrl(
        wxWindow* parent,
        Language::ILanguage& languageService, const std::shared_ptr<ViewModel::MainViewModel>& viewModel
    )
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
    {
        InsertColumn(ADDRESS_COLUMN,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnAddress")),
            wxLIST_FORMAT_LEFT,
            FromDIP(150));

        InsertColumn(VALUE_COLUMN,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnCurrentValue")),
            wxLIST_FORMAT_LEFT,
            FromDIP(120));

        InsertColumn(PREVIOUS_VALUE_COLUMN,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnPreviousValue")),
            wxLIST_FORMAT_LEFT,
            FromDIP(120));

        InsertColumn(TYPE_COLUMN,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnType")),
            wxLIST_FORMAT_LEFT,
            FromDIP(100));

        m_attr1.SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        m_attr2.SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

        m_refreshTimer = new wxTimer(this, wxID_ANY);
        m_scrollStopTimer = new wxTimer(this, wxID_ANY + 1);

        Bind(wxEVT_SCROLLWIN_TOP, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_BOTTOM, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_LINEUP, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_LINEDOWN, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_PAGEUP, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_PAGEDOWN, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_THUMBTRACK, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &ScannedValuesListCtrl::on_scroll, this);
        Bind(wxEVT_TIMER, &ScannedValuesListCtrl::on_refresh_timer, this, m_refreshTimer->GetId());
        Bind(wxEVT_TIMER, &ScannedValuesListCtrl::on_scroll_timer, this, m_scrollStopTimer->GetId());
    }

    ScannedValuesListCtrl::~ScannedValuesListCtrl()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
            delete m_refreshTimer;
        }
        if (m_scrollStopTimer)
        {
            m_scrollStopTimer->Stop();
            delete m_scrollStopTimer;
        }
    }

    void ScannedValuesListCtrl::refresh_list()
    {
        int count = m_viewModel->get_scanned_values_count();

        if (count > 10000)
        {
            count = 10000;
        }

        SetItemCount(count);
        Refresh();
    }

    void ScannedValuesListCtrl::clear_list()
    {
        stop_auto_refresh();
        SetItemCount(0);
        Refresh();
    }

    void ScannedValuesListCtrl::start_auto_refresh()
    {
        if (m_refreshTimer && !m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Start(250);
        }
    }

    void ScannedValuesListCtrl::stop_auto_refresh()
    {
        if (m_refreshTimer && m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Stop();
        }
    }

    void ScannedValuesListCtrl::on_scroll(wxScrollWinEvent& event)
    {
        m_isScrolling = true;

        if (m_refreshTimer && m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Stop();
        }

        if (m_scrollStopTimer)
        {
            m_scrollStopTimer->Start(300, wxTIMER_ONE_SHOT);
        }

        event.Skip();
    }

    void ScannedValuesListCtrl::on_scroll_timer([[maybe_unused]] wxTimerEvent& event)
    {
        m_isScrolling = false;

        refresh_visible_items();

        if (m_refreshTimer && !m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Start(750);
        }
    }

    void ScannedValuesListCtrl::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (!m_isScrolling)
        {
            refresh_visible_items();
        }
    }

    void ScannedValuesListCtrl::refresh_visible_items()
    {
        const long topItem = GetTopItem();
        const long visibleCount = GetCountPerPage();

        if (topItem < 0 || visibleCount <= 0)
        {
            return;
        }

        const int startIndex = static_cast<int>(topItem);
        const int endIndex = static_cast<int>(topItem + visibleCount);

        m_viewModel->update_cache_window(startIndex, endIndex);
        m_viewModel->refresh_visible_range(startIndex, endIndex);

        RefreshItems(topItem, std::min(topItem + visibleCount, static_cast<long>(GetItemCount()) - 1));
    }

    wxString ScannedValuesListCtrl::OnGetItemText(const long item, const long column) const
    {
        if (item < 0)
        {
            return wxEmptyString;
        }

        const auto scannedValue = m_viewModel->get_scanned_value_at(static_cast<int>(item));

        switch (column)
        {
            case ADDRESS_COLUMN:
                return wxString::FromUTF8(scannedValue.address);

            case VALUE_COLUMN:
                return wxString::FromUTF8(scannedValue.value);

            case PREVIOUS_VALUE_COLUMN:
                return wxString::FromUTF8(scannedValue.previousValue);

            case TYPE_COLUMN:
                return wxString::FromUTF8(scannedValue.firstValue);

            default:
                return wxEmptyString;
        }
    }

    int ScannedValuesListCtrl::OnGetItemImage([[maybe_unused]] long item) const
    {
        return -1;
    }

    wxListItemAttr* ScannedValuesListCtrl::OnGetItemAttr(const long item) const
    {
        return (item % 2 == 0) ? &m_attr1 : &m_attr2;
    }
}
