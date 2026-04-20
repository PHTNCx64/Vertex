//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/accesstrackercontrol.hh>

#include <utility>

#include <wx/dataview.h>
#include <wx/menu.h>

#include <vertex/utility.hh>

namespace Vertex::CustomWidgets
{
    AccessTrackerControl::AccessTrackerControl(wxWindow* parent,
                                               Language::ILanguage& languageService,
                                               const ViewModel::AccessTrackerViewModel& viewModel)
        : VertexDataViewCtrl(parent, wxID_ANY, wxDV_ROW_LINES | wxDV_SINGLE)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_dataModel(new AccessTrackerDataModel(viewModel))
    {
        AssociateModel(m_dataModel.get());

        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnInstruction")),
                           AccessTrackerDataModel::INSTRUCTION_COL,
                           AccessTrackerValues::COL_INSTRUCTION_WIDTH, wxALIGN_LEFT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnModule")),
                           AccessTrackerDataModel::MODULE_COL,
                           AccessTrackerValues::COL_MODULE_WIDTH,      wxALIGN_LEFT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnFunction")),
                           AccessTrackerDataModel::FUNCTION_COL,
                           AccessTrackerValues::COL_FUNCTION_WIDTH,    wxALIGN_LEFT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnMnemonic")),
                           AccessTrackerDataModel::MNEMONIC_COL,
                           AccessTrackerValues::COL_MNEMONIC_WIDTH,    wxALIGN_LEFT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnHits")),
                           AccessTrackerDataModel::HITS_COL,
                           AccessTrackerValues::COL_HITS_WIDTH,        wxALIGN_RIGHT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnAccess")),
                           AccessTrackerDataModel::ACCESS_COL,
                           AccessTrackerValues::COL_ACCESS_WIDTH,      wxALIGN_CENTER_HORIZONTAL, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnSize")),
                           AccessTrackerDataModel::SIZE_COL,
                           AccessTrackerValues::COL_SIZE_WIDTH,        wxALIGN_RIGHT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnRegisters")),
                           AccessTrackerDataModel::REGISTERS_COL,
                           AccessTrackerValues::COL_REGISTERS_WIDTH,   wxALIGN_LEFT, false);
        append_text_column(wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.columnCaller")),
                           AccessTrackerDataModel::CALLER_COL,
                           AccessTrackerValues::COL_CALLER_WIDTH,      wxALIGN_LEFT, false);

        Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &AccessTrackerControl::on_context_menu, this);
    }

    void AccessTrackerControl::notify_entries_changed()
    {
        m_dataModel->refresh_from_viewmodel();
    }

    void AccessTrackerControl::set_view_in_disassembly_callback(RowCallback callback)
    {
        m_viewInDisassemblyCallback = std::move(callback);
    }

    void AccessTrackerControl::set_show_call_stack_callback(RowCallback callback)
    {
        m_showCallStackCallback = std::move(callback);
    }

    void AccessTrackerControl::set_copy_address_callback(RowCallback callback)
    {
        m_copyAddressCallback = std::move(callback);
    }

    void AccessTrackerControl::set_copy_registers_callback(RowCallback callback)
    {
        m_copyRegistersCallback = std::move(callback);
    }

    std::size_t AccessTrackerControl::get_selected_row() const
    {
        const wxDataViewItem selection = GetSelection();
        if (!selection.IsOk())
        {
            return static_cast<std::size_t>(-1);
        }
        return static_cast<std::size_t>(m_dataModel->GetRow(selection));
    }

    void AccessTrackerControl::on_context_menu([[maybe_unused]] wxDataViewEvent& event)
    {
        const std::size_t row = get_selected_row();
        if (row == static_cast<std::size_t>(-1))
        {
            return;
        }

        wxMenu menu{};
        menu.Append(MENU_ID_VIEW_IN_DISASSEMBLY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.context.viewInDisassembly")));
        menu.Append(MENU_ID_SHOW_CALL_STACK,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.context.showCallStack")));
        menu.AppendSeparator();
        menu.Append(MENU_ID_COPY_ADDRESS,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.context.copyInstructionAddress")));
        menu.Append(MENU_ID_COPY_REGISTERS,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.context.copyRegisters")));

        const int selection = GetPopupMenuSelectionFromUser(menu);
        switch (selection)
        {
        case MENU_ID_VIEW_IN_DISASSEMBLY:
            if (m_viewInDisassemblyCallback)
            {
                m_viewInDisassemblyCallback(row);
            }
            break;
        case MENU_ID_SHOW_CALL_STACK:
            if (m_showCallStackCallback)
            {
                m_showCallStackCallback(row);
            }
            break;
        case MENU_ID_COPY_ADDRESS:
            if (m_copyAddressCallback)
            {
                m_copyAddressCallback(row);
            }
            break;
        case MENU_ID_COPY_REGISTERS:
            if (m_copyRegistersCallback)
            {
                m_copyRegistersCallback(row);
            }
            break;
        default:
            break;
        }
    }
}
