//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/watchpointspanel.hh>
#include <vertex/utility.hh>
#include <wx/menu.h>
#include <fmt/format.h>
#include <ranges>

namespace Vertex::View::Debugger
{
    WatchpointsPanel::WatchpointsPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void WatchpointsPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_watchpointList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxLC_REPORT | wxLC_SINGLE_SEL);
        m_watchpointList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_watchpointList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.columnId")), wxLIST_FORMAT_LEFT, FromDIP(40));
        m_watchpointList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.columnAddress")), wxLIST_FORMAT_LEFT, FromDIP(120));
        m_watchpointList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.columnSize")), wxLIST_FORMAT_LEFT, FromDIP(50));
        m_watchpointList->InsertColumn(3, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.columnType")), wxLIST_FORMAT_LEFT, FromDIP(80));
        m_watchpointList->InsertColumn(4, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.columnState")), wxLIST_FORMAT_LEFT, FromDIP(60));
        m_watchpointList->InsertColumn(5, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.columnHits")), wxLIST_FORMAT_LEFT, FromDIP(50));
    }

    void WatchpointsPanel::layout_controls()
    {
        m_mainSizer->Add(m_watchpointList, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        SetSizer(m_mainSizer);
    }

    void WatchpointsPanel::bind_events()
    {
        m_watchpointList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &WatchpointsPanel::on_item_activated, this);
        m_watchpointList->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &WatchpointsPanel::on_item_right_click, this);
    }

    void WatchpointsPanel::update_watchpoints(const std::vector<::Vertex::Debugger::Watchpoint>& watchpoints)
    {
        m_watchpoints = watchpoints;
        m_watchpointList->DeleteAllItems();

        for (const auto& [i, wp] : watchpoints | std::views::enumerate)
        {
            const long idx = m_watchpointList->InsertItem(static_cast<long>(i), std::to_string(wp.id));
            m_watchpointList->SetItem(idx, 1, fmt::format("{:X}", wp.address));
            m_watchpointList->SetItem(idx, 2, std::to_string(wp.size));

            wxString typeStr;
            switch (wp.type)
            {
                case ::Vertex::Debugger::WatchpointType::Read:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeRead"));
                    break;
                case ::Vertex::Debugger::WatchpointType::Write:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeWrite"));
                    break;
                case ::Vertex::Debugger::WatchpointType::ReadWrite:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeReadWrite"));
                    break;
                case ::Vertex::Debugger::WatchpointType::Execute:
                    typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeExecute"));
                    break;
            }
            m_watchpointList->SetItem(idx, 3, typeStr);

            const wxString stateStr = wp.enabled
                ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.stateEnabled"))
                : wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.stateDisabled"));
            m_watchpointList->SetItem(idx, 4, stateStr);

            m_watchpointList->SetItem(idx, 5, std::to_string(wp.hitCount));
        }
    }

    void WatchpointsPanel::add_watchpoint(const ::Vertex::Debugger::Watchpoint& watchpoint)
    {
        m_watchpoints.push_back(watchpoint);

        const long idx = m_watchpointList->InsertItem(static_cast<long>(m_watchpoints.size() - 1), std::to_string(watchpoint.id));
        m_watchpointList->SetItem(idx, 1, fmt::format("0x{:X}", watchpoint.address));
        m_watchpointList->SetItem(idx, 2, std::to_string(watchpoint.size));

        wxString typeStr;
        switch (watchpoint.type)
        {
            case ::Vertex::Debugger::WatchpointType::Read:
                typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeRead"));
                break;
            case ::Vertex::Debugger::WatchpointType::Write:
                typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeWrite"));
                break;
            case ::Vertex::Debugger::WatchpointType::ReadWrite:
                typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeReadWrite"));
                break;
            case ::Vertex::Debugger::WatchpointType::Execute:
                typeStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.typeExecute"));
                break;
        }
        m_watchpointList->SetItem(idx, 3, typeStr);

        const wxString stateStr = watchpoint.enabled
            ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.stateEnabled"))
            : wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.stateDisabled"));
        m_watchpointList->SetItem(idx, 4, stateStr);

        m_watchpointList->SetItem(idx, 5, std::to_string(watchpoint.hitCount));
    }

    void WatchpointsPanel::set_goto_callback(GotoWatchpointCallback callback)
    {
        m_gotoCallback = std::move(callback);
    }

    void WatchpointsPanel::set_goto_accessor_callback(GotoAccessorCallback callback)
    {
        m_gotoAccessorCallback = std::move(callback);
    }

    void WatchpointsPanel::set_remove_callback(RemoveWatchpointCallback callback)
    {
        m_removeCallback = std::move(callback);
    }

    void WatchpointsPanel::set_enable_callback(EnableWatchpointCallback callback)
    {
        m_enableCallback = std::move(callback);
    }

    void WatchpointsPanel::on_item_activated(wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx >= 0 && static_cast<std::size_t>(idx) < m_watchpoints.size() && m_gotoCallback)
        {
            m_gotoCallback(m_watchpoints[idx].address);
        }
    }

    void WatchpointsPanel::on_item_right_click(wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx < 0 || static_cast<std::size_t>(idx) >= m_watchpoints.size())
        {
            return;
        }

        const auto& wp = m_watchpoints[idx];
        wxMenu menu;

        menu.Append(1001, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.goto")));

        const bool hasAccessorAddress = wp.lastAccessorAddress != 0;
        wxMenuItem* gotoAccessorItem = menu.Append(1004, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.gotoAccessor")));
        gotoAccessorItem->Enable(hasAccessorAddress);

        wxMenuItem* inspectAccessorItem = menu.Append(1005, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.inspectAccessor")));
        inspectAccessorItem->Enable(hasAccessorAddress);

        menu.AppendSeparator();

        menu.Append(1002, wp.enabled
            ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.disable"))
            : wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.enable")));

        menu.AppendSeparator();
        menu.Append(1003, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watchpoints.remove")));

        const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPoint());
        switch (selection)
        {
            case 1001:
                if (m_gotoCallback)
                {
                    m_gotoCallback(wp.address);
                }
                break;
            case 1002:
                if (m_enableCallback)
                {
                    m_enableCallback(wp.id, !wp.enabled);
                }
                break;
            case 1003:
                if (m_removeCallback)
                {
                    m_removeCallback(wp.id);
                }
                break;
            case 1004:
                if (m_gotoAccessorCallback && wp.lastAccessorAddress != 0)
                {
                    m_gotoAccessorCallback(wp.lastAccessorAddress);
                }
                break;
            case 1005:
                break;
            default:
                break;
        }
    }

}
