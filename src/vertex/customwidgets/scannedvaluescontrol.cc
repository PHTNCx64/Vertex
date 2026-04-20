//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/scannedvaluescontrol.hh>

#include <algorithm>
#include <limits>
#include <utility>

#include <fmt/format.h>

#include <wx/clipbrd.h>
#include <wx/menu.h>

namespace Vertex::CustomWidgets
{
    ScannedValuesControl::ScannedValuesControl(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::shared_ptr<ViewModel::MainViewModel>& viewModel
    )
        : VertexDataViewCtrl(parent, wxID_ANY, wxDV_ROW_LINES | wxDV_SINGLE)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_dataModel(new ScannedValuesDataModel(viewModel))
    {
        AssociateModel(m_dataModel.get());

        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnAddress")),
            ScannedValuesDataModel::ADDRESS_COL, COLUMN_WIDTH_ADDRESS_DIP,
            wxALIGN_LEFT);
        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnCurrentValue")),
            ScannedValuesDataModel::VALUE_COL, COLUMN_WIDTH_VALUE_DIP,
            wxALIGN_LEFT);
        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnFirstValue")),
            ScannedValuesDataModel::FIRST_VALUE_COL, COLUMN_WIDTH_FIRST_VALUE_DIP,
            wxALIGN_LEFT);
        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.scannedColumnPreviousValue")),
            ScannedValuesDataModel::PREVIOUS_VALUE_COL, COLUMN_WIDTH_PREVIOUS_VALUE_DIP,
            wxALIGN_LEFT);

        m_refreshTimer = new wxTimer(this, wxID_ANY);

        Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &ScannedValuesControl::on_item_activated, this);
        Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &ScannedValuesControl::on_context_menu, this);
        Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ScannedValuesControl::on_selection_changed, this);
        Bind(wxEVT_TIMER, &ScannedValuesControl::on_refresh_timer, this, m_refreshTimer->GetId());
    }

    ScannedValuesControl::~ScannedValuesControl()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
            delete m_refreshTimer;
        }
    }

    void ScannedValuesControl::refresh_list()
    {
        const auto rawCount = std::min(
            m_viewModel->get_scanned_values_count(),
            static_cast<std::int64_t>(MAX_DISPLAYED_ITEMS));
        const unsigned int count = static_cast<unsigned int>(rawCount);

        m_dataModel->reset_rows(count);

        if (count > 0)
        {
            const int perPage = std::max(1, GetCountPerPage());
            const int endLine = std::min(static_cast<int>(count), perPage + CACHE_OVERSCAN);
            m_viewModel->update_cache_window(0, endLine);
        }
    }

    void ScannedValuesControl::clear_list()
    {
        stop_auto_refresh();
        m_dataModel->reset_rows(0);
    }

    void ScannedValuesControl::start_auto_refresh()
    {
        if (m_refreshTimer && !m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Start(AUTO_REFRESH_INTERVAL_MS);
        }
    }

    void ScannedValuesControl::stop_auto_refresh()
    {
        if (m_refreshTimer && m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Stop();
        }
    }

    void ScannedValuesControl::set_selection_change_callback(SelectionChangeCallback callback)
    {
        m_selectionChangeCallback = std::move(callback);
    }

    void ScannedValuesControl::set_add_to_table_callback(AddToTableCallback callback)
    {
        m_addToTableCallback = std::move(callback);
    }

    void ScannedValuesControl::set_find_access_callback(FindAccessCallback callback)
    {
        m_findAccessCallback = std::move(callback);
    }

    int ScannedValuesControl::get_selected_index() const
    {
        const wxDataViewItem selection = GetSelection();
        if (!selection.IsOk())
        {
            return -1;
        }
        const unsigned int viewRow = m_dataModel->GetRow(selection);
        return static_cast<int>(m_dataModel->get_source_row(viewRow));
    }

    std::optional<std::uint64_t> ScannedValuesControl::get_selected_address() const
    {
        return get_address_for_row(get_selected_index());
    }

    std::optional<std::uint64_t> ScannedValuesControl::get_address_for_row(const int modelRowIndex) const
    {
        if (modelRowIndex < 0)
        {
            return std::nullopt;
        }
        const auto value = m_viewModel->get_scanned_value_at(modelRowIndex);
        if (value.address.empty())
        {
            return std::nullopt;
        }
        try
        {
            return std::stoull(value.address, nullptr, 16);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    void ScannedValuesControl::on_item_activated([[maybe_unused]] wxDataViewEvent& event)
    {
        const int index = get_selected_index();
        if (index < 0 || !m_addToTableCallback)
        {
            return;
        }
        const auto address = get_address_for_row(index);
        if (address.has_value())
        {
            m_addToTableCallback(index, address.value());
        }
    }

    void ScannedValuesControl::on_selection_changed([[maybe_unused]] wxDataViewEvent& event)
    {
        if (!m_selectionChangeCallback)
        {
            return;
        }
        const int index = get_selected_index();
        const auto address = get_address_for_row(index);
        m_selectionChangeCallback(index, address.value_or(0));
    }

    void ScannedValuesControl::on_context_menu([[maybe_unused]] wxDataViewEvent& event)
    {
        const int index = get_selected_index();
        if (index < 0)
        {
            return;
        }
        const auto scannedValue = m_viewModel->get_scanned_value_at(index);

        wxMenu menu{};
        menu.Append(MENU_ID_ADD_TO_TABLE,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.addToTable")));
        menu.AppendSeparator();
        menu.Append(MENU_ID_COPY_ADDRESS,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyAddress")));
        menu.Append(MENU_ID_COPY_VALUE,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyValue")));
        menu.Append(MENU_ID_COPY_ALL,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyAll")));
        menu.AppendSeparator();
        menu.Append(MENU_ID_FIND_ACCESS,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.findAccess")));

        const int selection = GetPopupMenuSelectionFromUser(menu);

        switch (selection)
        {
        case MENU_ID_ADD_TO_TABLE:
            if (m_addToTableCallback)
            {
                const auto address = get_address_for_row(index);
                if (address.has_value())
                {
                    m_addToTableCallback(index, address.value());
                }
            }
            break;
        case MENU_ID_COPY_ADDRESS:
            if (wxTheClipboard->Open())
            {
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(scannedValue.address)));
                wxTheClipboard->Close();
            }
            break;
        case MENU_ID_COPY_VALUE:
            if (wxTheClipboard->Open())
            {
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(scannedValue.value)));
                wxTheClipboard->Close();
            }
            break;
        case MENU_ID_COPY_ALL:
            if (wxTheClipboard->Open())
            {
                const auto fullLine = fmt::format("{}\t{}\t{}\t{}",
                    scannedValue.address, scannedValue.value,
                    scannedValue.firstValue, scannedValue.previousValue);
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(fullLine)));
                wxTheClipboard->Close();
            }
            break;
        case MENU_ID_FIND_ACCESS:
        {
            if (!m_findAccessCallback)
            {
                break;
            }
            const auto address = get_address_for_row(index);
            if (!address.has_value())
            {
                break;
            }
            const auto size = m_viewModel->get_scanned_value_size();
            if (size > 0)
            {
                m_findAccessCallback(address.value(), size);
            }
            break;
        }
        default:
            break;
        }
    }

    void ScannedValuesControl::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (!IsShownOnScreen())
        {
            return;
        }
        const wxSize clientSize = GetClientSize();
        if (clientSize.GetWidth() <= 1 || clientSize.GetHeight() <= MIN_SAFE_VIEWPORT_HEIGHT_PX)
        {
            return;
        }
        refresh_visible_range();
    }

    void ScannedValuesControl::refresh_visible_range()
    {
        const unsigned int rowCount = m_dataModel->GetCount();
        if (rowCount == 0)
        {
            return;
        }

        const wxDataViewItem topItem = GetTopItem();
        unsigned int topRow{};
        if (topItem.IsOk())
        {
            topRow = m_dataModel->GetRow(topItem);
        }
        const int perPage = std::max(1, GetCountPerPage());
        const int endRow = std::min(static_cast<int>(rowCount),
                                    static_cast<int>(topRow) + perPage + CACHE_OVERSCAN);

        unsigned int minSource{std::numeric_limits<unsigned int>::max()};
        unsigned int maxSource{};
        for (unsigned int r = topRow; r < static_cast<unsigned int>(endRow); ++r)
        {
            const unsigned int sourceRow = m_dataModel->get_source_row(r);
            minSource = std::min(minSource, sourceRow);
            maxSource = std::max(maxSource, sourceRow);
        }
        const int sourceStart = static_cast<int>(minSource);
        const int sourceEndExclusive = static_cast<int>(maxSource) + 1;

        m_viewModel->update_cache_window(sourceStart, sourceEndExclusive);
        m_viewModel->refresh_visible_range(sourceStart, sourceEndExclusive);

        for (unsigned int r = topRow; r < static_cast<unsigned int>(endRow); ++r)
        {
            m_dataModel->RowChanged(r);
        }
    }
}
