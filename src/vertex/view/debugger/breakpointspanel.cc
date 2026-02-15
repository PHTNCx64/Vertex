//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/breakpointspanel.hh>
#include <vertex/utility.hh>
#include <wx/menu.h>
#include <fmt/format.h>
#include <ranges>

namespace Vertex::View::Debugger
{
    BreakpointsPanel::BreakpointsPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void BreakpointsPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_breakpointList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxLC_REPORT | wxLC_SINGLE_SEL);
        m_breakpointList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_breakpointList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.columnId")), wxLIST_FORMAT_LEFT, FromDIP(40));
        m_breakpointList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.columnAddress")), wxLIST_FORMAT_LEFT, FromDIP(100));
        m_breakpointList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.columnType")), wxLIST_FORMAT_LEFT, FromDIP(70));
        m_breakpointList->InsertColumn(3, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.columnState")), wxLIST_FORMAT_LEFT, FromDIP(60));
        m_breakpointList->InsertColumn(4, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.columnHits")), wxLIST_FORMAT_LEFT, FromDIP(50));
    }

    void BreakpointsPanel::layout_controls()
    {
        m_mainSizer->Add(m_breakpointList, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        SetSizer(m_mainSizer);
    }

    void BreakpointsPanel::bind_events()
    {
        m_breakpointList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &BreakpointsPanel::on_item_activated, this);
        m_breakpointList->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &BreakpointsPanel::on_item_right_click, this);
    }

    void BreakpointsPanel::update_breakpoints(const std::vector<::Vertex::Debugger::Breakpoint>& breakpoints)
    {
        m_breakpoints = breakpoints;
        m_breakpointList->DeleteAllItems();

        for (const auto& [i, bp] : breakpoints | std::views::enumerate)
        {
            const long idx = m_breakpointList->InsertItem(static_cast<long>(i), std::to_string(bp.id));
            m_breakpointList->SetItem(idx, 1, fmt::format("0x{:X}", bp.address));

            wxString typeStr;
            switch (bp.type)
            {
                case ::Vertex::Debugger::BreakpointType::Software:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.typeSoftware"));
                    break;
                case ::Vertex::Debugger::BreakpointType::Hardware:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.typeHardware"));
                    break;
                case ::Vertex::Debugger::BreakpointType::Memory:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.typeMemory"));
                    break;
                case ::Vertex::Debugger::BreakpointType::Conditional:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.typeConditional"));
                    break;
            }
            m_breakpointList->SetItem(idx, 2, typeStr);

            wxString stateStr;
            switch (bp.state)
            {
                case ::Vertex::Debugger::BreakpointState::Enabled:
                    stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.stateEnabled"));
                    break;
                case ::Vertex::Debugger::BreakpointState::Disabled:
                    stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.stateDisabled"));
                    break;
                case ::Vertex::Debugger::BreakpointState::Pending:
                    stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.statePending"));
                    break;
                case ::Vertex::Debugger::BreakpointState::Error:
                    stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.stateError"));
                    break;
            }
            m_breakpointList->SetItem(idx, 3, stateStr);
            m_breakpointList->SetItem(idx, 4, std::to_string(bp.hitCount));
        }
    }

    void BreakpointsPanel::set_goto_callback(GotoBreakpointCallback callback)
    {
        m_gotoCallback = std::move(callback);
    }

    void BreakpointsPanel::set_remove_callback(RemoveBreakpointCallback callback)
    {
        m_removeCallback = std::move(callback);
    }

    void BreakpointsPanel::set_enable_callback(EnableBreakpointCallback callback)
    {
        m_enableCallback = std::move(callback);
    }

    void BreakpointsPanel::on_item_activated(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx >= 0 && static_cast<std::size_t>(idx) < m_breakpoints.size() && m_gotoCallback)
        {
            m_gotoCallback(m_breakpoints[idx].address);
        }
    }

    void BreakpointsPanel::on_item_right_click(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx < 0 || static_cast<std::size_t>(idx) >= m_breakpoints.size())
        {
            return;
        }

        const auto& bp = m_breakpoints[idx];
        wxMenu menu;
        menu.Append(1001, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.goToAddress")));
        menu.Append(1002, bp.state == ::Vertex::Debugger::BreakpointState::Enabled
            ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.disable"))
            : wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.enable")));
        menu.AppendSeparator();
        menu.Append(1003, wxString::FromUTF8(m_languageService.fetch_translation("debugger.breakpoints.remove")));

        const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPoint());
        switch (selection)
        {
            case 1001:
                if (m_gotoCallback)
                {
                    m_gotoCallback(bp.address);
                }
                break;
            case 1002:
                if (m_enableCallback)
                {
                    m_enableCallback(bp.id, bp.state != ::Vertex::Debugger::BreakpointState::Enabled);
                }
                break;
            case 1003:
                if (m_removeCallback)
                {
                    m_removeCallback(bp.id);
                }
                break;
            default:
                break;
        }
    }

}
