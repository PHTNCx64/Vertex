//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/savedaddressescontrol.hh>

#include <algorithm>
#include <limits>
#include <utility>

#include <wx/arrstr.h>
#include <wx/clipbrd.h>
#include <wx/dataview.h>
#include <wx/menu.h>

namespace Vertex::CustomWidgets
{
    SavedAddressesControl::SavedAddressesControl(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::shared_ptr<ViewModel::MainViewModel>& viewModel
    )
        : VertexDataViewCtrl(parent, wxID_ANY, wxDV_ROW_LINES | wxDV_SINGLE)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_dataModel(new SavedAddressesDataModel(viewModel))
    {
        AssociateModel(m_dataModel.get());

        auto* freezeRenderer = new wxDataViewToggleRenderer("bool", wxDATAVIEW_CELL_ACTIVATABLE);
        AppendColumn(new wxDataViewColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.savedColumnFreeze")),
            freezeRenderer, SavedAddressesDataModel::FREEZE_COL,
            FromDIP(COLUMN_WIDTH_FREEZE_DIP),
            wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));

        auto* addressRenderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_EDITABLE);
        AppendColumn(new wxDataViewColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.savedColumnAddress")),
            addressRenderer, SavedAddressesDataModel::ADDRESS_COL,
            FromDIP(COLUMN_WIDTH_ADDRESS_DIP),
            wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));

        wxArrayString typeChoices{};
        for (const auto& typeName : m_viewModel->get_value_type_names())
        {
            typeChoices.Add(wxString::FromUTF8(typeName));
        }
        auto* typeRenderer = new wxDataViewChoiceByIndexRenderer(typeChoices, wxDATAVIEW_CELL_EDITABLE);
        AppendColumn(new wxDataViewColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.savedColumnType")),
            typeRenderer, SavedAddressesDataModel::TYPE_COL,
            FromDIP(COLUMN_WIDTH_TYPE_DIP),
            wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));

        auto* valueRenderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_EDITABLE);
        AppendColumn(new wxDataViewColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.ui.savedColumnValue")),
            valueRenderer, SavedAddressesDataModel::VALUE_COL,
            FromDIP(COLUMN_WIDTH_VALUE_DIP),
            wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));

        m_refreshTimer = new wxTimer(this, wxID_ANY);

        Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &SavedAddressesControl::on_context_menu, this);
        Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &SavedAddressesControl::on_selection_changed, this);
        Bind(wxEVT_KEY_DOWN, &SavedAddressesControl::on_key_down, this);
        Bind(wxEVT_TIMER, &SavedAddressesControl::on_refresh_timer, this, m_refreshTimer->GetId());
        Bind(wxEVT_DATAVIEW_ITEM_EDITING_STARTED, &SavedAddressesControl::on_editing_started, this);
        Bind(wxEVT_DATAVIEW_ITEM_EDITING_DONE, &SavedAddressesControl::on_editing_done, this);
    }

    SavedAddressesControl::~SavedAddressesControl()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
            delete m_refreshTimer;
        }
    }

    void SavedAddressesControl::refresh_list()
    {
        const unsigned int count = static_cast<unsigned int>(m_viewModel->get_saved_addresses_count());
        m_dataModel->reset_rows(count);
    }

    void SavedAddressesControl::clear_list()
    {
        stop_auto_refresh();
        m_dataModel->reset_rows(0);
        UnselectAll();
    }

    void SavedAddressesControl::start_auto_refresh()
    {
        if (m_refreshTimer && !m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Start(AUTO_REFRESH_INTERVAL_MS);
        }
    }

    void SavedAddressesControl::stop_auto_refresh()
    {
        if (m_refreshTimer && m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Stop();
        }
    }

    void SavedAddressesControl::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (m_isEditing)
        {
            return;
        }
        if (!IsShownOnScreen())
        {
            return;
        }
        const wxSize clientSize = GetClientSize();
        if (clientSize.GetWidth() <= 1 || clientSize.GetHeight() <= MIN_SAFE_VIEWPORT_HEIGHT_PX)
        {
            return;
        }
        refresh_values();
    }

    void SavedAddressesControl::on_editing_started([[maybe_unused]] wxDataViewEvent& event)
    {
        m_isEditing = true;
    }

    void SavedAddressesControl::on_editing_done([[maybe_unused]] wxDataViewEvent& event)
    {
        m_isEditing = false;
    }

    void SavedAddressesControl::refresh_values()
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
        if (topRow >= rowCount)
        {
            topRow = rowCount - 1;
        }

        const int perPage = std::max(1, GetCountPerPage());
        const int endRowExclusive = std::min(
            static_cast<int>(rowCount),
            static_cast<int>(topRow) + perPage + REFRESH_OVERSCAN_ROWS);

        unsigned int minSource{std::numeric_limits<unsigned int>::max()};
        unsigned int maxSource{};
        for (unsigned int r = topRow; r < static_cast<unsigned int>(endRowExclusive); ++r)
        {
            const unsigned int sourceRow = m_dataModel->get_source_row(r);
            minSource = std::min(minSource, sourceRow);
            maxSource = std::max(maxSource, sourceRow);
        }

        m_viewModel->refresh_saved_addresses_range(
            static_cast<int>(minSource),
            static_cast<int>(maxSource));

        for (unsigned int r = topRow; r < static_cast<unsigned int>(endRowExclusive); ++r)
        {
            m_dataModel->RowChanged(r);
        }
    }

    void SavedAddressesControl::set_selection_change_callback(SelectionChangeCallback callback)
    {
        m_selectionChangeCallback = std::move(callback);
    }

    void SavedAddressesControl::set_freeze_toggle_callback(FreezeToggleCallback callback)
    {
        m_freezeToggleCallback = std::move(callback);
        m_dataModel->set_freeze_toggle_callback(m_freezeToggleCallback);
    }

    void SavedAddressesControl::set_value_edit_callback(ValueEditCallback callback)
    {
        m_valueEditCallback = std::move(callback);
        m_dataModel->set_value_edit_callback(m_valueEditCallback);
    }

    void SavedAddressesControl::set_delete_callback(DeleteCallback callback)
    {
        m_deleteCallback = std::move(callback);
    }

    void SavedAddressesControl::set_view_in_disassembly_callback(ViewInDisassemblyCallback callback)
    {
        m_viewInDisassemblyCallback = std::move(callback);
    }

    void SavedAddressesControl::set_find_access_callback(FindAccessCallback callback)
    {
        m_findAccessCallback = std::move(callback);
    }

    int SavedAddressesControl::get_selected_index() const
    {
        const wxDataViewItem selection = GetSelection();
        if (!selection.IsOk())
        {
            return -1;
        }
        const unsigned int viewRow = m_dataModel->GetRow(selection);
        return static_cast<int>(m_dataModel->get_source_row(viewRow));
    }

    void SavedAddressesControl::on_selection_changed([[maybe_unused]] wxDataViewEvent& event)
    {
        if (m_selectionChangeCallback)
        {
            m_selectionChangeCallback(get_selected_index());
        }
    }

    void SavedAddressesControl::on_key_down(wxKeyEvent& event)
    {
        const int selected = get_selected_index();
        if (selected < 0)
        {
            event.Skip();
            return;
        }

        switch (event.GetKeyCode())
        {
        case WXK_SPACE:
            toggle_freeze_selected();
            return;
        case WXK_DELETE:
            delete_selected();
            return;
        default:
            event.Skip();
        }
    }

    void SavedAddressesControl::toggle_freeze_selected()
    {
        const wxDataViewItem selection = GetSelection();
        if (!selection.IsOk())
        {
            return;
        }
        const unsigned int viewRow = m_dataModel->GetRow(selection);
        const int sourceRow = static_cast<int>(m_dataModel->get_source_row(viewRow));
        const auto saved = m_viewModel->get_saved_address_at(sourceRow);
        const bool newFrozen = !saved.frozen;
        m_viewModel->set_saved_address_frozen(sourceRow, newFrozen);
        if (m_freezeToggleCallback)
        {
            m_freezeToggleCallback(sourceRow, newFrozen);
        }
        m_dataModel->RowChanged(viewRow);
    }

    void SavedAddressesControl::delete_selected()
    {
        const int selected = get_selected_index();
        if (selected < 0)
        {
            return;
        }
        m_viewModel->remove_saved_address(selected);
        if (m_deleteCallback)
        {
            m_deleteCallback(selected);
        }
        refresh_list();
    }

    void SavedAddressesControl::on_context_menu([[maybe_unused]] wxDataViewEvent& event)
    {
        const int selected = get_selected_index();
        if (selected < 0)
        {
            return;
        }
        const auto saved = m_viewModel->get_saved_address_at(selected);

        wxMenu menu{};
        menu.Append(MENU_ID_TOGGLE_FREEZE, saved.frozen
            ? wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.unfreeze"))
            : wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.freeze")));
        menu.AppendSeparator();
        menu.Append(MENU_ID_COPY_ADDRESS,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyAddress")));
        menu.Append(MENU_ID_COPY_VALUE,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyValue")));
        menu.AppendSeparator();
        menu.Append(MENU_ID_VIEW_IN_DISASSEMBLY,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.viewInDisassembly")));
        menu.Append(MENU_ID_FIND_ACCESS,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.findAccess")));
        menu.AppendSeparator();
        menu.Append(MENU_ID_DELETE,
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.delete")));

        const int selection = GetPopupMenuSelectionFromUser(menu);

        switch (selection)
        {
        case MENU_ID_TOGGLE_FREEZE:
            toggle_freeze_selected();
            break;
        case MENU_ID_COPY_ADDRESS:
            if (wxTheClipboard->Open())
            {
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(saved.addressStr)));
                wxTheClipboard->Close();
            }
            break;
        case MENU_ID_COPY_VALUE:
            if (wxTheClipboard->Open())
            {
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(saved.value)));
                wxTheClipboard->Close();
            }
            break;
        case MENU_ID_DELETE:
            delete_selected();
            break;
        case MENU_ID_VIEW_IN_DISASSEMBLY:
            if (m_viewInDisassemblyCallback)
            {
                m_viewInDisassemblyCallback(saved.address);
            }
            break;
        case MENU_ID_FIND_ACCESS:
            if (m_findAccessCallback)
            {
                const auto watchSize = m_viewModel->get_saved_address_watch_size(selected);
                if (watchSize > 0)
                {
                    m_findAccessCallback(saved.address, watchSize);
                }
            }
            break;
        default:
            break;
        }
    }
}
